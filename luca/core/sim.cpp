#include "sim.hpp"
#include "laws.hpp"

#include "creature_file.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "energy.hpp"
#include "footprint.hpp"
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
inline const float& WANDER_PATH    = g_laws.WANDER_PATH;  // law, 1.0f
// Growing up (2026-11-12): how fast a young body may build itself, and how
// much of what it holds it keeps back rather than turning into body.
inline const float& GROW_STEP      = g_laws.GROW_STEP;  // law, 0.05f: most of its adult mass it can add in one go (every 8 ticks)
inline const float& GROW_MIN       = g_laws.GROW_MIN;  // law, 0.01f: least worth stopping to build (except the last bit)
inline const float& GROW_RESERVE   = g_laws.GROW_RESERVE;  // law, 0.3f: of what it can hold, kept back for the night
constexpr int      SOCIAL_CELLS   = 2;     // the danger / kin sense looks this many cells out at most
inline const float& SOCIAL_FAST    = g_laws.SOCIAL_FAST;  // law, 0.25f: a body covering this much a tick counts as fully "moving"

// Species splits (check_splits). A species is looked at only when it has at
// least twice SPLIT_MIN alive; a sample of up to SPLIT_SAMPLE members is cut
// into two clusters, and they count as two species when each holds at least
// SPLIT_SHARE of the sample, their centres are SPLIT_GAP apart or more, and that
// gap is SPLIT_SEPARATION times the clusters' mean spread. After relabelling,
// both sides must still hold SPLIT_MIN.
constexpr uint64_t SPLIT_INTERVAL   = 2000;
constexpr uint32_t SPLIT_MIN        = 15;
constexpr std::size_t SPLIT_SAMPLE  = 48;
constexpr float    SPLIT_SHARE      = 0.25f;
constexpr float    SPLIT_GAP        = 0.8f;
constexpr float    SPLIT_SEPARATION = 3.0f;
constexpr int      SPLIT_ROUNDS     = 3;
// Following (watch_followed): a trait is "theirs" once TRAIT_ON of them carry
// it and lost once no more than TRAIT_OFF do, so a trait hovering near half
// doesn't raise an alert every time it is looked at.
constexpr uint64_t WATCH_INTERVAL   = 200;
constexpr float    TRAIT_ON         = 0.6f;
constexpr float    TRAIT_OFF        = 0.4f;
constexpr float    SIZE_ALERT       = 0.2f;  // mean adult mass this far from the last report
constexpr uint32_t WATCH_MIN        = 20;    // fewer alive than this: a mean size is noise, and doubling is nothing
constexpr int      MOVING_TRAIT     = static_cast<int>(PartType::Count);

}  // namespace

Simulation::Simulation(const SimConfig& cfg)
    : cfg_(cfg), draw_seed_(mix64(cfg.seed * 0x2545F4914F6CDD1Dull + 1)), events_(cfg.seed, cfg.event_rate) {
    world_.generate(cfg_);
    const std::size_t cells = static_cast<std::size_t>(world_.num_cells());
    cell_demand_.assign(cells, 0.0f);
    cell_corpse_demand_.assign(cells, 0.0f);
    cell_uptake_demand_.assign(cells * NCHEM, 0.0f);
    biomass_.assign(cells, 0.0f);
    leaver_.assign(cells, NO_LEAVER);
    left_.assign(cells, 0.0f);
    cell_p_demand_.assign(cells, 0.0f);

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
    lay_out_mineral();
}

// The mineral P (docs/design/stoichiometry.md): patchier than A (rich spots,
// long poor stretches), laid out by its own noise, scaled so the mean is P_MEAN.
void Simulation::lay_out_mineral() {
    const std::size_t cells = soil_.mineral.size();
    double sum = 0.0;
    for (std::size_t c = 0; c < cells; ++c) {
        const float k = std::clamp((world_.patch_noise(static_cast<int>(c), 131u) - 0.3f) / 0.4f, 0.0f, 1.0f);
        soil_.mineral[c] = k * k;
        sum += soil_.mineral[c];
    }
    const float mean_raw = sum > 0.0 ? static_cast<float>(sum / static_cast<double>(cells)) : 1.0f;
    const float patchy = std::clamp(cfg_.nutrient_patchiness, 0.0f, 1.0f);
    for (float& v : soil_.mineral)
        v = std::max(0.0f, g_laws.P_MEAN) * ((1.0f - patchy) + patchy * v / mean_raw);
    std::fill(soil_.corpse_p.begin(), soil_.corpse_p.end(), 0.0f);
}

void Simulation::seed_population() {
    const Genome luca = make_luca();
    const Phenotype p = compile_phenotype(luca);
    const float ext = world_.extent();
    c_.reserve(static_cast<std::size_t>(cfg_.initial_population) * 4);
    for (int k = 0; k < cfg_.initial_population; ++k) {
        Rng rng(draw_key(draw_seed_, Draw::Start, tick_, static_cast<uint64_t>(k)));
        const float x = world_.clamp_coord(rng.uniform(0.0f, ext));
        const float y = world_.clamp_coord(rng.uniform(0.0f, ext));
        // The first LUCAs are identical: one species.
        const uint32_t sp = species_.all().empty() ? species_.found_root(luca, tick_, "Luca")
                                                   : species_.classify_newcomer(luca, tick_);
        // They arrive with a body and a store of D: matter and energy from outside the books, once.
        admit(x, y, pure(CHEM_D, std::min(cfg_.initial_energy, p.energy_capacity) / TOP_LEVEL), pure(CHEM_D, p.mass),
              next_lineage_++, sp, 0, luca, p);
    }
}

void Simulation::admit(float x, float y, const Mix& store, const Mix& body, uint32_t lineage, uint32_t species, uint32_t generation,
                       Genome genome, const Phenotype& pheno, float body_temp, float body_p, float store_p) {
    if (body_temp < -273.0f) body_temp = felt_temperature(pheno, world_.env(world_.cell_index(x, y)));
    // body_p < 0: a newcomer from outside, built with exactly the P it needs.
    c_.add(x, y, store, body, body_temp, next_id_++, lineage, species, generation, std::move(genome), pheno,
           body_p < 0.0f ? pheno.p_need : body_p, store_p);
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
    alert(s.parent, "A new species branches from your " + from.name + ": " + s.name +
                        (how.empty() ? "." : ". It " + how + "."));
}

uint64_t Simulation::add_creature(float x, float y, float energy, const Genome& genome, float grown) {
    Genome g = genome;
    clamp_genome(g);
    const Phenotype p = compile_phenotype(g, grown);
    const uint64_t id = next_id_;
    const uint32_t sp = species_.classify_newcomer(g, tick_);
    // Placed from outside: its body and its store (of D) are new matter and energy.
    admit(world_.clamp_coord(x), world_.clamp_coord(y), pure(CHEM_D, std::clamp(energy, 0.0f, p.energy_capacity) / TOP_LEVEL),
          pure(CHEM_D, p.mass), next_lineage_++, sp, 0, std::move(g), p);
    return id;
}

void Simulation::fork(uint64_t fork_seed) {
    draw_seed_ = mix64(draw_seed_ ^ mix64(fork_seed * 0xD1B54A32D192ED03ull + 0xF0F0));
    events_.fork(fork_seed);
    chronicle_.add(tick_, "The world forks (fork " + std::to_string(fork_seed) + ").");
}

void Simulation::start_scenario(uint32_t id) {
    if (id >= scenario_list().size()) id = 0;
    scenario_ = ScenarioState{};
    scenario_.id = id;
    scenario_.start_tick = tick_;
    if (id != 0) chronicle_.add(tick_, std::string("A goal is set: ") + scenario_.info().title + ". " + scenario_.info().brief);
}

std::size_t Simulation::knockout(uint32_t species) {
    std::size_t gone = 0;
    for (std::size_t k = c_.size(); k-- > 0;) {
        if (c_.species[k] != species) continue;
        // As any death: its body and store stay where it stood, as a corpse.
        Mix remains = c_.tissue[k];
        remains.add(c_.pool[k]);
        soil_.add_corpse(static_cast<std::size_t>(world_.cell_index(c_.x[k], c_.y[k])), remains, c_.tissue_p[k] + c_.pool_p[k]);
        species_.note_death(species, c_.id[k], c_.age[k], c_.kills[k], c_.children[k], false, tick_);
        if (species_.died(species, tick_)) {
            const Species& s = species_.get(species);
            chronicle_.add(tick_, s.name + " is knocked out of the world (" + std::to_string(gone + 1) + " bodies).");
        }
        if (!borrowed_.empty()) borrowed_.erase(c_.id[k]);
        c_.remove_swap(k);
        ++gone;
    }
    return gone;
}

double Simulation::stored_energy() const {
    double s = 0.0;
    for (const Mix& m : c_.pool) s += m.energy();
    for (const Mix& m : c_.tissue) s += m.energy();
    for (const Mix& m : soil_.corpse) s += m.energy();
    for (const Mix& m : soil_.dissolved) s += m.energy();
    return s;
}

double Simulation::total_matter() const {
    double s = 0.0;
    for (const Mix& m : c_.pool) s += m.mass();
    for (const Mix& m : c_.tissue) s += m.mass();
    for (float m : soil_.nutrient) s += m;
    for (const Mix& m : soil_.corpse) s += m.mass();
    for (const Mix& m : soil_.dissolved) s += m.mass();
    return s;
}

double Simulation::total_p() const {
    double s = 0.0;
    for (float m : c_.pool_p) s += m;
    for (float m : c_.tissue_p) s += m;
    for (float m : soil_.mineral) s += m;
    for (float m : soil_.corpse_p) s += m;
    return s;
}

// -----------------------------------------------------------------------------
// The brain: what the body feels -> its evolved network -> a direction and a
// throttle. See BrainInput / BrainOutput in genome.hpp. Reads only the current
// state of the world; writes this creature's scratch slots and its own memory.
// -----------------------------------------------------------------------------
// The field a channel reads, at one point, by bilinear interpolation over the
// four nearest cell centres (clamped at the world's edge), so any separation
// between two sensors gives a difference (14.9, trap 1).
float Simulation::sample_channel(int channel, float x, float y, const Phenotype& p, float body_temp) const {
    const int n = world_.size();
    const float cs = std::max(world_.cell_size(), 1e-3f);
    const float fx = x / cs - 0.5f, fy = y / cs - 0.5f;
    const int ix = static_cast<int>(std::floor(fx)), iy = static_cast<int>(std::floor(fy));
    const float tx = fx - static_cast<float>(ix), ty = fy - static_cast<float>(iy);
    auto at = [&](int cx, int cy) {
        const int c = std::clamp(cy, 0, n - 1) * n + std::clamp(cx, 0, n - 1);
        switch (channel) {
            case CH_LIGHT:    return world_.light(c);
            case CH_WARMTH:   return std::clamp((world_.temperature(c) - p.thermal_optimum) / std::max(p.thermal_tolerance, 1.0f), -2.0f, 2.0f);
            case CH_CORPSE:   return soil_.corpse_energy[c];
            case CH_NUTRIENT: return soil_.nutrient[c];
            case CH_LOOSE_B:  return soil_.dissolved[c].v[CHEM_B];
            case CH_LOOSE_C:  return soil_.dissolved[c].v[CHEM_C];
            case CH_LOOSE_D:  return soil_.dissolved[c].v[CHEM_D];
            case CH_LIVING:   return biomass_[c];
            default:          return 0.0f;
        }
    };
    (void)body_temp;
    const float top = at(ix, iy) * (1.0f - tx) + at(ix + 1, iy) * tx;
    const float bottom = at(ix, iy + 1) * (1.0f - tx) + at(ix + 1, iy + 1) * tx;
    return top * (1.0f - ty) + bottom * ty;
}

// The sensor brain (SENSOR_BRAIN, 14.9, stage 1). Sensors read one point each
// into a bus of level and side-contrast inputs per channel; the brain drives
// thrust and turn directly. Heading is state (hx, hy); turning is paid for as
// distance along the rim and counts as moving.
void Simulation::decide_sensor_brain(std::size_t i, int cell, const LocalEnv& env) {
    const Phenotype& p = c_.pheno[i];
    const BrainPlan& plan = c_.brain[i];
    const float hunger = 1.0f - std::clamp(c_.energy[i] / p.repro_threshold, 0.0f, 1.0f);
    const float crowd = std::min(biomass_[cell] * 0.25f, 1.0f);
    const float want = std::clamp((p.thermal_optimum - c_.body_temp[i]) / std::max(p.thermal_tolerance, 1.0f), -1.0f, 1.0f);

    float in[NUM_INPUTS] = {};
    in[IN_BIAS]   = 1.0f;
    in[IN_HUNGER] = hunger;
    in[IN_LIGHT]  = env.light;
    in[IN_CROWD]  = crowd;
    in[IN_COLD]   = want;
    in[IN_CORPSE] = std::min(soil_.corpse_energy[cell] * (1.0f / 250.0f), 1.0f);
    in[IN_MOVED]  = p.max_speed > 0.0f ? std::min(c_.moved[i] / p.max_speed, 1.0f) : 0.0f;
    const float burn = std::max(p.upkeep, 1e-3f);
    in[IN_EATING] = std::min(c_.ate[i] / (2.0f * burn), 1.0f);
    in[IN_HURT]   = std::min(c_.hurt[i] / burn, 1.0f);
    in[IN_NOISE]  = 2.0f * key_uniform(draw_key(draw_seed_, Draw::Noise, tick_, c_.id[i])) - 1.0f;

    // The bus. k scales each field as a receptor would: level = m / (|m| + k),
    // contrast = (left - right) / (|left| + |right| + k).
    static constexpr float K[CH_COUNT] = {0.5f, 0.5f, 100.0f, 6.0f, 0.01f, 0.01f, 0.01f, 1.0f};
    const float heading = std::atan2(c_.hy[i], c_.hx[i]);
    float size_sum[CH_COUNT] = {}, value_sum[CH_COUNT] = {};
    float left_w[CH_COUNT] = {}, right_w[CH_COUNT] = {}, left_v[CH_COUNT] = {}, right_v[CH_COUNT] = {};
    for (int s = 0; s < plan.sensors; ++s) {
        const SensorPoint& sp = plan.sensor[s];
        const float a = heading + sp.angle;
        const float v = sample_channel(sp.channel, c_.x[i] + std::cos(a) * sp.range, c_.y[i] + std::sin(a) * sp.range, p, c_.body_temp[i]);
        const int ch = sp.channel;
        size_sum[ch] += sp.size;
        value_sum[ch] += sp.size * v;
        const float side = std::sin(sp.angle);
        if (side > 0.05f)       { left_w[ch] += sp.size * side;  left_v[ch] += sp.size * side * v; }
        else if (side < -0.05f) { right_w[ch] += sp.size * -side; right_v[ch] += sp.size * -side * v; }
    }
    for (int ch = 0; ch < CH_COUNT; ++ch) {
        if (size_sum[ch] <= 0.0f) continue;
        const float m = value_sum[ch] / size_sum[ch];
        in[bus_level(ch)] = m / (std::abs(m) + K[ch]);
        if (left_w[ch] > 0.0f && right_w[ch] > 0.0f) {
            const float l = left_v[ch] / left_w[ch], r = right_v[ch] / right_w[ch];
            in[bus_contrast(ch)] = (l - r) / (std::abs(l) + std::abs(r) + K[ch]);
        }
    }

    float w[NUM_OUTPUTS];
    think(plan, c_.brain_state[i], in, w);
    c_.shut[i] = 1.0f - std::clamp(1.0f + w[OUT_MOUTH], 0.0f, 1.0f);   // read by the next tick's contact
    if (p.max_speed <= 0.0f) return;                                     // a brain on a body that cannot move only gates its mouth
    const float thrust = std::clamp(g_laws.THRUST_BASE + w[OUT_THRUST], 0.0f, 1.0f);   // THRUST_BASE 0.5 moves by default, as the old throttle did
    const float turn = std::clamp(w[OUT_TURN], -1.0f, 1.0f);
    const float dtheta = turn * g_laws.TURN_RATE;
    const float arc = p.radius * std::abs(dtheta) * g_laws.TURN_COST;   // the rim's distance, as dear as TURN_COST says
    const float dist = thrust * p.max_speed;
    if (dist <= 0.0f && arc <= 0.0f) return;

    const float a = heading + dtheta;
    const float ux = std::cos(a), uy = std::sin(a);
    const float nx = dist > 0.0f ? world_.clamp_coord(c_.x[i] + ux * dist) : c_.x[i];
    const float ny = dist > 0.0f ? world_.clamp_coord(c_.y[i] + uy * dist) : c_.y[i];
    const float dh = dist > 0.0f ? world_.height(world_.cell_index(nx, ny)) - env.height : 0.0f;
    const float step_cost = std::max(0.0f, movement_cost(p, dist + arc, dh));
    if (step_cost + cost_upkeep_[i] + cost_thermal_[i] > power(p, c_.pool[i], soil_.nutrient[cell])) return;
    new_x_[i] = nx;
    new_y_[i] = ny;
    new_hx_[i] = ux;
    new_hy_[i] = uy;
    arc_[i] = arc;
    cost_move_[i] = step_cost;
}

void Simulation::decide_movement(std::size_t i, int cell, const LocalEnv& env) {
    if (g_laws.SENSOR_BRAIN != 0.0f) return decide_sensor_brain(i, cell, env);
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
    // Touch: what crossed its membrane last tick, against what it burns.
    const float burn = std::max(p.upkeep, 1e-3f);
    in[IN_EATING] = std::min(c_.ate[i] / (2.0f * burn), 1.0f);
    in[IN_HURT]   = std::min(c_.hurt[i] / burn, 1.0f);
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
                for (int xx = std::max(cx - reach, 0); xx <= std::min(cx + reach, n - 1); ++xx)
                for (uint32_t slot = grid_.begin(yy * n + xx), stop = grid_.end(yy * n + xx); slot < stop; ++slot) {
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
    Rng rng(draw_key(draw_seed_, Draw::Wander, tick_, c_.id[i]));
    angle += std::sqrt(2.0f * dist / WANDER_PATH) * rng.normal();
    if (angle > TWO_PI) angle -= TWO_PI; else if (angle < 0.0f) angle += TWO_PI;
    dx += w[OUT_WANDER] * std::cos(angle) + w[OUT_INERTIA] * c_.hx[i];
    dy += w[OUT_WANDER] * std::sin(angle) + w[OUT_INERTIA] * c_.hy[i];

    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-5f || throttle <= 0.0f) return;  // stays put

    const float ux = dx / len, uy = dy / len;
    const float nx = world_.clamp_coord(c_.x[i] + ux * dist);
    const float ny = world_.clamp_coord(c_.y[i] + uy * dist);
    const float dh = world_.height(world_.cell_index(nx, ny)) - env.height;
    // A body can only spend as fast as its enzymes release (burn): a step it
    // can't pay for this tick, on top of upkeep and heat, it doesn't take.
    // So a fast or climbing body needs enzymes with the rate to power it.
    const float step_cost = std::max(0.0f, movement_cost(p, dist, dh));
    if (step_cost + cost_upkeep_[i] + cost_thermal_[i] > power(p, c_.pool[i], soil_.nutrient[cell])) return;
    new_x_[i] = nx;
    new_y_[i] = ny;
    new_hx_[i] = ux;
    new_hy_[i] = uy;
    cost_move_[i] = step_cost;
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
    // Meteors that landed while time stood still (flush_powers) count in this tick.
    stats_.energy_in += arrived_energy_;
    stats_.in_meteor += arrived_energy_;
    stats_.matter_in += arrived_matter_;
    stats_.p_in += arrived_p_;
    arrived_energy_ = arrived_matter_ = arrived_p_ = 0.0;
    const bool p_on = g_laws.P_BASE > 0.0f;   // stoichiometry (docs/design/stoichiometry.md); off: no body needs P

    // God powers act first, then the sun moves, so every creature sees the
    // same world this tick.
    events_.update(tick_, world_, powers_, chronicle_);
    powers_.update(tick_, world_, chronicle_);
    land_arrivals(false);
    world_.set_time(tick_);
    drop_windfalls();
    if (world_.any_vents()) run_vents();

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
    arc_.resize(n);
    corpse_want_.resize(n);
    corpse_gain_.resize(n);
    uptake_want_.resize(n);
    uptake_gain_.resize(n);
    drain_want_.assign(n, 0.0f);
    drained_.assign(n, 0.0f);
    intake_.assign(n, Mix{});
    top_taker_.assign(n, 0);
    top_take_.assign(n, 0.0f);
    poisoned_.assign(n, 0.0f);
    if (p_on) {
        p_want_.resize(n);
        p_gain_.resize(n);
        intake_p_.assign(n, 0.0f);
    }

    // --- Phase 0: bin creatures into cells; living mass per cell --------------
    // The per-cell sums are cleared only where last tick wrote them (14.5 item 1).
    for (int c : scratch_cells_) {
        biomass_[c] = cell_demand_[c] = cell_corpse_demand_[c] = cell_p_demand_[c] = 0.0f;
        for (int ch = 0; ch < NCHEM; ++ch) cell_uptake_demand_[static_cast<std::size_t>(c) * NCHEM + ch] = 0.0f;
    }
    for (std::size_t i = 0; i < n; ++i) {
        const int cell = world_.cell_index(c_.x[i], c_.y[i]);
        cell_of_[i] = cell;
        biomass_[cell] += c_.pheno[i].mass;
    }
    grid_.build(cell_of_, world_.num_cells());
    scratch_cells_ = grid_.occupied();

    // --- Phase 1a: what everyone asks for, summed per cell --------------------
    const bool dark = world_.daylight() <= 0.0f;
    for (std::size_t i = 0; i < n; ++i) {
        const Phenotype& p = c_.pheno[i];
        const int cell = cell_of_[i];
        // A body on the move catches less light (it moved last tick; see energy.cpp).
        demand_[i] = dark ? 0.0f : std::max(0.0f, light_demand(p, world_.env(cell)) * light_while_moving(c_.moved[i]) * c_.unfold[i] * (1.0f - c_.sleep[i]));
        gain_[i] = demand_[i];
        cell_demand_[cell] += demand_[i];
        corpse_want_[i] = p.absorb > 0.0f ? std::max(0.0f, corpse_demand(p)) / TOP_LEVEL * (1.0f - c_.sleep[i]) * (1.0f - c_.shut[i]) : 0.0f;   // mass a tick
        cell_corpse_demand_[cell] += corpse_want_[i];
        // Through its skin, every chemical it can burn (energy.hpp).
        uptake_want_[i] = std::max(0.0f, uptake_rate(p)) * (1.0f - c_.sleep[i]);
        if (uptake_want_[i] > 0.0f)
            for (int ch = CHEM_B; ch < NCHEM; ++ch)
                if (p.can_burn(ch)) cell_uptake_demand_[static_cast<std::size_t>(cell) * NCHEM + ch] += uptake_want_[i];
        // The mineral P, through its skin, while its store has room for more.
        if (p_on) {
            p_want_[i] = c_.pool_p[i] < p.p_store_cap ? std::max(0.0f, g_laws.P_UPTAKE * p.surface_area) * (1.0f - c_.sleep[i]) : 0.0f;
            cell_p_demand_[cell] += p_want_[i];
        }
    }

    // The dead in the next cells (DEAD_IN_REACH, 14.2.3): a mouth pulls on the
    // corpses of a neighbouring cell when that cell lies within its reach,
    // with the pull it has towards it, as it would on a living body there.
    corpse_asks_.clear();
    if (g_laws.DEAD_IN_REACH != 0.0f) {
        const int wn = world_.size();
        const float cs = world_.cell_size();
        for (std::size_t i = 0; i < n; ++i) {
            if (corpse_want_[i] <= 0.0f) continue;
            const Phenotype& p = c_.pheno[i];
            const BodyPlan& mine = c_.body[i];
            const int cx = cell_of_[i] % wn, cy = cell_of_[i] / wn;
            const float px = c_.x[i], py = c_.y[i], hx = c_.hx[i], hy = c_.hy[i];
            const float longest = std::min(p.reach + mine.longest_stalk, MAX_REACH);
            for (int yy = std::max(cy - 1, 0); yy <= std::min(cy + 1, wn - 1); ++yy)
                for (int xx = std::max(cx - 1, 0); xx <= std::min(cx + 1, wn - 1); ++xx) {
                    const int c = yy * wn + xx;
                    if (c == cell_of_[i] || soil_.corpse[c].mass() <= 0.0f) continue;
                    // Nearest point of that cell, and the way to its middle.
                    const float qx = std::clamp(px, xx * cs, (xx + 1) * cs), qy = std::clamp(py, yy * cs, (yy + 1) * cs);
                    const float d = std::hypot(qx - px, qy - py);
                    if (d > longest) continue;
                    float pull;
                    if (mine.even_mouth) pull = mine.mouth[0];
                    else {
                        const float ddx = (xx + 0.5f) * cs - px, ddy = (yy + 0.5f) * cs - py;
                        const int dir = body_dir(ddx * hx + ddy * hy, hx * ddy - hy * ddx);
                        if (d <= p.reach) pull = mine.mouth[dir];
                        else pull = d <= std::min(p.reach + mine.stalk[dir], MAX_REACH) ? mine.far_mouth[dir] : 0.0f;
                    }
                    const float want = std::max(0.0f, corpse_demand_toward(pull)) / TOP_LEVEL * (1.0f - c_.sleep[i]) * (1.0f - c_.shut[i]);
                    if (want <= 0.0f) continue;
                    corpse_asks_.push_back({static_cast<uint32_t>(i), c, want, 0.0f});
                    cell_corpse_demand_[c] += want;
                    scratch_cells_.push_back(c);
                }
        }
    }

    if (g_laws.LIGHT_FOOTPRINT != 0.0f) share_light_by_footprint(dark);
    else if (!dark) share_light_by_canopy();

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
            const float hx = c_.hx[i], hy = c_.hy[i];
            const BodyPlan& mine = c_.body[i];
            for (int yy = std::max(cy - 1, 0); yy <= std::min(cy + 1, wn - 1); ++yy) {
                for (int xx = x0; xx <= x1; ++xx)
                for (uint32_t slot = grid_.begin(yy * wn + xx), stop = grid_.end(yy * wn + xx); slot < stop; ++slot) {
                    const Near& q = near_[slot];
                    const float ddx = q.x - px, ddy = q.y - py;
                    const float touch = std::min(p.reach + q.reach, MAX_REACH);
                    const float reach = std::min(p.reach + mine.longest_stalk + q.reach, MAX_REACH);
                    const float d2 = ddx * ddx + ddy * ddy;
                    if (d2 > reach * reach || q.who == i) continue;
                    // Which of its mouths face the other body, and which of the
                    // other's plates face back (body.hpp). Beyond touching, only
                    // mouths out on stalks long enough reach it.
                    float pull;
                    if (mine.even_mouth) pull = mine.mouth[0];
                    else {
                        const int dir = body_dir(ddx * hx + ddy * hy, hx * ddy - hy * ddx);
                        if (d2 <= touch * touch) pull = mine.mouth[dir];
                        else {
                            const float far = std::min(p.reach + mine.stalk[dir] + q.reach, MAX_REACH);
                            pull = d2 <= far * far ? mine.far_mouth[dir] : 0.0f;
                        }
                    }
                    const BodyPlan& theirs = c_.body[q.who];
                    float membrane = MEMBRANE_BASE;
                    if (theirs.any_armour) {
                        const float gx = c_.hx[q.who], gy = c_.hy[q.who];
                        membrane += ARMOUR_RESIST * theirs.armour[body_dir(-ddx * gx - ddy * gy, -gx * ddy + gy * ddx)];
                    }
                    // As mass a tick from its store: a flow of energy as if the store were all D.
                    const float flow = contact_flow(p, pull, c_.pheno[q.who], membrane) / TOP_LEVEL * (1.0f - c_.sleep[i]) * (1.0f - c_.shut[i]);
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
            const float have = soil_.corpse[cell].mass(), want = cell_corpse_demand_[cell];
            corpse_gain_[i] = corpse_want_[i] * (want > have ? have / want : 1.0f);
        }
        // Loose chemicals: each body asks for a share of the stock; if the
        // shares come to more than all of it, they are scaled down.
        uptake_gain_[i].clear();
        if (uptake_want_[i] > 0.0f)
            for (int ch = CHEM_B; ch < NCHEM; ++ch) {
                if (!p.can_burn(ch)) continue;
                const float asked = cell_uptake_demand_[static_cast<std::size_t>(cell) * NCHEM + ch];
                uptake_gain_[i].v[ch] = soil_.dissolved[cell].v[ch] * uptake_want_[i] / std::max(1.0f, asked);
            }

        if (p_on)
            p_gain_[i] = p_want_[i] > 0.0f ? soil_.mineral[cell] * p_want_[i] / std::max(1.0f, cell_p_demand_[cell]) : 0.0f;

        // A dormant body burns less, whatever the weather.
        const float burning = cost_while_asleep(c_.sleep[i]);
        cost_upkeep_[i]  = std::max(0.0f, upkeep_cost(p, c_.age[i]) * burning);
        cost_thermal_[i] = std::max(0.0f, thermal_cost(p, c_.body_temp[i]) * burning);

        new_x_[i] = c_.x[i];
        new_y_[i] = c_.y[i];
        new_hx_[i] = c_.hx[i];
        new_hy_[i] = c_.hy[i];
        cost_move_[i] = 0.0f;
        arc_[i] = 0.0f;
        if (p.has_brain && (p.max_speed > 0.0f || (g_laws.SENSOR_BRAIN != 0.0f && p.absorb > 0.0f)) && c_.sleep[i] < AWAKE) decide_movement(i, cell, env);
    }

    for (CorpseAsk& a : corpse_asks_) {
        const float have = soil_.corpse[a.cell].mass(), want = cell_corpse_demand_[a.cell];
        a.gain = a.want * (want > have ? have / want : 1.0f);
    }

    // --- Phase 2a: flows between touching bodies ------------------------------
    // What flows is the giver's store, chemicals as they are mixed in it. A
    // body can't give more than it holds: if several pull on it at once, each
    // gets the same fraction of what it asked for.
    for (const Contact& k : contacts_) {
        const Mix& from = c_.pool[k.giver];
        const float have = from.mass(), want = drain_want_[k.giver];
        const float amount = want > have ? k.flow * (have / want) : k.flow;
        if (amount <= 0.0f || have <= 0.0f) continue;
        drained_[k.giver] += amount;
        intake_[k.taker].add(from, amount / have);
        if (p_on) intake_p_[k.taker] += c_.pool_p[k.giver] * (amount / have);   // its store's P goes with it
        if (amount > top_take_[k.giver]) {
            top_take_[k.giver] = amount;
            top_taker_[k.giver] = k.taker;
        }
        // A poisonous body costs its eater energy for every unit taken.
        poisoned_[k.taker] += amount * from.density() * c_.pheno[k.giver].toxicity;
        food_.add(c_.species[k.taker], FoodKind::Living, c_.species[k.giver], amount * from.density());   // the food web (observation)

        // Gene transfer: now and then, something of the body being drained is
        // taken up as a blueprint. It goes into the taker's genome only: its
        // own body is already built and does not change, but its children
        // are built from the new plan (and pay for the extra part).
        const float transfer_chance = cfg_.gene_transfer_chance * g_laws.GENE_TRANSFER;   // law: 0 switches it off
        const uint64_t transfer_key = transfer_chance > 0.0f
            ? draw_key(draw_seed_, Draw::Transfer, tick_, c_.id[k.taker], c_.id[k.giver]) : 0;
        if (transfer_chance > 0.0f && key_uniform(transfer_key) < transfer_chance) {
            Rng rng(transfer_key);   // which part: the same key, drawn on
            float before[static_cast<int>(PartType::Count)] = {};
            for (const Part& part : c_.genome[k.taker].parts) before[static_cast<int>(part.type)] += part.size;
            PartType copied = PartType::Photo;
            const uint64_t taker_id = c_.id[k.taker];
            const bool first_borrow = borrowed_.find(taker_id) == borrowed_.end();
            Genome as_built = first_borrow ? c_.genome[k.taker] : Genome{};
            if (transfer_part(c_.genome[k.giver], c_.genome[k.taker], rng, copied)) {
                if (first_borrow) borrowed_.emplace(taker_id, Borrowed{as_built, std::move(as_built)});
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
    std::size_t ask_k = 0;   // cursor into corpse_asks_, which is in body order
    const bool spill_lost = g_laws.SPILL_LOST != 0.0f;   // law: 0 burns a meal's lost share at the mouth (14.2.2)
    for (std::size_t i = 0; i < n; ++i) {
        const Phenotype& p = c_.pheno[i];
        const int cell = cell_of_[i];
        Mix& pool = c_.pool[i];
        float& soil_a = soil_.nutrient[cell];
        const LocalEnv env_here = world_.env(cell);

        // Out: what others drained, in proportion to what its store holds
        // (its store's P too, in the same proportion: phase 2a gave it to them).
        float& pool_p = c_.pool_p[i];
        if (p_on && drained_[i] > 0.0f) {
            const float m = pool.mass();
            if (m > 0.0f) pool_p -= pool_p * std::min(1.0f, drained_[i] / m);
        }
        const float hurt = drained_[i] > 0.0f ? take_share(pool, drained_[i]).energy() : 0.0f;

        // In: the living and the dead arrive as chemicals. Of what it can
        // burn it keeps `efficiency`, or what fits in its store; the rest,
        // and everything it can't burn, spills into its cell as it is.
        Mix& loose = soil_.dissolved[cell];
        soil_.mark_loose(static_cast<std::size_t>(cell));
        float spilled = 0.0f;
        TickStats::Probe& probe = stats_.probe;   // observation only
        const bool full = pool.mass() >= 0.95f * p.pool_capacity;
        auto take_in = [&](const Mix& meal, bool living) {
            float usable = 0.0f;
            for (int ch = CHEM_B; ch < NCHEM; ++ch) usable += p.can_burn(ch) ? meal.v[ch] : 0.0f;
            const float room = std::max(0.0f, p.pool_capacity - pool.mass());
            const float keep = usable > 0.0f ? std::min(p.efficiency, room / usable) : 0.0f;
            soil_a += meal.v[CHEM_A];
            float kept = 0.0f;
            for (int ch = CHEM_B; ch < NCHEM; ++ch) {
                const float in = p.can_burn(ch) ? meal.v[ch] * keep : 0.0f;
                pool.v[ch] += in;
                kept += in * LEVEL[ch];
                if (!spill_lost && p.can_burn(ch)) {
                    // SPILL_LOST 0 (14.2.2): what it could burn but didn't keep is
                    // lost at the mouth as heat, burned down to A in its cell.
                    const float lost = meal.v[ch] - in;
                    soil_a += lost;
                    stats_.out_transfer += lost * LEVEL[ch];
                    stats_.energy_out += lost * LEVEL[ch];
                    continue;
                }
                loose.v[ch] += meal.v[ch] - in;
                spilled += (meal.v[ch] - in) * LEVEL[ch];
            }
            const double worth = meal.energy();
            if (living) {
                double usable_e = 0.0;
                for (int ch = CHEM_B; ch < NCHEM; ++ch) usable_e += p.can_burn(ch) ? meal.v[ch] * LEVEL[ch] : 0.0;
                probe.meal_living += worth;
                probe.kept_living += kept;
                probe.spill_share += usable_e * (1.0 - p.efficiency);
                probe.spill_room += usable_e * (p.efficiency - keep);
                probe.spill_unburnable += worth - usable_e;
                if (full) probe.meal_by_full += worth;
            } else {
                probe.meal_dead += worth;
                probe.kept_dead += kept;
            }
            return kept;
        };
        const float from_living = take_in(intake_[i], true);
        // 2.8 causes 2 and 3 (observation only): gear's share of upkeep, and
        // how long a mouth goes between meals from the living.
        if (p.absorb > 0.0f && p.upkeep > 0.0f) {
            probe.mouth_upkeep += cost_upkeep_[i];
            probe.mouth_gear += cost_upkeep_[i] * (p.gear_upkeep / p.upkeep);
        }
        if (p.has_brain && p.max_speed > 0.0f && p.upkeep > 0.0f) {
            probe.mover_upkeep += cost_upkeep_[i];
            probe.mover_gear += cost_upkeep_[i] * (p.gear_upkeep / p.upkeep);
        }
        if (from_living > 0.0f) {
            const uint32_t now = static_cast<uint32_t>(tick_) + 1;
            if (c_.last_meal[i] != 0 && now - c_.last_meal[i] > 1) {
                probe.meal_gaps += 1.0;
                probe.meal_gap_ticks += now - c_.last_meal[i];
            }
            c_.last_meal[i] = now;
        }
        float meal_p_dead = 0.0f;   // P in the corpses it ate
        float from_dead = corpse_gain_[i] > 0.0f ? take_in(soil_.take_corpse(cell, corpse_gain_[i], meal_p_dead), false) : 0.0f;
        for (; ask_k < corpse_asks_.size() && corpse_asks_[ask_k].taker == i; ++ask_k)
            if (corpse_asks_[ask_k].gain > 0.0f)
                from_dead += take_in(soil_.take_corpse(static_cast<std::size_t>(corpse_asks_[ask_k].cell), corpse_asks_[ask_k].gain, meal_p_dead), false);

        // The mineral P (stoichiometry): of the P its meals carried it keeps
        // what its store has room for and gives the rest off into its cell's
        // soil, where others take it up. Then it takes up P through its skin.
        if (p_on) {
            float& soil_p = soil_.mineral[cell];
            const float meal_p = intake_p_[i] + meal_p_dead;
            const float keep = std::min(meal_p, std::max(0.0f, p.p_store_cap - pool_p));
            pool_p += keep;
            soil_p += meal_p - keep;
            stats_.p_given_off += meal_p - keep;
            stats_.p_corpse_eaten += meal_p_dead;
            if (p_gain_[i] > 0.0f) {
                const float got = std::min({p_gain_[i], soil_p, std::max(0.0f, p.p_store_cap - pool_p)});
                if (got > 0.0f) {
                    soil_p -= got;
                    pool_p += got;
                    stats_.p_taken_up += got;
                }
            }
        }

        // Through its skin it takes up the loose chemicals it can burn, as far
        // as its store has room. What a vent made counts as food from the vent
        // (in proportion to the vent's share of the cell's stock; observation only).
        float from_waste = 0.0f, from_vent = 0.0f;
        const float asked = uptake_gain_[i].mass();
        const float loose_before = asked > 0.0f && leaver_[cell] != NO_LEAVER ? loose.energy() : 0.0f;   // leavings tag
        if (asked > 0.0f) {
            const float k = std::min(1.0f, std::max(0.0f, p.pool_capacity - pool.mass()) / asked);
            for (int ch = CHEM_B; ch < NCHEM; ++ch) {
                const float got = std::min(uptake_gain_[i].v[ch] * k, loose.v[ch]);
                if (got <= 0.0f) continue;
                float vented = 0.0f;
                if (ch == VENT_CHEM && soil_.vent_made[cell] > 0.0f) {
                    vented = got * std::min(1.0f, soil_.vent_made[cell] / loose.v[ch]);
                    soil_.vent_made[cell] = std::max(0.0f, soil_.vent_made[cell] - vented);
                }
                loose.v[ch] -= got;
                pool.v[ch] += got;
                from_waste += (got - vented) * LEVEL[ch];
                from_vent += vented * LEVEL[ch];
            }
        }

        // The leavings tag (observation only): the share of what it took up
        // that the cell's tagged species gave off.
        float untagged = from_waste;
        if (loose_before > 0.0f && from_waste > 0.0f) {
            const float credited = from_waste * std::min(1.0f, left_[cell] / loose_before);
            left_[cell] = std::max(0.0f, left_[cell] - credited);
            leavings_eaten_[(static_cast<uint64_t>(c_.species[i]) << 32) | leaver_[cell]] += credited;
            food_.add(c_.species[i], FoodKind::Leavings, leaver_[cell], credited);
            untagged -= credited;
        }
        {   // the food web (observation only): what did not come from another species' body
            const uint32_t sp = c_.species[i];
            food_.add_source(sp, FoodSource::Dead, from_dead);
            food_.add_source(sp, FoodSource::Vent, from_vent);
            food_.add_source(sp, FoodSource::Loose, untagged);
        }

        // Light comes in as energy with no matter: the body keeps it by
        // running its enzymes uphill, turning A from the soil into richer
        // chemicals in its store (charge). What it can't keep is never taken in.
        float two = 0.0f;   // units turned over by two-input enzymes (observation)
        const float from_light = gain_[i] > 0.0f ? charge(p, pool, soil_a, gain_[i], &two) : 0.0f;

        stats_.energy_in += from_light;
        food_.add_source(c_.species[i], FoodSource::Sun, from_light);
        stats_.eaten_living += from_living;
        stats_.eaten_dead += from_dead;
        stats_.eaten_waste += from_waste + from_vent;
        c_.fed_light[i] += from_light;
        c_.fed_vent[i] += from_vent;
        c_.fed_waste[i] += from_waste;
        c_.fed_living[i] += from_living;
        c_.fed_dead[i] += from_dead;
        c_.ate[i] = from_living + from_dead;   // felt next tick (touch)
        c_.hurt[i] = hurt;

        // Costs, all paid by burning its store (burn): poison from what it
        // ate, upkeep, heat, moving, and living among chemicals it can't burn.
        // A body that cannot pay them in full dies. All of it is lost as heat.
        float foul_mass = 0.0f;
        for (int ch = CHEM_B; ch < NCHEM; ++ch) foul_mass += p.can_burn(ch) ? 0.0f : loose.v[ch];
        const float foul = foul_mass > 0.0f ? std::max(0.0f, fouling_cost(p, foul_mass)) : 0.0f;
        const float want = poisoned_[i] + cost_upkeep_[i] + cost_thermal_[i] + cost_move_[i] + foul;
        const float held = pool.energy();   // probe: what its store was worth before paying
        const float paid = want > 0.0f ? burn(p, pool, soil_a, want, &two, &loose) : 0.0f;
        stats_.two_flow += two;
        if (want > 0.0f) {
            const float f = paid / want;  // pro-rata if it couldn't pay in full
            stats_.energy_out   += paid;
            stats_.out_upkeep   += cost_upkeep_[i] * f;
            stats_.out_thermal  += cost_thermal_[i] * f;
            stats_.out_movement += cost_move_[i] * f;
            stats_.out_fouling  += foul * f;
            stats_.out_poison   += poisoned_[i] * f;
        }
        // It gives off whatever its store holds that it can't burn (made by
        // its own enzymes, or handed down by a parent that could).
        for (int ch = CHEM_B; ch < NCHEM; ++ch) {
            if (p.can_burn(ch) || pool.v[ch] <= 0.0f) continue;
            loose.v[ch] += pool.v[ch];
            spilled += pool.v[ch] * LEVEL[ch];
            pool.v[ch] = 0.0f;
        }
        stats_.excreted += spilled;
        if (spilled > 0.0f) tag_leavings(cell, c_.species[i], spilled);
        const bool starved = paid + 1e-5f * want + 1e-7f < want;

        c_.moved[i] = std::hypot(new_x_[i] - c_.x[i], new_y_[i] - c_.y[i]) + arc_[i] * g_laws.ARC_FOLDS;   // turning counts as moving (ARC_FOLDS 0: it does not fold the leaves)
        c_.unfold[i] = next_unfold(c_.unfold[i], c_.moved[i]);
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

        if (starved) {
            probe.deaths += 1.0;
            probe.deaths_drained += drained_[i] > 0.0f;
            probe.deaths_could_pay += held >= want;
            probe.deaths_poison += poisoned_[i] > 0.0f &&
                                   poisoned_[i] >= std::max({cost_upkeep_[i], cost_thermal_[i], cost_move_[i], foul});
            probe.store_at_death += p.energy_capacity > 0.0f ? held / p.energy_capacity : 0.0f;
            if (p.absorb > 0.0f && c_.last_meal[i] != 0) {
                probe.mouth_deaths_fed += 1.0;
                probe.fast_at_death += static_cast<uint32_t>(tick_) + 1 - c_.last_meal[i];
            }
            // Death: the body and whatever is left in its store stay where it
            // fell, as chemicals: food for whatever can take them.
            Mix remains = c_.tissue[i];
            remains.add(pool);
            soil_.add_corpse(static_cast<std::size_t>(cell), remains, c_.tissue_p[i] + pool_p);
            pool.clear();
            pool_p = 0.0f;
            c_.energy[i] = 0.0f;
            dead_.push_back(static_cast<uint32_t>(i));
            // The record book: whatever took most from it this tick finished it off.
            if (drained_[i] > 0.0f) {
                const uint32_t killer = top_taker_[i];
                ++c_.kills[killer];
                species_.note_kill(c_.species[killer], c_.species[i]);
            }
            continue;
        }

        // Growing up: every 8 ticks a young body builds more of itself out of
        // its store, keeping back a reserve, at most GROW_STEP of its adult
        // mass at a time. Its body is made of whatever its store holds, so
        // chemicals only move from store to body: neither book changes.
        if (p.grown < 1.0f && (c_.age[i] & 7u) == 4 && c_.sleep[i] < AWAKE) {
            const float adult_mass = p.mass / p.grown;
            const float rich = pool.density();
            const float spare = pool.energy() - GROW_RESERVE * p.energy_capacity;
            float dm = rich > 0.0f ? std::min({GROW_STEP * adult_mass, adult_mass - p.mass, pool.mass(), spare / rich}) : 0.0f;
            // Liebig: no more body than the P in its store can build.
            if (p_on && p.p_adult > 0.0f) dm = std::min(dm, pool_p * adult_mass / p.p_adult);
            if (dm > 0.0f && dm >= std::min(GROW_MIN * adult_mass, adult_mass - p.mass)) {
                float to = (p.mass + dm) / adult_mass;
                if (to > 0.999f) to = 1.0f;
                const Phenotype bigger = compile_phenotype(c_.genome[i], to);
                const float matter = bigger.mass - p.mass;
                if (matter > 0.0f && matter <= pool.mass()) {
                    c_.tissue[i].add(take_share(pool, matter));
                    if (p_on) {
                        const float dp = std::min(std::max(0.0f, bigger.p_need - p.p_need), pool_p);
                        pool_p -= dp;
                        c_.tissue_p[i] += dp;
                    }
                    stats_.grown_mass += matter;
                    c_.pheno[i] = bigger;   // `p` refers to this slot, so it now reads the bigger body
                    if (!borrowed_.empty())
                        if (auto b = borrowed_.find(c_.id[i]); b != borrowed_.end()) b->second.built = c_.genome[i];
                }
            }
        }

        // Reproduction: divide when the store is worth the threshold. The
        // child's body is built out of the parent's store, as it is mixed;
        // then the throw is paid; then the child takes its share of what is
        // left. Nothing is created. A body tries once every 8 ticks.
        // Only a grown body breeds; its child is born at the birth size in its
        // own (mutated) genome, and grows the rest itself.
        // With P (stoichiometry), it also waits until its store holds the P
        // its child's body needs. Tries held back each way are counted.
        bool tries = false;
        if (p.grown >= 1.0f && (c_.age[i] & 7u) == 0 && c_.sleep[i] < AWAKE) {
            if (pool.energy() < p.repro_threshold || pool.mass() <= p.mass * 0.75f * p.birth_size) ++stats_.energy_limited;
            else if (pool_p < p.p_adult * p.birth_size) ++stats_.p_limited;
            else tries = true;
        }
        if (tries) {
            Rng rng(draw_key(draw_seed_, Draw::Birth, tick_, c_.id[i]));
            Genome g = mutate(c_.genome[i], rng);
            const Phenotype cp = compile_phenotype(g, g.birth_size);
            // Dispersal is paid for: a child lands touching its parent, unless
            // the parent has the means to fling it further, which costs energy.
            const float ang = rng.uniform(0.0f, TWO_PI);
            const float flung = rng.uniform() * p.throw_range;
            const float thrown = flung > 0.0f ? std::max(0.0f, throw_cost(cp.mass, flung)) : 0.0f;
            if (pool.mass() > cp.mass && pool.energy() - cp.mass * pool.density() - thrown > 0.0f && pool_p >= cp.p_need) {
                Mix body = take_share(pool, cp.mass);
                const float paid_throw = thrown > 0.0f ? burn(p, pool, soil_a, thrown) : 0.0f;
                stats_.out_movement += paid_throw;
                stats_.energy_out += paid_throw;
                Mix store = take_share(pool, std::min(pool.mass() * p.offspring_share, cp.pool_capacity));
                float body_p = 0.0f, store_p = 0.0f;
                if (p_on) {
                    body_p = cp.p_need;
                    pool_p -= body_p;
                    store_p = std::min(pool_p * p.offspring_share, cp.p_store_cap);
                    pool_p -= store_p;
                }
                ++c_.children[i];
                float bx, by;
                land_child(c_.x[i], c_.y[i], std::cos(ang), std::sin(ang), p.reach + cp.reach + flung, bx, by);
                births_.push_back(Birth{
                    bx, by, store, body, c_.body_temp[i], c_.lineage[i], species_.classify(c_.species[i], g, tick_), c_.generation[i] + 1,
                    std::move(g), cp, body_p, store_p});
            }
        }
        c_.energy[i] = pool.energy();
    }

    // Deaths first (descending, so swap-remove never moves a dead creature),
    // then births appended at the end.
    for (auto it = dead_.rbegin(); it != dead_.rend(); ++it) {
        const uint32_t sp = c_.species[*it];
        species_.note_death(sp, c_.id[*it], c_.age[*it], c_.kills[*it], c_.children[*it], drained_[*it] > 0.0f, tick_);
        if (species_.died(sp, tick_)) {
            const Species& s = species_.get(sp);
            const std::string text = s.name + " is extinct, after " + with_commas(tick_ - s.born_tick) + " ticks.";
            if (s.established) chronicle_.add(tick_, text);
            alert(sp, "Your " + text);
        }
        if (!borrowed_.empty()) borrowed_.erase(c_.id[*it]);
        c_.remove_swap(*it);
    }
    for (Birth& b : births_)
        admit(b.x, b.y, b.store, b.body, b.lineage, b.species, b.generation, std::move(b.genome), b.pheno, b.body_temp, b.body_p, b.store_p);

    stats_.deaths = dead_.size();
    stats_.births = births_.size();
    ++tick_;
    if (tick_ % FIELD_INTERVAL == 0) { weather_corpses(); spread_dissolved(); }
    if (tick_ % WASH_INTERVAL == 0) wash_soil();
    // What the loose chemicals are worth, for the viewer and the metrics, where
    // bodies changed them (vents and the drift keep their own cells in step).
    for (int c : scratch_cells_) soil_.waste[c] = soil_.dissolved[c].energy();
    if (tick_ % SPLIT_INTERVAL == 0) check_splits();
    if (!watch_.empty() && tick_ % WATCH_INTERVAL == 0) watch_followed();
    if (tick_ % 50 == 0 || c_.size() == 0) {
        take_census();
        food_.fade();
        detect_events();
        if (check_scenario(scenario_, tick_, c_.size(), census_)) chronicle_.add(tick_, scenario_.verdict);
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
        for (uint32_t j : grid_.in_cell(cell)) canopy_.push_back({c_.pheno[j].canopy, j});   // core radius plus stalks
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

// LIGHT_FOOTPRINT (a law, 14.2.1): a body catches light over the cells its
// outline covers (Footprints), each in proportion, and in each cell the
// canopy rule shares the light among every body asking there: the tallest
// takes its fill first, bodies of the same height share alike. Demand and
// gain are the sums over its cells.
void Simulation::share_light_by_footprint(bool dark) {
    const std::size_t n = c_.size();
    const int wn = world_.size();
    const float cs = world_.cell_size();
    light_asks_.clear();
    light_ask_cell_.clear();
    for (std::size_t i = 0; i < n; ++i) {
        demand_[i] = gain_[i] = 0.0f;
        if (dark) continue;
        const Phenotype& p = c_.pheno[i];
        const float k = light_catch(p) * light_while_moving(c_.moved[i]) * c_.unfold[i] * (1.0f - c_.sleep[i]);
        if (k <= 0.0f) continue;
        const Footprints::Stencil& st = Footprints::of(p.radius / cs);
        const int cx = cell_of_[i] % wn, cy = cell_of_[i] / wn;
        float total = 0.0f;
        for (int dy = -st.ring; dy <= st.ring; ++dy) {
            const int yy = cy + dy;
            if (yy < 0 || yy >= wn) continue;
            for (int dx = -st.ring; dx <= st.ring; ++dx) {
                const int xx = cx + dx;
                const float w = st.w[std::abs(dx)][std::abs(dy)];
                if (xx < 0 || xx >= wn || w <= 0.0f) continue;   // light off the edge of the map falls on nothing
                const int cell = yy * wn + xx;
                const float want = k * world_.light(cell) * w;
                if (want <= 0.0f) continue;
                light_asks_.push_back({p.canopy, static_cast<uint32_t>(i), want});
                light_ask_cell_.push_back(cell);
                total += want;
            }
        }
        demand_[i] = total;
    }
    if (light_asks_.empty()) return;
    light_grid_.build(light_ask_cell_, world_.num_cells());
    for (int cell : light_grid_.occupied()) {
        float left = world_.light(cell);
        float want = 0.0f;
        for (uint32_t a : light_grid_.in_cell(cell)) want += light_asks_[a].want;
        if (want <= left) {
            for (uint32_t a : light_grid_.in_cell(cell)) gain_[light_asks_[a].who] += light_asks_[a].want;
            continue;
        }
        light_cell_.clear();
        for (uint32_t a : light_grid_.in_cell(cell)) light_cell_.push_back(light_asks_[a]);
        std::sort(light_cell_.begin(), light_cell_.end(), [](const LightAsk& a, const LightAsk& b) {
            return a.canopy != b.canopy ? a.canopy > b.canopy : a.who < b.who;
        });
        for (std::size_t a = 0; a < light_cell_.size();) {
            std::size_t b = a;
            float tier = 0.0f;
            for (; b < light_cell_.size() && light_cell_[b].canopy == light_cell_[a].canopy; ++b) tier += light_cell_[b].want;
            const float share = tier > left ? left / tier : 1.0f;
            for (std::size_t k = a; k < b; ++k) gain_[light_cell_[k].who] += light_cell_[k].want * share;
            left = std::max(0.0f, left - tier);
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

// Corpses nobody eats still fall apart, slowly: their chemicals burn down to A in the soil, the energy to heat.
void Simulation::weather_corpses() {
    Soil::each(soil_.corpse_bits, [&](std::size_t c) {
        const float cm = soil_.corpse[c].mass();
        float& cp = soil_.corpse_p[c];
        if (cm <= 0.0f) {
            if (cp != 0.0f) {   // the last of its P, once nothing is left to eat
                soil_.mineral[c] += cp;
                stats_.p_corpse_weathered += cp;
                cp = 0.0f;
            }
            bool any = false;
            for (float v : soil_.corpse[c].v) any = any || v != 0.0f;
            if (!any) Soil::unmark(soil_.corpse_bits, c);
            return;
        }
        const float rate = cm < 1e-3f ? 1.0f : CORPSE_WEATHERING;  // sweep up crumbs
        const Mix gone = soil_.take_corpse(c, cm * rate);
        // Weathering frees P more slowly than mass (P_WEATHER_SHARE): eating
        // the dead is the fast way it goes back to the soil.
        if (cp > 0.0f) {
            const float freed = rate >= 1.0f ? cp : cp * rate * std::clamp(g_laws.P_WEATHER_SHARE, 0.0f, 1.0f);
            cp -= freed;
            soil_.mineral[c] += freed;
            stats_.p_corpse_weathered += freed;
        }
        const float de = gone.energy();
        soil_.nutrient[c] += gone.mass();   // burned down to A
        stats_.out_decay += de;
        stats_.energy_out += de;
    });
}

// Windfalls: food falls on random cells, windfall_rate per cell per tick on
// average, in lumps of windfall_size. The number of lumps per tick has the
// right mean (whole lumps, plus one more by chance for the fraction), so on a
// 128 x 128 map at the default rate one falls about every 16 ticks. It lands
// in the cell's corpses as D made out of the cell's own A, so it is eaten,
// smelled and weathered exactly like the dead. It is new energy (booked under
// energy_in) but no new matter.
void Simulation::drop_windfalls() {
    if (cfg_.windfall_rate <= 0.0f || cfg_.windfall_size <= 0.0f) return;
    const int cells = world_.num_cells();
    float lumps = static_cast<float>(cells) * cfg_.windfall_rate / cfg_.windfall_size;
    int count = static_cast<int>(lumps);
    lumps -= static_cast<float>(count);
    Rng rng(draw_key(draw_seed_, Draw::Windfall, tick_));
    if (rng.uniform() < lumps) ++count;
    for (int k = 0; k < count; ++k) {
        const int cell = std::min(static_cast<int>(rng.uniform() * static_cast<float>(cells)), cells - 1);
        // The lump is D made from the cell's own A (no matter arrives from
        // outside), as much as the cell holds, up to windfall_size of energy.
        const std::size_t at = static_cast<std::size_t>(cell);
        const float made = std::min(cfg_.windfall_size / TOP_LEVEL, soil_.nutrient[at]);
        if (made <= 0.0f) continue;
        soil_.nutrient[at] -= made;
        soil_.add_corpse(at, pure(CHEM_D, made));
        stats_.in_windfall += made * TOP_LEVEL;
        stats_.energy_in += made * TOP_LEVEL;
        ++stats_.windfalls;
    }
}

// Vents (chemistry stage 3): each vent cell brings up VENT_CHEM from below,
// loose in the cell, vent_energy worth of it a tick, day and night. Both the
// energy and the matter are new (energy_in and in_vent; matter_in), so a vent
// works on bare ground and never strips its own soil. Who eats it is decided
// by enzymes alone: it is taken up like any loose chemical, and fouls
// whatever can't burn it. What nobody eats breaks down to A where it drifts,
// so the ground around a vent grows rich.
void Simulation::run_vents() {
    const auto& vent = world_.vents();
    const float level = LEVEL[VENT_CHEM];
    for (std::size_t c = 0; c < vent.size(); ++c) {
        if (vent[c] <= 0.0f) continue;
        const float made = world_.vent_energy(static_cast<int>(c)) / level;
        soil_.dissolved[c].v[VENT_CHEM] += made;
        soil_.vent_made[c] += made;
        soil_.mark_loose(c);
        soil_.waste[c] = soil_.dissolved[c].energy();
        stats_.energy_in += made * level;
        stats_.in_vent += made * level;
        stats_.matter_in += made;
        matter_brought_up_ += made;
        // Mineral comes up with it (stoichiometry): new P.
        const float pp = made * std::max(0.0f, g_laws.VENT_P);
        soil_.mineral[c] += pp;
        stats_.p_in += pp;
        p_brought_up_ += pp;
    }
}

void Simulation::land_arrivals(bool between_ticks) {
    for (const PowerCommand& cmd : powers_.take_arrivals()) land_meteor(cmd, between_ticks);
}

// A life-bearing meteor: `amount` bodies from the cargo, cycling through its
// genomes, laid out on a sunflower spiral within `radius` (no randomness, so
// a replayed power log lands them in the same places). Every body arrives
// grown, with a store worth 60% of what it needs to divide, made of the
// richest chemical it can burn (a vent dweller arrives full of C, a plant of
// D). All of them join one species: the nearest living one if it is close
// enough, otherwise a new root under the cargo's name.
void Simulation::land_meteor(const PowerCommand& cmd, bool between_ticks) {
    CreatureFile cargo;
    std::string why;
    if (!read_creature_file(cmd.creature, cargo, why)) return;   // parse_power_script and the viewer check first
    const int count = std::clamp(static_cast<int>(std::lround(cmd.amount)), 1, 500);
    const uint32_t sp = species_.classify_newcomer(cargo.bodies.front(), tick_, cargo.name);
    const uint32_t lineage = next_lineage_++;   // one founding event
    double energy = 0.0, matter = 0.0, mineral = 0.0;
    for (int k = 0; k < count; ++k) {
        const Genome& g = cargo.bodies[static_cast<std::size_t>(k) % cargo.bodies.size()];
        const Phenotype p = compile_phenotype(g);
        const float r = cmd.radius * std::sqrt((static_cast<float>(k) + 0.5f) / static_cast<float>(count));
        const float a = 2.3999632f * static_cast<float>(k);   // the golden angle
        int chem = CHEM_D;
        for (int c = NCHEM - 1; c > CHEM_A; --c)
            if (p.can_burn(c)) { chem = c; break; }
        const float store_e = std::min(0.6f * p.repro_threshold, p.energy_capacity);
        const Mix store = pure(chem, store_e / LEVEL[chem]);
        const Mix body = pure(CHEM_D, p.mass);
        energy += store.energy() + body.energy();
        matter += store.mass() + body.mass();
        mineral += p.p_need;   // built with the P it needs, brought from outside
        admit(world_.clamp_coord(cmd.x + r * std::cos(a)), world_.clamp_coord(cmd.y + r * std::sin(a)), store, body,
              lineage, sp, 0, g, p);
    }
    matter_brought_up_ += matter;
    p_brought_up_ += mineral;
    if (between_ticks) {
        arrived_energy_ += energy;
        arrived_matter_ += matter;
        arrived_p_ += mineral;
    } else {
        stats_.p_in += mineral;
        stats_.energy_in += energy;
        stats_.in_meteor += energy;
        stats_.matter_in += matter;
    }
    const Species& s = species_.get(sp);
    const std::string what = cargo.bodies.size() > 1 ? " a sample of " : " ";
    chronicle_.add(tick_, "A meteor falls in " + region_name(world_, cmd.x, cmd.y) + ", carrying" + what +
                              with_commas(static_cast<uint64_t>(count)) + " " + (cargo.name.empty() ? s.name : cargo.name) +
                              (s.name == cargo.name || cargo.name.empty() ? "." : " (counted among the " + s.name + ")."));
}

// The leavings tag (observation only; docs/design/cooperation.md): a cell
// remembers the species that gave off most of its loose chemicals, and how
// much of their worth that covers. A species that gives off more than the tag
// holds takes it over; the tag drifts and fades with the chemicals, and
// uptake draws on it in proportion.
void Simulation::tag_leavings(int cell, uint32_t species, float energy) {
    if (leaver_[cell] == species) left_[cell] += energy;
    else if (energy >= left_[cell]) {
        leaver_[cell] = species;
        left_[cell] = energy;
    }
}

// Loose chemicals drift: each cell hands WASTE_SPREAD of them to its four
// neighbours (a quarter each; what would fall off the edge stays put), then
// WASTE_DECAY of what is left breaks down to A in the soil, its energy lost as
// heat. All taking is worked out before any giving, so cell order doesn't matter.
void Simulation::spread_dissolved() {
    // Only cells that may hold something are walked (Soil::loose_bits), in
    // ascending order, so the sums come out as a walk over every cell would.
    const int n = world_.size();
    const std::size_t cells = soil_.dissolved.size();
    if (spread_.size() != cells) {
        spread_.assign(cells, Mix{});
        spread_vent_.assign(cells, 0.0f);
    }
    if (tag_in_.size() != cells) tag_in_.assign(cells, TagIn{});
    loose_was_ = soil_.loose_bits;   // the cells giving; their neighbours are marked as they receive
    Soil::each(loose_was_, [&](std::size_t cell) {
        const int c = static_cast<int>(cell);
        const int x = c % n, y = c / n;
        Mix& m = soil_.dissolved[c];
        const float mass = m.mass();
        if (mass <= 0.0f) return;
        if (mass < 1e-7f) {  // sweep up crumbs
            const double e = m.energy();
            stats_.out_waste_decay += e;
            stats_.energy_out += e;
            soil_.nutrient[c] += mass;
            m.clear();
            soil_.vent_made[c] = 0.0f;
            leaver_[c] = NO_LEAVER;
            left_[c] = 0.0f;
            return;
        }
        const Mix part = m.scaled(WASTE_SPREAD * 0.25f);
        const float tag_part = leaver_[c] != NO_LEAVER ? left_[c] * (WASTE_SPREAD * 0.25f) : 0.0f;
        float& vented = soil_.vent_made[c];
        const float vent_part = vented * (WASTE_SPREAD * 0.25f);
        const int nb[4] = {x > 0 ? c - 1 : -1, x + 1 < n ? c + 1 : -1, y > 0 ? c - n : -1, y + 1 < n ? c + n : -1};
        for (int k : nb)
            if (k >= 0) {
                spread_[k].add(part);
                m.add(part, -1.0f);
                spread_vent_[k] += vent_part;
                vented -= vent_part;
                soil_.mark_loose(static_cast<std::size_t>(k));
                if (tag_part > 0.0f) {   // the leavings tag drifts too (observation only)
                    left_[c] -= tag_part;
                    if (tag_in_[k].species == leaver_[c]) tag_in_[k].amount += tag_part;
                    else if (tag_part > tag_in_[k].amount) tag_in_[k] = {leaver_[c], tag_part};
                }
            }
    });
    Soil::each(soil_.loose_bits, [&](std::size_t c) {
        Mix& m = soil_.dissolved[c];
        m.add(spread_[c]);
        // The vent's share drifts and breaks down with the rest, and never
        // exceeds what is there.
        float& vented = soil_.vent_made[c];
        vented = std::min((vented + spread_vent_[c]) * (1.0f - WASTE_DECAY), m.v[VENT_CHEM] * (1.0f - WASTE_DECAY));
        spread_[c] = Mix{};
        spread_vent_[c] = 0.0f;
        const Mix gone = m.scaled(WASTE_DECAY);
        m.add(gone, -1.0f);
        const double e = gone.energy();
        stats_.out_waste_decay += e;
        stats_.energy_out += e;
        soil_.nutrient[c] += gone.mass();
        soil_.waste[c] = m.energy();
        if (tag_in_[c].amount > 0.0f) {
            tag_leavings(static_cast<int>(c), tag_in_[c].species, tag_in_[c].amount);
            tag_in_[c] = TagIn{};
        }
        left_[c] = std::min(left_[c] * (1.0f - WASTE_DECAY), soil_.waste[c]);
        if (left_[c] <= 0.0f) leaver_[c] = NO_LEAVER;
        bool any = vented != 0.0f;
        for (float v : m.v) any = any || v != 0.0f;
        if (!any) Soil::unmark(soil_.loose_bits, c);
    });
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
    // The mineral P washes downhill the same way (stoichiometry).
    wash_.assign(cells, 0.0f);
    for (std::size_t c = 0; c < cells; ++c) {
        const int low = world_.downhill(static_cast<int>(c));
        if (low < 0 || soil_.mineral[c] <= 0.0f) continue;
        const float moved = soil_.mineral[c] * std::min(cfg_.wash_rate * world_.steepness(static_cast<int>(c)), 1.0f);
        soil_.mineral[c] -= moved;
        wash_[static_cast<std::size_t>(low)] += moved;
    }
    for (std::size_t c = 0; c < cells; ++c) soil_.mineral[c] += wash_[c];
}

void Simulation::take_census() {
    Census k;
    for (std::size_t i = 0; i < c_.size(); ++i) {
        const Phenotype& p = c_.pheno[i];
        if (p.has_brain && p.max_speed > 0.0f) ++k.movers;
        if (p.sense_cells > 0) ++k.sighted;
        if (p.part_total[static_cast<int>(PartType::Armour)] > 0.0f) ++k.armoured;
        if (p.toxicity > 0.0f) ++k.poisonous;
        if (p.grown < 1.0f) ++k.growing;
        const float l = c_.fed_light[i], v = c_.fed_living[i], d = c_.fed_dead[i], w = c_.fed_waste[i], h = c_.fed_vent[i];
        const float total = l + v + d + w + h;
        if (total < 20.0f)           ++k.young;
        else if (l > 0.6f * total)   ++k.producers;
        else if (v > 0.6f * total)   ++k.hunters;
        else if (d > 0.6f * total)   ++k.scavengers;
        else if (w > 0.6f * total)   ++k.cleaners;
        else if (h > 0.6f * total)   ++k.chemotrophs;
        else                         ++k.mixed;
    }
    census_ = k;
    take_habits();
}

// What each living species is like right now, for its profile in the viewer.
// Species with no members keep their last word. Observation only.
void Simulation::take_habits() {
    struct Sum {
        double count = 0, fed = 0, diet[5] = {}, movers = 0, sighted = 0, sleepers = 0, armoured = 0, poisonous = 0;
        double radius = 0, age = 0, speed = 0, light = 0, temp = 0, height = 0, x = 0, y = 0;
    };
    std::vector<Sum> sum(species_.all().size());
    const auto& lights = world_.lights();
    const auto& temps = world_.temperatures();
    const auto& heights = world_.heights();
    for (std::size_t i = 0; i < c_.size(); ++i) {
        Sum& s = sum[c_.species[i]];
        const Phenotype& p = c_.pheno[i];
        const int cell = world_.cell_index(c_.x[i], c_.y[i]);
        s.count += 1;
        const double food[5] = {c_.fed_light[i], c_.fed_living[i], c_.fed_dead[i], c_.fed_waste[i], c_.fed_vent[i]};
        const double total = food[0] + food[1] + food[2] + food[3] + food[4];
        if (total >= 20.0) {
            s.fed += 1;
            for (int k = 0; k < 5; ++k) s.diet[k] += food[k] / total;
        }
        const bool mover = p.has_brain && p.max_speed > 0.0f;
        s.movers += mover ? 1 : 0;
        s.speed += mover ? p.max_speed : 0.0f;
        s.sighted += p.sense_cells > 0 ? 1 : 0;
        s.sleepers += p.dormancy > 0.1f ? 1 : 0;
        s.armoured += p.part_total[static_cast<int>(PartType::Armour)] > 0.0f ? 1 : 0;
        s.poisonous += p.toxicity > 0.0f ? 1 : 0;
        s.radius += c_.genome[i].core_radius;
        s.age += c_.age[i];
        s.light += lights[cell];
        s.temp += temps[cell];
        s.height += heights[cell];
        s.x += c_.x[i];
        s.y += c_.y[i];
    }
    for (std::size_t sp = 0; sp < sum.size(); ++sp) {
        const Sum& s = sum[sp];
        if (s.count == 0) continue;
        Habits& h = species_.habits(static_cast<uint32_t>(sp));
        const double n = s.count;
        h.count = static_cast<uint32_t>(n);
        h.tick = tick_;
        for (int k = 0; k < 5; ++k) h.diet[k] = s.fed > 0 ? static_cast<float>(s.diet[k] / s.fed) : 0.0f;
        h.movers = static_cast<float>(s.movers / n);
        h.speed = s.movers > 0 ? static_cast<float>(s.speed / s.movers) : 0.0f;
        h.sighted = static_cast<float>(s.sighted / n);
        h.sleepers = static_cast<float>(s.sleepers / n);
        h.armoured = static_cast<float>(s.armoured / n);
        h.poisonous = static_cast<float>(s.poisonous / n);
        h.radius = static_cast<float>(s.radius / n);
        h.age = static_cast<float>(s.age / n);
        h.light = static_cast<float>(s.light / n);
        h.temperature = static_cast<float>(s.temp / n);
        h.height = static_cast<float>(s.height / n);
        h.x = static_cast<float>(s.x / n);
        h.y = static_cast<float>(s.y / n);
        h.spread = 0.0f;
    }
    // A second pass for how widely each species ranges.
    for (std::size_t i = 0; i < c_.size(); ++i) {
        Habits& h = species_.habits(c_.species[i]);
        h.spread += std::hypot(c_.x[i] - h.x, c_.y[i] - h.y) / static_cast<float>(h.count);
    }
}

// -----------------------------------------------------------------------------
// Species splits (design doc 9.3 step 3). A species founded by one drifting
// newborn can go on to become two kinds of creature without any single
// newborn crossing the threshold: either side of a mountain, or two ways of
// life side by side. Look inside each big species for two clusters (a
// two-centre clustering on a sample, centres being sample members), and if
// they are well apart, make the one further from the founder a new species.
// Deterministic: samples are taken by stride over the creature arrays.
// -----------------------------------------------------------------------------
void Simulation::check_splits() {
    const std::size_t n = c_.size();
    const std::size_t kinds = species_.all().size();   // splits below add to the list; look only at these
    if (n == 0) return;

    // Members of each species, in creature order (a counting sort).
    std::vector<uint32_t> start(kinds + 1, 0), members(n);
    for (std::size_t i = 0; i < n; ++i) ++start[c_.species[i] + 1];
    for (std::size_t k = 0; k < kinds; ++k) start[k + 1] += start[k];
    {
        std::vector<uint32_t> at(start.begin(), start.end() - 1);
        for (std::size_t i = 0; i < n; ++i) members[at[c_.species[i]]++] = static_cast<uint32_t>(i);
    }

    std::vector<uint32_t> sample;
    std::vector<float> dist;          // sample x sample distances
    std::vector<uint8_t> side;
    for (uint32_t sp = 0; sp < kinds; ++sp) {
        const uint32_t count = start[sp + 1] - start[sp];
        if (count < 2 * SPLIT_MIN || !species_.get(sp).established) continue;
        const uint32_t* m = members.data() + start[sp];

        // A sample spread evenly over the members.
        const std::size_t k = std::min<std::size_t>(count, SPLIT_SAMPLE);
        sample.resize(k);
        for (std::size_t j = 0; j < k; ++j) sample[j] = m[j * count / k];
        dist.assign(k * k, 0.0f);
        for (std::size_t a = 0; a < k; ++a)
            for (std::size_t b = a + 1; b < k; ++b)
                dist[a * k + b] = dist[b * k + a] = genome_distance(c_.genome[sample[a]], c_.genome[sample[b]]);
        auto farthest = [&](std::size_t from) {
            std::size_t best = from;
            for (std::size_t j = 0; j < k; ++j) if (dist[from * k + j] > dist[from * k + best]) best = j;
            return best;
        };
        std::size_t ca = farthest(0), cb = farthest(ca);
        if (dist[ca * k + cb] < SPLIT_GAP) continue;

        // Two centres: assign each to the nearer, move each centre to the
        // member with the least total distance to its own side, repeat.
        side.assign(k, 0);
        for (int round = 0; round < SPLIT_ROUNDS; ++round) {
            for (std::size_t j = 0; j < k; ++j) side[j] = dist[j * k + cb] < dist[j * k + ca] ? 1 : 0;
            for (int s = 0; s < 2; ++s) {
                float best = 1e30f;
                std::size_t centre = s ? cb : ca;
                for (std::size_t j = 0; j < k; ++j) {
                    if (side[j] != s) continue;
                    float sum = 0.0f;
                    for (std::size_t q = 0; q < k; ++q) if (side[q] == s) sum += dist[j * k + q];
                    if (sum < best) { best = sum; centre = j; }
                }
                (s ? cb : ca) = centre;
            }
        }
        std::size_t on_b = 0;
        float spread = 0.0f;
        for (std::size_t j = 0; j < k; ++j) {
            side[j] = dist[j * k + cb] < dist[j * k + ca] ? 1 : 0;
            on_b += side[j];
            spread += dist[j * k + (side[j] ? cb : ca)];
        }
        spread /= static_cast<float>(k);
        const float gap = dist[ca * k + cb];
        const float min_side = SPLIT_SHARE * static_cast<float>(k);
        if (static_cast<float>(on_b) < min_side || static_cast<float>(k - on_b) < min_side) continue;
        if (gap < SPLIT_GAP || gap < SPLIT_SEPARATION * spread) continue;

        // The side further from the founder leaves.
        const Genome& founder = species_.get(sp).representative;
        if (genome_distance(c_.genome[sample[ca]], founder) > genome_distance(c_.genome[sample[cb]], founder)) std::swap(ca, cb);
        const Genome stay = c_.genome[sample[ca]];
        const Genome leave = c_.genome[sample[cb]];

        // Every member goes to the nearer centre; both sides must stay viable.
        std::vector<uint32_t> leavers;
        float sx[2] = {}, sy[2] = {};
        for (uint32_t j = 0; j < count; ++j) {
            const uint32_t i = m[j];
            const bool go = genome_distance(c_.genome[i], leave) < genome_distance(c_.genome[i], stay);
            if (go) leavers.push_back(i);
            sx[go] += c_.x[i];
            sy[go] += c_.y[i];
        }
        const uint32_t gone = static_cast<uint32_t>(leavers.size());
        if (gone < SPLIT_MIN || count - gone < SPLIT_MIN) continue;

        const std::string old_name = species_.get(sp).name;
        const uint32_t to = species_.split_off(sp, leave, tick_);
        for (uint32_t i : leavers) {
            species_.move_member(sp, to);
            c_.species[i] = to;
        }
        const std::string new_name = species_.get(to).name;
        const std::string here = region_name(world_, sx[0] / static_cast<float>(count - gone), sy[0] / static_cast<float>(count - gone));
        const std::string there = region_name(world_, sx[1] / static_cast<float>(gone), sy[1] / static_cast<float>(gone));
        const std::string how = describe_difference(leave, stay);
        std::string text = old_name + " splits in two";
        text += here == there ? " in " + here + ": " + new_name + " goes its own way"
                              : ": " + new_name + " in " + there + ", " + old_name + " in " + here;
        text += how.empty() ? "." : ". " + new_name + " " + how + ".";
        chronicle_.add(tick_, text);
        alert(sp, "Your " + text);
    }
}

// -----------------------------------------------------------------------------
// Following a species (design doc 4.8): observation only.
// -----------------------------------------------------------------------------
void Simulation::follow(uint32_t species, bool on) {
    if (species >= species_.all().size()) return;
    auto it = std::find_if(watch_.begin(), watch_.end(), [&](const Watch& w) { return w.species == species; });
    if (!on) {
        if (it != watch_.end()) watch_.erase(it);
        return;
    }
    if (it != watch_.end()) return;
    watch_.push_back({species, survey(species, 0)});
}

bool Simulation::following(uint32_t species) const {
    return std::any_of(watch_.begin(), watch_.end(), [&](const Watch& w) { return w.species == species; });
}

std::vector<uint32_t> Simulation::followed() const {
    std::vector<uint32_t> ids;
    for (const Watch& w : watch_) ids.push_back(w.species);
    return ids;
}

void Simulation::alert(uint32_t species, const std::string& text) {
    if (following(species)) alerts_.add(tick_, text);
}

Simulation::Survey Simulation::survey(uint32_t species, uint32_t old_traits) const {
    Survey v;
    uint32_t have[MOVING_TRAIT + 1] = {};
    double mass = 0.0;
    for (std::size_t i = 0; i < c_.size(); ++i) {
        if (c_.species[i] != species) continue;
        const Phenotype& p = c_.pheno[i];
        ++v.count;
        for (int t = 0; t < MOVING_TRAIT; ++t) have[t] += p.part_total[t] > 0.0f;
        have[MOVING_TRAIT] += p.has_brain && p.max_speed > 0.0f;
        mass += p.mass / p.grown;
    }
    v.traits = old_traits;
    if (v.count == 0) return v;
    v.mass = static_cast<float>(mass / v.count);
    for (int t = 0; t <= MOVING_TRAIT; ++t) {
        const float share = static_cast<float>(have[t]) / static_cast<float>(v.count);
        if (share >= TRAIT_ON) v.traits |= 1u << t;
        else if (share <= TRAIT_OFF) v.traits &= ~(1u << t);
    }
    return v;
}

void Simulation::watch_followed() {
    static const char* const parts[MOVING_TRAIT] = {"sun patches", "mouths", "tails", "legs",
                                                    "eyes", "armour", "fat stores", "poison"};
    for (Watch& w : watch_) {
        const Species& s = species_.get(w.species);
        if (!s.alive()) continue;
        const Survey now = survey(w.species, w.seen.traits);
        const std::string your = "Your " + s.name;
        for (int t = 0; t <= MOVING_TRAIT; ++t) {
            const bool had = w.seen.traits >> t & 1u, has = now.traits >> t & 1u;
            if (had == has) continue;
            if (t == MOVING_TRAIT) alerts_.add(tick_, has ? your + " have begun to move." : your + " no longer move.");
            else alerts_.add(tick_, has ? "Most of your " + s.name + " now have " + parts[t] + "."
                                        : your + " have lost their " + parts[t] + ".");
        }
        w.seen.traits = now.traits;
        if (w.seen.mass > 0.0f && now.count >= WATCH_MIN && std::abs(now.mass / w.seen.mass - 1.0f) >= SIZE_ALERT) {
            const int pct = static_cast<int>(std::lround(100.0f * std::abs(now.mass / w.seen.mass - 1.0f)));
            alerts_.add(tick_, your + (now.mass > w.seen.mass ? " are getting bigger: " : " are getting smaller: ") +
                                   std::to_string(pct) + (now.mass > w.seen.mass ? "% heavier" : "% lighter") +
                                   " than at the last report.");
            w.seen.mass = now.mass;
        } else if (w.seen.mass <= 0.0f) {
            w.seen.mass = now.mass;
        }
        if (2 * now.count <= w.seen.count) {
            alerts_.add(tick_, your + " are dwindling: " + with_commas(now.count) + " left, from " + with_commas(w.seen.count) + ".");
            w.seen.count = now.count;
        } else if (now.count >= 2 * w.seen.count && now.count >= WATCH_MIN) {
            alerts_.add(tick_, your + " are thriving: " + with_commas(now.count) + " alive.");
            w.seen.count = now.count;
        } else if (w.seen.count == 0) {
            w.seen.count = now.count;
        }
    }
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
    first(chemotrophs_logged_, census_.chemotrophs, "Life at the vents: some creatures now live mostly on what the vents make.");

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
