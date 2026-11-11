#include "sim.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "energy.hpp"
#include "mutation.hpp"

namespace luca {

namespace {

constexpr uint64_t FIELD_INTERVAL = 8;     // ticks between corpse weathering passes
constexpr uint64_t WASH_INTERVAL  = 64;    // ticks between passes of matter washing downhill
constexpr float    MAX_REACH      = 1.5f;  // contact looks at the 3x3 cells around a body, no further
constexpr float    TWO_PI         = 6.28318531f;
// How fast the wandering urge turns: sd of its angle change per tick, in
// radians. 0.5 keeps a heading for roughly 10 ticks: an average turn of about
// 23 degrees a tick, a wobbly path rather than a straight one. Slower turning
// (0.15) let wanderers sweep the map and overgraze it; 1.0 brought back most
// of the jitter. Picked from a three-seed sweep, 2026-11-11.
// Wandering bends by distance, not by time: over each world unit travelled
// the wander angle drifts with variance 2 / WANDER_PATH, so a wanderer's path
// stays fairly straight for about WANDER_PATH world units whatever its speed.
// (It used to turn ~0.5 rad every tick however far it went; a body creeping
// 0.01 a tick then spun in place, its loops smaller than a pixel.)
constexpr float    WANDER_PATH    = 1.0f;
constexpr int      SOCIAL_CELLS   = 2;     // the danger / kin sense looks this many cells out at most
constexpr float    SOCIAL_FAST    = 0.25f; // a body covering this much a tick counts as fully "moving"

}  // namespace

Simulation::Simulation(const SimConfig& cfg) : cfg_(cfg), rng_(cfg.seed * 0x2545F4914F6CDD1Dull + 1) {
    world_.generate(cfg_);
    const std::size_t cells = static_cast<std::size_t>(world_.num_cells());
    cell_demand_.assign(cells, 0.0f);
    cell_corpse_demand_.assign(cells, 0.0f);
    cell_waste_demand_.assign(cells, 0.0f);
    biomass_.assign(cells, 0.0f);

    // Starting soil: patches of rich ground and stretches of poor ground,
    // scaled so the average is exactly nutrient_mean.
    soil_.resize(cells);
    double sum = 0.0;
    for (std::size_t c = 0; c < cells; ++c) {
        const float k = std::clamp((world_.patch_noise(static_cast<int>(c), 77u) - 0.3f) / 0.4f, 0.0f, 1.0f);
        soil_.nutrient[c] = k * std::sqrt(k);
        sum += soil_.nutrient[c];
    }
    const float mean_raw = sum > 0.0 ? static_cast<float>(sum / static_cast<double>(cells)) : 1.0f;
    const float patchy = std::clamp(cfg_.nutrient_patchiness, 0.0f, 1.0f);
    for (float& v : soil_.nutrient)
        v = std::max(0.0f, cfg_.nutrient_mean) * ((1.0f - patchy) + patchy * v / mean_raw);
}

void Simulation::seed_population() {
    const Genome luca = make_luca();
    const Phenotype p = compile_phenotype(luca);
    const float ext = world_.extent();
    c_.reserve(static_cast<std::size_t>(cfg_.initial_population) * 4);
    for (int k = 0; k < cfg_.initial_population; ++k) {
        const float x = world_.clamp_coord(rng_.uniform(0.0f, ext));
        const float y = world_.clamp_coord(rng_.uniform(0.0f, ext));
        // The first LUCAs are identical: one species.
        const uint32_t sp = species_.all().empty() ? species_.found_root(luca, tick_, "Luca")
                                                   : species_.classify_newcomer(luca, tick_);
        admit(x, y, std::min(cfg_.initial_energy, p.energy_capacity), next_lineage_++, sp, 0, luca, p);
    }
}

void Simulation::admit(float x, float y, float energy, uint32_t lineage, uint32_t species, uint32_t generation,
                       Genome genome, const Phenotype& pheno, float body_temp) {
    if (body_temp < -273.0f) body_temp = felt_temperature(pheno, world_.env(world_.cell_index(x, y)));
    c_.add(x, y, energy, body_temp, next_id_++, lineage, species, generation, std::move(genome), pheno);
    if (!species_.born(species)) return;

    // The species has taken hold: enough alive at once to be more than a freak.
    const Species& s = species_.get(species);
    if (s.parent == NO_SPECIES) {
        if (species != 0) chronicle_.add(tick_, "Newcomers take hold: " + s.name + " is established.");
        return;
    }
    const Species& from = species_.get(s.parent);
    const std::string how = describe_difference(s.representative, from.representative);
    chronicle_.add(tick_, "A new species: " + s.name + " branches from " + from.name +
                              (how.empty() ? "." : ". It " + how + "."));
}

uint64_t Simulation::add_creature(float x, float y, float energy, const Genome& genome) {
    Genome g = genome;
    clamp_genome(g);
    const Phenotype p = compile_phenotype(g);
    const uint64_t id = next_id_;
    const uint32_t sp = species_.classify_newcomer(g, tick_);
    admit(world_.clamp_coord(x), world_.clamp_coord(y), std::clamp(energy, 0.0f, p.energy_capacity),
          next_lineage_++, sp, 0, std::move(g), p);
    return id;
}

double Simulation::stored_energy() const {
    double s = 0.0;
    for (float e : c_.energy) s += e;
    for (const Phenotype& p : c_.pheno) s += p.body_energy;
    for (float e : soil_.corpse_energy) s += e;
    for (float e : soil_.waste) s += e;
    return s;
}

double Simulation::total_matter() const {
    double s = 0.0;
    for (const Phenotype& p : c_.pheno) s += p.mass;
    for (float m : soil_.nutrient) s += m;
    for (float m : soil_.corpse_matter) s += m;
    return s;
}

// -----------------------------------------------------------------------------
// The brain: what the body feels -> its evolved network -> a direction and a
// throttle. See BrainInput / BrainOutput in genome.hpp. Reads only the current
// state of the world; writes this creature's scratch slots and its own memory.
// -----------------------------------------------------------------------------
void Simulation::decide_movement(std::size_t i, int cell, const LocalEnv& env) {
    const Phenotype& p = c_.pheno[i];
    const float hunger = 1.0f - std::clamp(c_.energy[i] / p.repro_threshold, 0.0f, 1.0f);
    const float crowd = std::min(biomass_[cell] * 0.25f, 1.0f);
    // Comfort: positive when too cold, negative when too hot.
    const float want = std::clamp((p.thermal_optimum - c_.body_temp[i]) / std::max(p.thermal_tolerance, 1.0f), -1.0f, 1.0f);

    float in[NUM_INPUTS];
    in[IN_BIAS]   = 1.0f;
    in[IN_HUNGER] = hunger;
    in[IN_LIGHT]  = env.light;
    in[IN_CROWD]  = crowd;
    in[IN_COLD]   = want;
    in[IN_CORPSE] = std::min(soil_.corpse_energy[cell] * (1.0f / 250.0f), 1.0f);   // about one body's worth = 1
    in[IN_MOVED]  = p.max_speed > 0.0f ? std::min(c_.moved[i] / p.max_speed, 1.0f) : 0.0f;
    float w[NUM_OUTPUTS];
    think(c_.brain[i], c_.brain_state[i], in, w);

    float dx = 0.0f, dy = 0.0f;
    if (p.sense_cells > 0) {
        const int n = world_.size();
        const int cx = cell % n, cy = cell / n;
        const int xm = std::max(cx - p.sense_cells, 0), xp = std::min(cx + p.sense_cells, n - 1);
        const int ym = std::max(cy - p.sense_cells, 0), yp = std::min(cy + p.sense_cells, n - 1);
        const int left = cy * n + xm, right = cy * n + xp, up = ym * n + cx, down = yp * n + cx;

        // Each sense is the slope of one field: a direction, with a strength
        // that saturates at 1 (`soft` is the difference that counts as "clear").
        auto lean = [&](const std::vector<float>& f, float soft, float weight) {
            const float gx = f[right] - f[left], gy = f[down] - f[up];
            const float len = std::sqrt(gx * gx + gy * gy);
            if (len <= 0.0f) return;
            const float k = weight / (len + soft);
            dx += gx * k;
            dy += gy * k;
        };
        lean(world_.lights(),       0.02f, w[OUT_LIGHT]);
        lean(biomass_,              0.5f,  w[OUT_BIOMASS]);
        lean(soil_.corpse_energy,   1.0f,  w[OUT_CORPSE]);
        lean(soil_.nutrient,        0.2f,  w[OUT_NUTRIENT]);
        // Comfort: uphill in temperature when too cold, downhill when too hot.
        lean(world_.temperatures(), 0.5f,  w[OUT_COMFORT] * want);

        // The danger / kin sense: the bodies close by, each pulling or pushing
        // by what it is to this creature (see BrainOutput). It looks at most
        // SOCIAL_CELLS cells out, through the spatial grid, so the work is
        // bounded by how crowded the neighbourhood is, never by the world.
        if (w[OUT_KIN] != 0.0f || w[OUT_BIGGER] != 0.0f || w[OUT_MOVING] != 0.0f) {
            const int reach = std::min(p.sense_cells, SOCIAL_CELLS);
            const float px = c_.x[i], py = c_.y[i];
            const uint32_t mine = c_.species[i];
            float sx = 0.0f, sy = 0.0f;
            for (int yy = std::max(cy - reach, 0); yy <= std::min(cy + reach, n - 1); ++yy) {
                const uint32_t stop = grid_.end(yy * n + std::min(cx + reach, n - 1));
                for (uint32_t slot = grid_.begin(yy * n + std::max(cx - reach, 0)); slot < stop; ++slot) {
                    const uint32_t j = grid_.creature_at(slot);
                    if (j == i) continue;
                    const float ox = c_.x[j] - px, oy = c_.y[j] - py;
                    const float dist = std::sqrt(ox * ox + oy * oy);
                    if (dist < 1e-4f) continue;
                    const Phenotype& q = c_.pheno[j];
                    const float bigger = std::clamp(q.radius / p.radius - 1.0f, -1.0f, 1.0f);
                    const float moving = std::min(c_.moved[j] * (1.0f / SOCIAL_FAST), 1.0f);
                    const float score = (c_.species[j] == mine ? w[OUT_KIN] : 0.0f) + w[OUT_BIGGER] * bigger + w[OUT_MOVING] * moving;
                    // Nearer and heavier bodies count for more.
                    const float k = score * q.mass / (dist * (1.0f + dist));
                    sx += ox * k;
                    sy += oy * k;
                }
            }
            const float slen = std::sqrt(sx * sx + sy * sy);
            if (slen > 0.0f) {
                dx += sx / (slen + 0.5f);
                dy += sy / (slen + 0.5f);
            }
        }
    }

    // Wandering is a direction that drifts, not a fresh one every tick: a
    // brand-new random angle each tick made wanderers jitter on the spot,
    // facing a new way every frame and going nowhere. The angle is the
    // creature's own state, so writing it here is like its brain's memory.
    const float throttle = std::clamp(0.5f + w[OUT_THROTTLE], 0.0f, 1.0f);
    const float dist = throttle * p.max_speed;
    float& angle = c_.wander[i];
    angle += std::sqrt(2.0f * dist / WANDER_PATH) * rng_.normal();
    if (angle > TWO_PI) angle -= TWO_PI; else if (angle < 0.0f) angle += TWO_PI;
    dx += w[OUT_WANDER] * std::cos(angle) + w[OUT_INERTIA] * c_.hx[i];
    dy += w[OUT_WANDER] * std::sin(angle) + w[OUT_INERTIA] * c_.hy[i];

    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-5f || throttle <= 0.0f) return;  // stays put

    const float ux = dx / len, uy = dy / len;
    const float nx = world_.clamp_coord(c_.x[i] + ux * dist);
    const float ny = world_.clamp_coord(c_.y[i] + uy * dist);
    const float dh = world_.height(world_.cell_index(nx, ny)) - env.height;
    new_x_[i] = nx;
    new_y_[i] = ny;
    new_hx_[i] = ux;
    new_hy_[i] = uy;
    cost_move_[i] = std::max(0.0f, movement_cost(p, dist, dh));
}

// -----------------------------------------------------------------------------
// One tick, in two phases.
//
// Phase 1 (decide) only READS the current state and writes into scratch
// buffers: how much light each creature gets, what flows between bodies that
// touch, what each pays, where it wants to move. No creature sees another's
// half-updated state.
//
// Phase 2 (apply) writes the results: energy changes, movement, deaths (a
// body becomes a corpse in the soil), births (a body is built from the soil).
//
// This split is what will let phase 1 run in parallel across world tiles
// later without race conditions. For now it's single-threaded and the RNG is
// consumed in creature-index order, so runs are deterministic.
// -----------------------------------------------------------------------------
void Simulation::step() {
    const auto t0 = std::chrono::steady_clock::now();
    stats_ = TickStats{};
    stats_.tick = tick_;

    // God powers act first, then the sun moves, so every creature sees the
    // same world this tick.
    powers_.update(tick_, world_, chronicle_);
    world_.set_time(tick_);

    const std::size_t n = c_.size();
    cell_of_.resize(n);
    demand_.resize(n);
    gain_.resize(n);
    cost_upkeep_.resize(n);
    cost_thermal_.resize(n);
    cost_move_.resize(n);
    new_x_.resize(n);
    new_y_.resize(n);
    new_hx_.resize(n);
    new_hy_.resize(n);
    corpse_want_.resize(n);
    corpse_gain_.resize(n);
    waste_want_.resize(n);
    waste_gain_.resize(n);
    drain_want_.assign(n, 0.0f);
    drained_.assign(n, 0.0f);
    intake_.assign(n, 0.0f);
    poisoned_.assign(n, 0.0f);

    // --- Phase 0: bin creatures into cells; living mass per cell --------------
    std::fill(biomass_.begin(), biomass_.end(), 0.0f);
    for (std::size_t i = 0; i < n; ++i) {
        cell_of_[i] = world_.cell_index(c_.x[i], c_.y[i]);
        biomass_[cell_of_[i]] += c_.pheno[i].mass;
    }
    grid_.build(cell_of_, world_.num_cells());

    // --- Phase 1a: what everyone asks for, summed per cell --------------------
    std::fill(cell_demand_.begin(), cell_demand_.end(), 0.0f);
    std::fill(cell_corpse_demand_.begin(), cell_corpse_demand_.end(), 0.0f);
    std::fill(cell_waste_demand_.begin(), cell_waste_demand_.end(), 0.0f);
    const bool dark = world_.daylight() <= 0.0f;
    for (std::size_t i = 0; i < n; ++i) {
        const Phenotype& p = c_.pheno[i];
        const int cell = cell_of_[i];
        // A body on the move catches less light (it moved last tick; see energy.cpp).
        demand_[i] = dark ? 0.0f : std::max(0.0f, light_demand(p, world_.env(cell)) * light_while_moving(c_.moved[i]) * (1.0f - c_.sleep[i]));
        gain_[i] = demand_[i];
        cell_demand_[cell] += demand_[i];
        corpse_want_[i] = p.absorb > 0.0f ? std::max(0.0f, corpse_demand(p)) * (1.0f - c_.sleep[i]) : 0.0f;
        cell_corpse_demand_[cell] += corpse_want_[i];
        waste_want_[i] = p.detox > 0.0f ? std::max(0.0f, waste_demand(p)) * (1.0f - c_.sleep[i]) : 0.0f;
        cell_waste_demand_[cell] += waste_want_[i];
    }

    if (!dark) share_light_by_canopy();

    // --- Phase 1b: contact. Only bodies that can pull look at their neighbours
    // Positions, reach and membrane are copied into cell order first, so the
    // scan of the 3x3 cells around a body is a walk along three short strips.
    contacts_.clear();
    const int wn = world_.size();
    bool any_pull = false;
    for (std::size_t i = 0; i < n && !any_pull; ++i) any_pull = c_.pheno[i].absorb > 0.0f;
    if (any_pull) {
        near_.resize(n);
        for (uint32_t slot = 0; slot < n; ++slot) {
            const uint32_t j = grid_.creature_at(slot);
            near_[slot] = {c_.x[j], c_.y[j], c_.pheno[j].reach, j};
        }
        for (std::size_t i = 0; i < n; ++i) {
            const Phenotype& p = c_.pheno[i];
            if (p.absorb <= 0.0f) continue;
            const int cx = cell_of_[i] % wn, cy = cell_of_[i] / wn;
            const int x0 = std::max(cx - 1, 0), x1 = std::min(cx + 1, wn - 1);
            const float px = c_.x[i], py = c_.y[i];
            for (int yy = std::max(cy - 1, 0); yy <= std::min(cy + 1, wn - 1); ++yy) {
                // Cells in a row are neighbours in slot order too: one strip per row.
                const uint32_t stop = grid_.end(yy * wn + x1);
                for (uint32_t slot = grid_.begin(yy * wn + x0); slot < stop; ++slot) {
                    const Near& q = near_[slot];
                    const float ddx = q.x - px, ddy = q.y - py;
                    const float reach = std::min(p.reach + q.reach, MAX_REACH);
                    if (ddx * ddx + ddy * ddy > reach * reach || q.who == i) continue;
                    const float flow = contact_flow(p, c_.pheno[q.who]) * (1.0f - c_.sleep[i]);
                    if (flow <= 0.0f) continue;
                    contacts_.push_back({static_cast<uint32_t>(i), q.who, flow});
                    drain_want_[q.who] += flow;
                }
            }
        }
    }

    // --- Phase 1c: shares, costs, movement ------------------------------------
    for (std::size_t i = 0; i < n; ++i) {
        const int cell = cell_of_[i];
        const LocalEnv env = world_.env(cell);
        const Phenotype& p = c_.pheno[i];

        // Light was shared out by the canopy rule above. The dead are shared evenly.
        corpse_gain_[i] = 0.0f;
        if (corpse_want_[i] > 0.0f) {
            const float have = soil_.corpse_energy[cell], want = cell_corpse_demand_[cell];
            corpse_gain_[i] = corpse_want_[i] * (want > have ? have / want : 1.0f);
        }
        // Waste is shared out the same way.
        waste_gain_[i] = 0.0f;
        if (waste_want_[i] > 0.0f) {
            const float have = soil_.waste[cell], want = cell_waste_demand_[cell];
            waste_gain_[i] = waste_want_[i] * (want > have ? have / want : 1.0f);
        }

        // A dormant body burns less, whatever the weather.
        const float burning = cost_while_asleep(c_.sleep[i]);
        cost_upkeep_[i]  = std::max(0.0f, upkeep_cost(p, c_.age[i]) * burning);
        cost_thermal_[i] = std::max(0.0f, thermal_cost(p, c_.body_temp[i]) * burning);

        new_x_[i] = c_.x[i];
        new_y_[i] = c_.y[i];
        new_hx_[i] = c_.hx[i];
        new_hy_[i] = c_.hy[i];
        cost_move_[i] = 0.0f;
        if (p.has_brain && p.max_speed > 0.0f && c_.sleep[i] < AWAKE) decide_movement(i, cell, env);
    }

    // --- Phase 2a: flows between touching bodies ------------------------------
    // A body can't give more than it holds: if several pull on it at once,
    // each gets the same fraction of what it asked for.
    for (const Contact& k : contacts_) {
        const float have = c_.energy[k.giver], want = drain_want_[k.giver];
        const float amount = want > have ? k.flow * (have / want) : k.flow;
        drained_[k.giver] += amount;
        intake_[k.taker] += amount;
        // A poisonous body costs its eater energy for every unit taken.
        poisoned_[k.taker] += amount * c_.pheno[k.giver].toxicity;

        // Gene transfer: now and then, something of the body being drained is
        // taken up as a blueprint. It goes into the taker's genome only: its
        // own body is already built and does not change, but its children
        // are built from the new plan (and pay for the extra part).
        if (amount > 0.0f && cfg_.gene_transfer_chance > 0.0f && rng_.chance(cfg_.gene_transfer_chance)) {
            float before[static_cast<int>(PartType::Count)] = {};
            for (const Part& part : c_.genome[k.taker].parts) before[static_cast<int>(part.type)] += part.size;
            PartType copied = PartType::Photo;
            if (transfer_part(c_.genome[k.giver], c_.genome[k.taker], rng_, copied)) {
                const bool cross = c_.species[k.taker] != c_.species[k.giver];
                const bool novel = before[static_cast<int>(copied)] <= 0.0f;
                ++stats_.gene_transfers;
                stats_.gene_transfers_cross += cross ? 1 : 0;
                stats_.gene_transfers_novel += novel ? 1 : 0;
                if (cross && novel && !transfer_logged_) {
                    transfer_logged_ = true;
                    chronicle_.add(tick_, "A borrowed blueprint: " + species_.get(c_.species[k.taker]).name + " takes up the plan for " +
                                              part_noun(copied) + " from " + species_.get(c_.species[k.giver]).name + ", which it feeds on.");
                }
            }
        }
    }

    // --- Phase 2b: apply ------------------------------------------------------
    dead_.clear();
    births_.clear();
    for (std::size_t i = 0; i < n; ++i) {
        const Phenotype& p = c_.pheno[i];
        const int cell = cell_of_[i];
        float e = std::max(0.0f, c_.energy[i] - drained_[i]);
        const LocalEnv env_here = world_.env(cell);

        // In: light, the living, the dead.
        const float from_living = intake_[i] * p.efficiency;
        float lost = intake_[i] - from_living;
        float from_dead = 0.0f;
        if (corpse_gain_[i] > 0.0f) {
            float& ce = soil_.corpse_energy[cell];
            const float take = std::min(corpse_gain_[i], ce);
            if (take > 0.0f) {
                // Breaking a corpse down frees its matter in proportion.
                const float freed = soil_.corpse_matter[cell] * (take / ce);
                soil_.corpse_matter[cell] -= freed;
                soil_.nutrient[cell] += freed;
                ce -= take;
                from_dead = take * p.efficiency;
                lost += take - from_dead;
            }
        }
        // Waste: one look at the cell's waste does all three things. A body
        // with detox takes some up first; then it pays for living in what is
        // left; then it adds its own. (Read here rather than in phase 1 to
        // touch the cell once: the field is a tick's excretion fresher for
        // later bodies in the same cell, which is far below its size.)
        float& w = soil_.waste[cell];
        float from_waste = 0.0f;
        if (waste_gain_[i] > 0.0f) {
            const float take = std::min(waste_gain_[i], w);
            w -= take;
            from_waste = take * p.efficiency;
            lost += take - from_waste;
        }
        e += gain_[i] + from_living + from_dead + from_waste;
        stats_.eaten_waste += from_waste;
        c_.fed_waste[i] += from_waste;
        stats_.energy_in += gain_[i];
        stats_.eaten_living += from_living;
        stats_.eaten_dead += from_dead;
        stats_.out_transfer += lost;
        stats_.energy_out += lost;
        c_.fed_light[i] += gain_[i];
        c_.fed_living[i] += from_living;
        c_.fed_dead[i] += from_dead;

        // Poison: paid out of whatever it now holds; it can cost more than the meal gave.
        if (poisoned_[i] > 0.0f) {
            const float burned = std::min(poisoned_[i], e);
            e -= burned;
            stats_.out_poison += burned;
            stats_.energy_out += burned;
        }

        // A full body can't hold more; the surplus is lost as heat.
        if (e > p.energy_capacity) {
            const float waste = e - p.energy_capacity;
            e = p.energy_capacity;
            stats_.out_waste += waste;
            stats_.energy_out += waste;
        }

        // Pay costs; can never pay more than you have.
        // Living in waste costs everyone in the cell, asleep or awake.
        const float foul = std::max(0.0f, fouling_cost(p, w));
        const float want = cost_upkeep_[i] + cost_thermal_[i] + cost_move_[i] + foul;
        const float paid = std::min(want, e);
        e -= paid;
        if (want > 0.0f) {
            const float f = paid / want;  // pro-rata if it couldn't pay in full
            // Part of its upkeep leaves the body as waste, still holding energy;
            // the rest, and every other cost, is lost as heat.
            const float upkeep = cost_upkeep_[i] * f;
            const float excreted = WASTE_SHARE * upkeep;
            w += excreted;
            stats_.excreted     += excreted;
            stats_.energy_out   += paid - excreted;
            stats_.out_upkeep   += upkeep - excreted;
            stats_.out_thermal  += cost_thermal_[i] * f;
            stats_.out_movement += cost_move_[i] * f;
            stats_.out_fouling  += foul * f;
        }

        c_.moved[i] = std::hypot(new_x_[i] - c_.x[i], new_y_[i] - c_.y[i]);
        c_.x[i] = new_x_[i];
        c_.y[i] = new_y_[i];
        c_.hx[i] = new_hx_[i];
        c_.hy[i] = new_hy_[i];
        // A deep sleeper ages at an eighth of the usual pace; then sleep deepens
        // or lifts for the next tick.
        if (c_.sleep[i] < DEEP_SLEEP || (tick_ & 7u) == 0) c_.age[i] += 1;
        // Its body warms or cools towards the air where it now stands.
        c_.body_temp[i] = next_body_temp(p, env_here, c_.body_temp[i]);
        c_.sleep[i] = next_sleep(p, env_here, c_.body_temp[i], c_.sleep[i]);

        if (e <= 0.0f) {
            // Death: the body stays where it fell, as matter and as food.
            c_.energy[i] = 0.0f;
            soil_.corpse_energy[cell] += p.body_energy;
            soil_.corpse_matter[cell] += p.mass;
            dead_.push_back(static_cast<uint32_t>(i));
            continue;
        }

        // Reproduction: divide when energy passes the threshold. The child's
        // body is built from soil matter and from the parent's energy; neither
        // is created. No free matter in the soil here = no child yet.
        // A body tries once every 8 ticks: when the soil is nearly bare a
        // failed try (mutate, compile, find the child too heavy) would
        // otherwise be repeated by every ready parent on every tick.
        if (e >= p.repro_threshold && (c_.age[i] & 7u) == 0 && c_.sleep[i] < AWAKE && soil_.nutrient[cell] >= p.mass * 0.75f) {
            Genome g = mutate(c_.genome[i], rng_);
            const Phenotype cp = compile_phenotype(g);
            // Dispersal is paid for: a child lands touching its parent, unless
            // the parent has the means to fling it further, which costs energy.
            const float ang = rng_.uniform(0.0f, TWO_PI);
            const float flung = rng_.uniform() * p.throw_range;
            const float thrown = flung > 0.0f ? std::max(0.0f, throw_cost(cp.mass, flung)) : 0.0f;
            const float spare = e - cp.body_energy - thrown;
            if (spare > 0.0f && soil_.nutrient[cell] >= cp.mass) {
                const float child_e = std::min(spare * p.offspring_share, cp.energy_capacity);
                e -= cp.body_energy + child_e + thrown;
                stats_.out_movement += thrown;
                stats_.energy_out += thrown;
                soil_.nutrient[cell] -= cp.mass;
                float bx, by;
                land_child(c_.x[i], c_.y[i], std::cos(ang), std::sin(ang), p.reach + cp.reach + flung, bx, by);
                births_.push_back(Birth{
                    bx, by,
                    child_e, c_.body_temp[i], c_.lineage[i], species_.classify(c_.species[i], g, tick_), c_.generation[i] + 1,
                    std::move(g), cp});
            }
        }
        c_.energy[i] = e;
    }

    // Deaths first (descending, so swap-remove never moves a dead creature),
    // then births appended at the end.
    for (auto it = dead_.rbegin(); it != dead_.rend(); ++it) {
        const uint32_t sp = c_.species[*it];
        if (species_.died(sp, tick_) && species_.get(sp).established) {
            const Species& s = species_.get(sp);
            chronicle_.add(tick_, s.name + " is extinct, after " + with_commas(tick_ - s.born_tick) + " ticks.");
        }
        c_.remove_swap(*it);
    }
    for (Birth& b : births_)
        admit(b.x, b.y, b.energy, b.lineage, b.species, b.generation, std::move(b.genome), b.pheno, b.body_temp);

    stats_.deaths = dead_.size();
    stats_.births = births_.size();
    ++tick_;
    if (tick_ % FIELD_INTERVAL == 0) { weather_corpses(); spread_waste(); }
    if (tick_ % WASH_INTERVAL == 0) wash_soil();
    if (tick_ % 50 == 0 || c_.size() == 0) {
        take_census();
        detect_events();
    }

    const auto t1 = std::chrono::steady_clock::now();
    stats_.tick_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
}

// The canopy: in a cell where bodies ask for more light than falls on it, the
// biggest takes its fill first and the smaller get what is left. Bodies of
// exactly the same size share alike. Cells with light to spare are untouched
// (gain_ already holds each body's full demand).
void Simulation::share_light_by_canopy() {
    const std::size_t n = c_.size();
    for (std::size_t i = 0; i < n; ++i) {
        const int cell = cell_of_[i];
        float left = world_.light(cell);
        if (cell_demand_[cell] <= left) continue;
        // Handle each crowded cell once: when we reach its first occupant.
        if (grid_.creature_at(grid_.begin(cell)) != i) continue;

        canopy_.clear();
        for (uint32_t j : grid_.in_cell(cell)) canopy_.push_back({c_.pheno[j].radius, j});
        std::sort(canopy_.begin(), canopy_.end(), [](const Tall& a, const Tall& b) {
            return a.radius != b.radius ? a.radius > b.radius : a.who < b.who;
        });
        for (std::size_t a = 0; a < canopy_.size();) {
            std::size_t b = a;
            float want = 0.0f;
            for (; b < canopy_.size() && canopy_[b].radius == canopy_[a].radius; ++b) want += demand_[canopy_[b].who];
            const float share = want > left ? left / want : 1.0f;
            for (std::size_t k = a; k < b; ++k) gain_[canopy_[k].who] = demand_[canopy_[k].who] * share;
            left = std::max(0.0f, left - want);
            a = b;
        }
    }
}

void Simulation::land_child(float x, float y, float ux, float uy, float dist, float& out_x, float& out_y) const {
    out_x = x;
    out_y = y;
    float h = world_.height(world_.cell_index(x, y));
    const int steps = std::max(1, static_cast<int>(std::ceil(dist / (0.5f * world_.cell_size()))));
    for (int s = 1; s <= steps; ++s) {
        const float t = dist * static_cast<float>(s) / static_cast<float>(steps);
        const float px = world_.clamp_coord(x + ux * t), py = world_.clamp_coord(y + uy * t);
        const float ph = world_.height(world_.cell_index(px, py));
        if (std::abs(ph - h) > cfg_.birth_max_step) return;  // a wall or a cliff: it comes down here
        out_x = px;
        out_y = py;
        h = ph;
    }
}

// Corpses nobody eats still fall apart, slowly: energy to heat, matter to soil.
void Simulation::weather_corpses() {
    const std::size_t cells = soil_.corpse_matter.size();
    for (std::size_t c = 0; c < cells; ++c) {
        float& cm = soil_.corpse_matter[c];
        float& ce = soil_.corpse_energy[c];
        if (cm <= 0.0f && ce <= 0.0f) continue;
        const float rate = (cm < 1e-3f && ce < 1e-3f) ? 1.0f : CORPSE_WEATHERING;  // sweep up crumbs
        const float de = ce * rate, dm = cm * rate;
        ce -= de;
        cm -= dm;
        soil_.nutrient[c] += dm;
        stats_.out_decay += de;
        stats_.energy_out += de;
    }
}

// Waste drifts: each cell hands WASTE_SPREAD of its waste to its four
// neighbours (a quarter each; what would fall off the edge stays put), then
// WASTE_DECAY of what is left breaks down to heat. All taking is worked out
// before any giving, so cell order doesn't matter.
void Simulation::spread_waste() {
    const int n = world_.size();
    const std::size_t cells = soil_.waste.size();
    waste_flow_.assign(cells, 0.0f);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) {
            const int c = y * n + x;
            float& w = soil_.waste[c];
            if (w <= 0.0f) continue;
            if (w < 1e-4f) {  // sweep up crumbs
                stats_.out_waste_decay += w;
                stats_.energy_out += w;
                w = 0.0f;
                continue;
            }
            const float part = w * (WASTE_SPREAD * 0.25f);
            const int nb[4] = {x > 0 ? c - 1 : -1, x + 1 < n ? c + 1 : -1, y > 0 ? c - n : -1, y + 1 < n ? c + n : -1};
            for (int k : nb)
                if (k >= 0) { waste_flow_[k] += part; w -= part; }
        }
    for (std::size_t c = 0; c < cells; ++c) {
        float& w = soil_.waste[c];
        w += waste_flow_[c];
        const float gone = w * WASTE_DECAY;
        w -= gone;
        stats_.out_waste_decay += gone;
        stats_.energy_out += gone;
    }
}

// Rain: a little free matter moves from each cell to its lowest neighbour,
// more on a steep slope, none on the flat. Ridges grow poor and hollows rich.
// Every amount taken from one cell is given to another, so matter is conserved;
// all the taking is worked out before any giving, so cell order doesn't matter.
void Simulation::wash_soil() {
    if (cfg_.wash_rate <= 0.0f) return;
    const std::size_t cells = soil_.nutrient.size();
    wash_.assign(cells, 0.0f);
    for (std::size_t c = 0; c < cells; ++c) {
        const int low = world_.downhill(static_cast<int>(c));
        if (low < 0 || soil_.nutrient[c] <= 0.0f) continue;
        const float moved = soil_.nutrient[c] * std::min(cfg_.wash_rate * world_.steepness(static_cast<int>(c)), 1.0f);
        soil_.nutrient[c] -= moved;
        wash_[static_cast<std::size_t>(low)] += moved;
    }
    for (std::size_t c = 0; c < cells; ++c) soil_.nutrient[c] += wash_[c];
}

void Simulation::take_census() {
    Census k;
    for (std::size_t i = 0; i < c_.size(); ++i) {
        const Phenotype& p = c_.pheno[i];
        if (p.has_brain && p.max_speed > 0.0f) ++k.movers;
        if (p.sense_cells > 0) ++k.sighted;
        if (p.part_total[static_cast<int>(PartType::Armour)] > 0.0f) ++k.armoured;
        if (p.toxicity > 0.0f) ++k.poisonous;
        const float l = c_.fed_light[i], v = c_.fed_living[i], d = c_.fed_dead[i], w = c_.fed_waste[i];
        const float total = l + v + d + w;
        if (total < 20.0f)           ++k.young;
        else if (l > 0.6f * total)   ++k.producers;
        else if (v > 0.6f * total)   ++k.hunters;
        else if (d > 0.6f * total)   ++k.scavengers;
        else if (w > 0.6f * total)   ++k.cleaners;
        else                         ++k.mixed;
    }
    census_ = k;
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

    // Firsts. "Sustained" = a handful living that way at once, not one freak.
    auto first = [&](bool& logged, std::size_t count, const char* text) {
        if (logged || count < 10) return;
        logged = true;
        chronicle_.add(tick_, text);
    };
    first(movers_logged_, census_.movers, "Something stirs: creatures have begun to move.");
    first(sighted_logged_, census_.sighted, "The first senses: some creatures can now feel what lies around them.");
    first(armoured_logged_, census_.armoured, "The first armour: some bodies have grown plates.");
    first(poisonous_logged_, census_.poisonous, "The first poison: some bodies now sicken whatever feeds on them.");
    first(hunters_logged_, census_.hunters, "The first hunters: some creatures now live mostly by draining the living.");
    first(scavengers_logged_, census_.scavengers, "The first scavengers: some creatures now live mostly on the dead.");
    first(cleaners_logged_, census_.cleaners, "The first cleaners: some creatures now live mostly on what others give off.");

    // First time the world holds 1,000 / 10,000 / 100,000 ... creatures.
    while (pop >= next_milestone_) {
        chronicle_.add(tick_, "Life is now " + with_commas(next_milestone_) + " strong.");
        next_milestone_ *= 10;
    }

    // Mass die-off: population halves from its peak. With a day/night cycle
    // the population breathes every day (the young of the afternoon don't all
    // see the morning), so the yardstick is each day's peak, judged at the end
    // of that day. It is logged once, then ends either with a recovery (back
    // to 90% of the old peak) or, if the world has simply become poorer,
    // quietly after a while.
    day_peak_ = std::max(day_peak_, pop);
    const uint64_t day = cfg_.day_length > 0 ? tick_ / static_cast<uint64_t>(cfg_.day_length) : tick_ / 50;
    if (day == day_seen_) return;
    day_seen_ = day;
    const std::size_t high = day_peak_;
    day_peak_ = 0;

    if (crash_from_ == 0) {
        peak_pop_ = std::max(peak_pop_, high);
        if (peak_pop_ >= 100 && high * 2 < peak_pop_) {
            chronicle_.add(tick_, "A great dying: only " + with_commas(high) + " remain of " +
                                      with_commas(peak_pop_) + ".");
            crash_from_ = peak_pop_;
            crash_tick_ = tick_;
        }
    } else if (high * 10 >= crash_from_ * 9) {
        chronicle_.add(tick_, "Life has recovered: " + with_commas(high) + " creatures.");
        crash_from_ = 0;
        peak_pop_ = high;
    } else if (tick_ - crash_tick_ >= 20000) {
        crash_from_ = 0;  // the new normal; watch for the next die-off from here
        peak_pop_ = high;
    }
}

}  // namespace luca
