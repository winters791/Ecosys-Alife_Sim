#include "sim.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "energy.hpp"
#include "mutation.hpp"

namespace luca {

Simulation::Simulation(const SimConfig& cfg) : cfg_(cfg), rng_(cfg.seed * 0x2545F4914F6CDD1Dull + 1) {
    world_.generate(cfg_);
    cell_demand_.assign(world_.num_cells(), 0.0f);
}

void Simulation::seed_population() {
    const Genome luca = make_luca();
    const Phenotype p = compile_phenotype(luca);
    const float ext = world_.extent();
    c_.reserve(static_cast<std::size_t>(cfg_.initial_population) * 4);
    for (int k = 0; k < cfg_.initial_population; ++k) {
        const float x = world_.clamp_coord(rng_.uniform(0.0f, ext));
        const float y = world_.clamp_coord(rng_.uniform(0.0f, ext));
        c_.add(x, y, cfg_.initial_energy, next_lineage_++, 0, luca, p);
    }
}

double Simulation::stored_energy() const {
    double s = 0.0;
    for (float e : c_.energy) s += e;
    return s;
}

// -----------------------------------------------------------------------------
// One tick, in two phases.
//
// Phase 1 (decide) only READS the current state and writes into scratch
// buffers: how much light each creature gets, what it pays, where it wants
// to move. No creature sees another's half-updated state.
//
// Phase 2 (apply) writes the results: energy changes, movement, deaths,
// births.
//
// This split is what will let phase 1 run in parallel across world tiles
// later without race conditions. For now it's single-threaded and the RNG is
// consumed in creature-index order, so runs are deterministic.
// -----------------------------------------------------------------------------
void Simulation::step() {
    const auto t0 = std::chrono::steady_clock::now();
    stats_ = TickStats{};
    stats_.tick = tick_;

    // God powers act first, so every creature sees the same world this tick.
    powers_.update(tick_, world_, chronicle_);

    const std::size_t n = c_.size();
    cell_of_.resize(n);
    demand_.resize(n);
    gain_.resize(n);
    cost_upkeep_.resize(n);
    cost_thermal_.resize(n);
    cost_move_.resize(n);
    new_x_.resize(n);
    new_y_.resize(n);

    // --- Phase 0: bin creatures into cells -----------------------------------
    for (std::size_t i = 0; i < n; ++i) cell_of_[i] = world_.cell_index(c_.x[i], c_.y[i]);
    grid_.build(cell_of_, world_.num_cells());

    // --- Phase 1a: light demand, summed per cell -----------------------------
    std::fill(cell_demand_.begin(), cell_demand_.end(), 0.0f);
    for (std::size_t i = 0; i < n; ++i) {
        const LocalEnv env = world_.env(cell_of_[i]);
        demand_[i] = std::max(0.0f, light_demand(c_.pheno[i], env));
        cell_demand_[cell_of_[i]] += demand_[i];
    }

    // --- Phase 1b: share light, compute costs, decide movement ---------------
    for (std::size_t i = 0; i < n; ++i) {
        const int cell = cell_of_[i];
        const LocalEnv env = world_.env(cell);
        const Phenotype& p = c_.pheno[i];

        // Finite light: if the cell is over-subscribed, everyone gets the same fraction.
        const float cd = cell_demand_[cell];
        const float share = (cd > env.light && cd > 0.0f) ? env.light / cd : 1.0f;
        gain_[i] = demand_[i] * share;

        cost_upkeep_[i]  = std::max(0.0f, upkeep_cost(p));
        cost_thermal_[i] = std::max(0.0f, thermal_cost(p, env));

        // Random wander, only for creatures that can move. (A brain will
        // replace this once creatures have sensors.)
        new_x_[i] = c_.x[i];
        new_y_[i] = c_.y[i];
        cost_move_[i] = 0.0f;
        if (p.max_speed > 0.0f) {
            const float angle = rng_.uniform(0.0f, 6.28318531f);
            const float dist  = rng_.uniform() * p.max_speed;
            const float nx = world_.clamp_coord(c_.x[i] + std::cos(angle) * dist);
            const float ny = world_.clamp_coord(c_.y[i] + std::sin(angle) * dist);
            const float dh = world_.height(world_.cell_index(nx, ny)) - env.height;
            new_x_[i] = nx;
            new_y_[i] = ny;
            cost_move_[i] = std::max(0.0f, movement_cost(p, dist, dh));
        }
    }

    // --- Phase 2: apply ------------------------------------------------------
    dead_.clear();
    births_.clear();
    for (std::size_t i = 0; i < n; ++i) {
        float e = c_.energy[i] + gain_[i];
        stats_.energy_in += gain_[i];

        // Pay costs; can never pay more than you have.
        const float want = cost_upkeep_[i] + cost_thermal_[i] + cost_move_[i];
        const float paid = std::min(want, e);
        e -= paid;
        stats_.energy_out += paid;
        if (want > 0.0f) {
            const double f = static_cast<double>(paid) / want;  // pro-rata if it couldn't pay in full
            stats_.out_upkeep   += cost_upkeep_[i] * f;
            stats_.out_thermal  += cost_thermal_[i] * f;
            stats_.out_movement += cost_move_[i] * f;
        }

        c_.x[i] = new_x_[i];
        c_.y[i] = new_y_[i];
        c_.age[i] += 1;

        if (e <= 0.0f) {
            c_.energy[i] = 0.0f;
            dead_.push_back(static_cast<uint32_t>(i));
            continue;
        }

        // Reproduction: divide when energy passes the threshold. Energy is
        // transferred, not created.
        const Phenotype& p = c_.pheno[i];
        if (e >= p.repro_threshold) {
            const float share = std::clamp(p.offspring_share, 0.0f, 1.0f);
            const float child_e = e * share;
            if (child_e > 0.0f) {
                e -= child_e;
                Genome g = mutate(c_.genome[i], rng_);
                Phenotype cp = compile_phenotype(g);
                const float ang = rng_.uniform(0.0f, 6.28318531f);
                const float r = rng_.uniform() * cfg_.offspring_spread;
                births_.push_back(Birth{
                    world_.clamp_coord(c_.x[i] + std::cos(ang) * r),
                    world_.clamp_coord(c_.y[i] + std::sin(ang) * r),
                    child_e, c_.lineage[i], c_.generation[i] + 1,
                    std::move(g), cp});
            }
        }
        c_.energy[i] = e;
    }

    // Deaths first (descending, so swap-remove never moves a dead creature),
    // then births appended at the end.
    for (auto it = dead_.rbegin(); it != dead_.rend(); ++it) c_.remove_swap(*it);
    for (Birth& b : births_)
        c_.add(b.x, b.y, b.energy, b.lineage, b.generation, std::move(b.genome), b.pheno);

    stats_.deaths = dead_.size();
    stats_.births = births_.size();
    ++tick_;
    if (tick_ % 50 == 0 || c_.size() == 0) detect_events();

    const auto t1 = std::chrono::steady_clock::now();
    stats_.tick_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
}

// -----------------------------------------------------------------------------
// Event detectors (population level). These read the sim and write chronicle
// entries; they never change the sim. Species-level detectors (speciation,
// first predator, new trophic level) will join them here.
// -----------------------------------------------------------------------------
void Simulation::detect_events() {
    const std::size_t pop = c_.size();

    if (pop == 0) {
        if (!extinct_logged_) chronicle_.add(tick_, "The last living thing dies. The world is silent.");
        extinct_logged_ = true;
        return;
    }

    // First time the world holds 1,000 / 10,000 / 100,000 ... creatures.
    while (pop >= next_milestone_) {
        chronicle_.add(tick_, "Life is now " + with_commas(next_milestone_) + " strong.");
        next_milestone_ *= 10;
    }

    // Mass die-off: population halves from its peak. It is logged once, then
    // ends either with a recovery (back to 90% of the old peak) or, if the
    // world has simply become poorer, quietly after a while.
    if (crash_from_ == 0) {
        peak_pop_ = std::max(peak_pop_, pop);
        if (peak_pop_ >= 100 && pop * 2 < peak_pop_) {
            chronicle_.add(tick_, "A great dying: only " + with_commas(pop) + " remain of " +
                                      with_commas(peak_pop_) + ".");
            crash_from_ = peak_pop_;
            crash_tick_ = tick_;
        }
    } else if (pop * 10 >= crash_from_ * 9) {
        chronicle_.add(tick_, "Life has recovered: " + with_commas(pop) + " creatures.");
        crash_from_ = 0;
        peak_pop_ = pop;
    } else if (tick_ - crash_tick_ >= 20000) {
        crash_from_ = 0;  // the new normal; watch for the next die-off from here
        peak_pop_ = pop;
    }
}

}  // namespace luca
