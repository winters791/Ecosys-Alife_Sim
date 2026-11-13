#include "phenotype.hpp"
#include "laws.hpp"

#include "body.hpp"
#include "energy.hpp"

#include <algorithm>
#include <cmath>

namespace luca {

namespace {  // only visible inside this file

constexpr int NUM_PARTS = static_cast<int>(PartType::Count);

// Order MUST match the PartType enum in genome.hpp:
//                                           Photo  Absorber Flagellum Legs  Sensor Armour Storage Poison
constexpr float MASS_PER_SIZE[NUM_PARTS] = { 0.1f,  0.2f,    0.15f,    0.3f, 0.05f, 0.6f,  0.4f,   0.1f };
constexpr float COST_PER_SIZE[NUM_PARTS] = { 0.1f,  0.15f,   0.2f,     0.2f, 0.05f, 0.05f, 0.02f,  0.12f };
// Stoichiometry: P a part needs per unit of size, on top of P_BASE per unit of
// body mass, as units of mass worth of P (times P_PART_SCALE). Shells and bone
// are mineral; leaves and fat are mostly carbon (docs/design/stoichiometry.md).
constexpr float P_PER_SIZE[NUM_PARTS]    = { 0.0f,  0.5f,    0.5f,     1.5f, 0.5f,  2.0f,  0.0f,   0.2f };
// Sensor upkeep was 0.3 per unit of size until 2026-11-11, the dearest part
// there is: seeing one cell cost 0.15 a tick, ~40% of a LUCA's whole upkeep,
// so movers went blind and their brains were a fixed wander urge.

inline const float& BASE_UPKEEP    = g_laws.BASE_UPKEEP;  // law, 0.3f
inline const float& PHOTO_PER_AREA = g_laws.PHOTO_PER_AREA;  // law, 1.0f: max photo patch size per unit of skin

// Thrust was 0.5 / 0.4 before anything moved; at 60 ticks a second that crossed
// the map in five seconds. These give a one-flagellum LUCA about 6 cells/s.
inline const float& FLAGELLUM_THRUST = g_laws.FLAGELLUM_THRUST;  // law, 0.125f: world units per tick, per unit size, per unit mass
inline const float& LEGS_THRUST      = g_laws.LEGS_THRUST;  // law, 0.10f
inline const float& MAX_SPEED        = g_laws.MAX_SPEED;  // law, 0.5f: hard physical ceiling (a law, not a population cap)
// MOVER_SUN (movers caught a share of the light, moving or not) was replaced
// by folding (UNFOLD_TICKS, energy.hpp) and deleted in step 0.5 (14.10.8).

// Contact. MEMBRANE_BASE, ARMOUR_RESIST and ARMOUR_CLOGS are in body.hpp:
// where a plate sits decides what it protects and what it clogs.
inline const float& ARMOUR_SHADES   = g_laws.ARMOUR_SHADES;  // law, 0.5f: skin under a plate catches no light
inline const float& REACH_PER_RADIUS = g_laws.REACH_PER_RADIUS;  // law, 0.5f
// Poison: light to carry but dear to keep up (see the table above). Unlike
// armour it does not slow the eater down; it makes the meal cost more than it gives.
inline const float& TOXICITY_PER_SIZE = g_laws.TOXICITY_PER_SIZE;  // law, 1.0f: energy lost per unit drained, per unit of poison part
inline const float& MAX_TOXICITY      = g_laws.MAX_TOXICITY;  // law, 2.0f

// Storage and building.
inline const float& CAPACITY_PER_VOLUME = g_laws.CAPACITY_PER_VOLUME;  // law, 650.0f: energy a core blob of radius 1 can hold (450 before bodies cost 244 to build)
inline const float& CAPACITY_PER_STORE  = g_laws.CAPACITY_PER_STORE;  // law, 600.0f: extra per unit of storage part
// A body is worth eating: a LUCA (mass 1.1) takes 244 to build, and leaves
// that much behind as a corpse. It was 40 (a 44-energy corpse nobody bothered with).

// Behaviour.
inline const float& SENSE_CELLS_PER_SIZE = g_laws.SENSE_CELLS_PER_SIZE;  // law, 2.0f
constexpr int   MAX_SENSE_CELLS      = 4;
// Thinking is nearly free (2026-11-12, Uday's call): all four brain costs
// are a hundredth of what they were (0.01 / 0.012 / 0.002 / 0.002), so a full
// brain (8 neurons, 48 links, weights near 1) costs about 0.007 a tick, about
// 2% of a LUCA's upkeep. Not zero, so dead wiring still drifts away; the size
// caps (MAX_HIDDEN, MAX_LINKS) bound it.
inline const float& BRAIN_COST           = g_laws.BRAIN_COST;  // law, 0.0001f: upkeep per unit of total weight
inline const float& NEURON_COST          = g_laws.NEURON_COST;  // law, 0.00012f: upkeep per hidden neuron
inline const float& LINK_COST            = g_laws.LINK_COST;  // law, 0.00002f: upkeep per link, however weak
// A slow neuron holds on to what it felt: memory, which costs upkeep per
// e-fold of its time constant (a neuron with tau 1000 pays about 0.00014
// more, a little over one more neuron). Its bias is paid for like a weight.
inline const float& MEMORY_COST          = g_laws.MEMORY_COST;  // law, 0.00002f
inline const float& TOLERANCE_COST       = g_laws.TOLERANCE_COST;  // law, 0.004f: upkeep per °C of tolerance: generalists pay

// Dispersal: the means to fling a child costs upkeep whether or not it is used
// (each throw also costs energy; see throw_cost in energy.cpp).
inline const float& THROW_UPKEEP         = g_laws.THROW_UPKEEP;  // law, 0.015f: upkeep per cell of range

// Shape (body.hpp): skin beyond a ball's costs upkeep to keep.
inline const float& SKIN_UPKEEP          = g_laws.SKIN_UPKEEP;  // law, 0.1f: upkeep per unit of extra skin (in r^2)

// Stalks (mouths and sun patches only, genome.hpp): mass and upkeep per world
// unit of length, whatever the part's size. A stalked mouth reaches further
// and is never clogged by armour (body.hpp); stalked sun patches stand higher
// in the canopy.
inline const float& STALK_MASS   = g_laws.STALK_MASS;  // law, 0.1f

inline const float& STALK_UPKEEP = g_laws.STALK_UPKEEP;  // law, 0.06f

// Feeding specialisation: pure cost to a body that eats nothing.
inline const float& BITE_UPKEEP          = g_laws.BITE_UPKEEP;  // law, 0.06f: upkeep per unit of bite (each unit adds the plain pull again)

// Dormancy: the machinery for shutting down costs a little all the time.
// Its real price is paid in energy.cpp: a sleeper is helpless and slow to wake.
inline const float& DORMANCY_UPKEEP      = g_laws.DORMANCY_UPKEEP;  // law, 0.02f: upkeep at full depth
// Cold nights (added by Claude, 2026-11-11): a body holds heat for mass /
// surface area x HEAT_LAG ticks (heat is stored in bulk and lost through skin).
// A LUCA lags the air by ~130 ticks, so it feels about 60% of the night's
// chill; a body of radius 2 feels about 30%; a small thin one nearly all of it.
// Not a new gene: size and fat already cost upkeep.
inline const float& HEAT_LAG             = g_laws.HEAT_LAG;  // law, 120.0f
// Enzymes (chemistry layer): upkeep per unit of rate, paid whether or not
// there is anything to turn over. LUCA's 0.015 costs 0.015 (4% of its upkeep).
inline const float& ENZYME_UPKEEP        = g_laws.ENZYME_UPKEEP;  // law, 1.0f
// 2.8 B, screening only: the share of standing gear's upkeep (mouths, motors,
// senses, bite, brain, enzymes) a body pays. 1 is the rule as it was; below 1
// gear is cheaper to keep and nothing charges for using it, so it is an upper
// bound on what splitting resting from working costs could give, not the rule.
inline const float& GEAR_STANDING        = g_laws.GEAR_STANDING;  // law, 1.0f

// Gene ranges live in genome.hpp (limits::); mutation enforces them through
// clamp_genome(). The clamps below are a second line of defence for genomes
// that arrive from outside (creature files).
using limits::MIN_RADIUS;
using limits::MAX_RADIUS;

bool thrust_possible(float flagella, float legs) { return flagella + legs > 0.0f; }

}  // namespace

// =============================================================================
// THE PHENOTYPE COMPILER (Uday's)
//
// Done:
//   1. Clamping (radius, part sizes, pigment, reproduction, thermal tolerance)
//   2. Body scaling: mass ∝ r³ (+ parts), surface area ∝ r²
//   3. Upkeep: Kleiber's law on total mass + per-part running costs
//   4. Light capture: photo patch total, capped by available skin
//   5. Max speed: thrust from flagella + legs, divided by mass, capped
//   6. Heat absorption: pigment × surface area / mass (small bodies swing more)
//
//   7. (added by Claude, 2026-11-11, at Uday's request) wide thermal tolerance
//      and brain weights cost upkeep; contact stats (absorb / membrane / reach);
//      energy capacity; the energy locked in the body; sensor range; brain copy
//
// Later:
//   * Split max_speed into swim_speed (flagella) and walk_speed (legs) once
//     the world has water
//
// Rule of thumb: every number you set here should have a cost somewhere.
// =============================================================================

Phenotype compile_phenotype(const Genome& g, float grown) {
    Phenotype p;

    // --- 1. Clamp genes that mutation can push out of range -----------------
    const float r       = std::clamp(g.core_radius, MIN_RADIUS, MAX_RADIUS);
    const float pigment = std::clamp(g.pigment, 0.0f, 1.0f);

    // --- 2 + 3. One pass over the parts: mass, upkeep, per-type totals -------
    float mass            = r * r * r;  // core blob
    float part_upkeep     = 0.0f;
    float photo_total     = 0.0f;
    float flagellum_total = 0.0f;
    float legs_total      = 0.0f;
    float thrust          = 0.0f;  // motor parts, by how well placed and how bare they are
    float pull            = 0.0f;  // mouths, by how bare they are
    float armour_cover    = 0.0f;  // armour averaged over every direction
    float photo_lift      = 0.0f;  // photo size x stalk, summed
    float stalk_upkeep    = 0.0f;
    float gear            = 0.0f;  // observation: upkeep of standing gear
    float p_parts         = 0.0f;  // stoichiometry: extra P the parts need

    for (const Part& part : g.parts) {
        const int   t    = static_cast<int>(part.type);
        const float size = std::max(0.0f, part.size);  // negative parts would be free energy
        const float stalk = can_have_stalk(part.type) ? std::clamp(part.stalk, 0.0f, limits::MAX_STALK) : 0.0f;
        mass        += size * MASS_PER_SIZE[t] + stalk * STALK_MASS;
        p_parts     += size * P_PER_SIZE[t];
        part_upkeep += size * COST_PER_SIZE[t];
        if (part.type == PartType::Absorber || part.type == PartType::Flagellum || part.type == PartType::Legs ||
            part.type == PartType::Sensor)
            gear += size * COST_PER_SIZE[t];
        stalk_upkeep += stalk * STALK_UPKEEP;
        if (part.type == PartType::Photo) photo_lift += size * stalk;

        p.part_total[t] += size;

        if (part.type == PartType::Photo)     photo_total     += size;
        if (part.type == PartType::Flagellum) flagellum_total += size;
        if (part.type == PartType::Legs)      legs_total      += size;
        if (part.type == PartType::Flagellum || part.type == PartType::Legs)
            thrust += size * (part.type == PartType::Flagellum ? FLAGELLUM_THRUST : LEGS_THRUST)
                    * thrust_facing(part.type, part.theta) * clogged(g, part);
        if (part.type == PartType::Absorber) pull += size * clogged(g, part);
        if (part.type == PartType::Armour)   armour_cover += size * cover_halfwidth(size) / 3.14159265f;
    }
    const float sensor_total   = p.part_total[static_cast<int>(PartType::Sensor)];
    const float armour_total   = p.part_total[static_cast<int>(PartType::Armour)];
    const float storage_total  = p.part_total[static_cast<int>(PartType::Storage)];
    const float tolerance      = std::max(0.0f, g.thermal_tolerance);
    const float seed_throw     = std::clamp(g.seed_throw, 0.0f, limits::MAX_THROW);
    const float bite           = std::clamp(g.bite, 0.0f, limits::MAX_BITE);
    const float dormancy       = std::clamp(g.dormancy, 0.0f, 1.0f);

    // A brain only exists if there is something for it to drive.
    // Its price grows with its size: neurons, links and the strength of the links.
    p.has_brain = thrust_possible(flagellum_total, legs_total) && !g.brain.empty();
    float brain_upkeep = 0.0f;
    if (p.has_brain) {
        const int hidden = std::min<int>(g.brain.hidden, MAX_HIDDEN);
        brain_upkeep = NEURON_COST * static_cast<float>(hidden);
        for (int k = 0; k < hidden; ++k)
            brain_upkeep += MEMORY_COST * std::log(std::clamp(g.brain.tau[k], limits::MIN_TAU, limits::MAX_TAU))
                          + BRAIN_COST * std::min(std::abs(g.brain.bias[k]), limits::MAX_WEIGHT);
        const std::size_t links = std::min(g.brain.links.size(), static_cast<std::size_t>(MAX_LINKS));
        for (std::size_t k = 0; k < links; ++k)
            brain_upkeep += LINK_COST + BRAIN_COST * std::min(std::abs(g.brain.links[k].weight), limits::MAX_WEIGHT);
    }

    p.mass         = mass;
    p.p_adult      = g_laws.P_BASE > 0.0f ? g_laws.P_BASE * (mass + g_laws.P_PART_SCALE * p_parts) : 0.0f;
    p.p_need       = p.p_adult;
    p.p_store_cap  = g_laws.P_STORE * p.p_adult;
    p.radius       = r;
    p.canopy       = r + (photo_total > 0.0f ? photo_lift / photo_total : 0.0f);   // patches held up on stalks overtop their neighbours
    p.throw_range  = seed_throw;
    p.dormancy     = dormancy;
    const float skin = skin_factor(g.elongation, g.squareness);
    p.surface_area = r * r * skin;
    p.drag         = drag_factor(g.elongation, g.squareness);
    p.upkeep       = BASE_UPKEEP * std::pow(mass, 0.75f) + part_upkeep  // Kleiber + parts
                   + (skin - 1.0f) * r * r * SKIN_UPKEEP + stalk_upkeep
                   + tolerance * TOLERANCE_COST + brain_upkeep + seed_throw * THROW_UPKEEP
                   + bite * BITE_UPKEEP + dormancy * DORMANCY_UPKEEP;
    // Chemistry: compile the enzymes, each written the richer side first. An
    // enzyme with equal levels on both sides releases and stores nothing, so
    // it never runs (neutral exchanges were deleted after the ablation pass,
    // 14.10.8); it still costs upkeep.
    float enzyme_rate = 0.0f;
    {
        Enzyme es[limits::MAX_ENZYMES];
        int ne = 0;
        for (const Enzyme& e : g.enzymes) {
            if (ne >= limits::MAX_ENZYMES) break;
            es[ne++] = canonical_enzyme(e);
        }
        auto level = [](uint8_t c) { return c == NO_CHEM ? 0.0f : LEVEL[c]; };
        auto bit = [](uint8_t c) { return c == NO_CHEM || c == CHEM_A ? 0u : 1u << c; };
        for (int k = 0; k < ne; ++k) {
            const Enzyme& e = es[k];
            const float rate = std::clamp(e.rate, limits::MIN_ENZYME_RATE, limits::MAX_ENZYME_RATE);
            const float step = level(e.y) + level(e.y2) - level(e.x) - level(e.x2);
            const int j = p.enzymes;
            p.enz_in[j][0]  = e.y;   // the richer side, burned from
            p.enz_in[j][1]  = e.y2;
            p.enz_out[j][0] = e.x;
            p.enz_out[j][1] = e.x2;
            p.enz_step[j] = std::max(0.0f, step);
            p.enz_rate[j] = rate;
            if (step > 0.0f) p.burns |= static_cast<uint8_t>(bit(p.enz_in[j][0]) | bit(p.enz_in[j][1]));
            p.any_two = p.any_two || e.two();
            ++p.enzymes;
            enzyme_rate += rate;
        }
    }
    p.upkeep += enzyme_rate * ENZYME_UPKEEP;
    p.gear_upkeep = gear + bite * BITE_UPKEEP + brain_upkeep + enzyme_rate * ENZYME_UPKEEP;
    p.upkeep -= (1.0f - GEAR_STANDING) * p.gear_upkeep;
    p.gear_upkeep *= GEAR_STANDING;
    // LUCA (r = 1, one photo patch of size 1): mass 1.1, upkeep ≈ 0.42 + 0.04 for its ±10 °C.

    // --- 4. Light capture: patches need skin to sit on, and plates cover skin
    const float open_skin = std::max(0.0f, PHOTO_PER_AREA * p.surface_area - ARMOUR_SHADES * armour_total);
    p.light_capture = std::min(photo_total, open_skin);

    // --- 5. Movement: thrust / mass, with a physical ceiling ----------------
    // A tail pushes best from the back and legs from the sides (body.hpp).
    p.max_speed = std::min(thrust / (p.mass * p.drag), MAX_SPEED);
    // LUCA + one size-1 flagellum straight behind: mass 1.25, speed 0.1.

    // --- 6. Heat: absorbed over skin, spread over body mass ------------------
    p.heat_absorption = pigment * p.surface_area / p.mass;
    p.skin = skin_colour(g.hue, pigment);
    // LUCA: 0.3 × 1 / 1.1 ≈ 0.27.
    // Heat is held in bulk and lost through skin: bigger and fatter bodies lag the air.
    p.heat_follow = 1.0f / (1.0f + HEAT_LAG * p.mass / p.surface_area);

    // --- 7. Copy-through genes (clamped) -------------------------------------
    p.offspring_share   = std::clamp(g.offspring_share, limits::MIN_SHARE, limits::MAX_SHARE);
    p.thermal_optimum   = g.thermal_optimum;
    p.thermal_tolerance = tolerance;

    // --- 8. Contact: pull, resistance, reach --------------------------------
    // Totals over the whole body. Contact with the living reads the
    // per-direction tables in BodyPlan instead (body.hpp); the dead lie all
    // round, so feeding on corpses uses the total.
    p.absorb   = pull;
    p.membrane = MEMBRANE_BASE + ARMOUR_RESIST * armour_cover;   // averaged over every direction
    p.reach    = REACH_PER_RADIUS * r;
    p.toxicity = std::min(TOXICITY_PER_SIZE * p.part_total[static_cast<int>(PartType::Poison)], MAX_TOXICITY);
    p.bite       = 1.0f + bite;
    p.efficiency = TRANSFER_EFFICIENCY;   // the same for all since chemistry stage 3 (no digestion gene)

    // --- 9. Storage: a body can only hold so much, and can't save up more
    //        than it can hold before dividing --------------------------------
    p.energy_capacity = CAPACITY_PER_VOLUME * r * r * r + CAPACITY_PER_STORE * storage_total;
    p.repro_threshold = std::min(std::clamp(g.repro_threshold, limits::MIN_THRESHOLD, limits::MAX_THRESHOLD),
                                 0.95f * p.energy_capacity);

    // --- 10. Senses -----------------------------------------------------------
    p.sense_cells = sensor_total > 0.0f
                        ? std::clamp(static_cast<int>(std::ceil(sensor_total * SENSE_CELLS_PER_SIZE)), 1, MAX_SENSE_CELLS)
                        : 0;

    // --- 11. Growing up (2026-11-12) ------------------------------------------
    // A young body is the adult scaled down: a fraction f of its mass, so its
    // length is f^(1/3) of the adult's and its skin f^(2/3). It holds and
    // locks up less, costs less to keep (Kleiber again), catches less light,
    // stands lower in the canopy, and loses heat faster. Its pull, armour and
    // speed are left alone: the size rule at the mouth (energy.cpp) already
    // makes a small body weak against a big one, in both directions.
    // It cannot breed until it is grown (sim.cpp).
    p.birth_size = std::clamp(g.birth_size, limits::MIN_BIRTH, limits::MAX_BIRTH);
    const float f = std::clamp(grown, limits::MIN_BIRTH, 1.0f);
    p.grown = f;
    if (f < 1.0f) {
        const float k = std::cbrt(f);
        p.mass            *= f;
        p.p_need          *= f;
        p.energy_capacity *= f;
        p.repro_threshold  = std::min(p.repro_threshold, 0.95f * p.energy_capacity);
        p.upkeep          *= std::pow(f, 0.75f);
        p.gear_upkeep     *= std::pow(f, 0.75f);
        p.radius          *= k;
        p.canopy          *= k;
        p.reach           *= k;
        p.surface_area    *= k * k;
        p.light_capture   *= k * k;
        p.heat_absorption  = pigment * p.surface_area / p.mass;
        p.heat_follow      = 1.0f / (1.0f + HEAT_LAG * p.mass / p.surface_area);
    }

    p.pool_capacity = p.energy_capacity / TOP_LEVEL;
    return p;
}

BrainPlan compile_brain(const Genome& g) {
    BrainPlan plan;
    const int hidden = std::min<int>(g.brain.hidden, MAX_HIDDEN);
    const int nodes = FIRST_HIDDEN + hidden;
    plan.hidden = static_cast<uint8_t>(hidden);
    for (const Part& part : g.parts) {
        if (part.type != PartType::Sensor) continue;
        const float range = g.core_radius + g_laws.SENSE_CELLS_PER_SIZE * part.size;
        const bool pair = std::abs(std::sin(part.theta)) > 0.05f;
        for (int side = 0; side < (pair ? 2 : 1) && plan.sensors < MAX_SENSOR_POINTS; ++side) {
            SensorPoint& s = plan.sensor[plan.sensors++];
            s.channel = part.channel;
            s.angle = side == 0 ? part.theta : -part.theta;
            s.range = range;
            s.size = part.size;
        }
    }
    for (int k = 0; k < hidden; ++k) {
        plan.rate[k] = 1.0f / std::clamp(g.brain.tau[k], limits::MIN_TAU, limits::MAX_TAU);
        plan.bias[k] = std::clamp(g.brain.bias[k], -limits::MAX_WEIGHT, limits::MAX_WEIGHT);
    }
    // Two passes, so links that feed hidden neurons come first. Anything
    // malformed (a genome from outside) is simply left out.
    for (int pass = 0; pass < 2; ++pass) {
        for (const BrainLink& l : g.brain.links) {
            if (plan.links >= MAX_LINKS) break;
            const bool from_ok = l.from < NUM_INPUTS || (l.from >= FIRST_HIDDEN && l.from < nodes);
            const bool feeds_hidden = l.to >= FIRST_HIDDEN && l.to < nodes;
            const bool feeds_output = l.to >= FIRST_OUTPUT && l.to < FIRST_OUTPUT + NUM_OUTPUTS;
            if (!from_ok || (pass == 0 ? !feeds_hidden : !feeds_output)) continue;
            plan.link[plan.links] = l;
            plan.link[plan.links].weight = std::clamp(l.weight, -limits::MAX_WEIGHT, limits::MAX_WEIGHT);
            ++plan.links;
        }
        if (pass == 0) plan.to_hidden = plan.links;
    }
    return plan;
}

void think(const BrainPlan& plan, BrainState& state, const float* inputs, float* outputs) {
    // A CTRNN, one Euler step a tick. Every hidden neuron fires
    // tanh(state + bias) from last tick's state; then all states move together,
    // each 1/tau of the way towards what its links feed it, so the order of
    // links never matters. With tau = 1 and no bias this is a plain recurrent
    // net: the state is just the sum of what it is fed.
    float fire[MAX_HIDDEN];
    for (int h = 0; h < plan.hidden; ++h) fire[h] = std::tanh(state.hidden[h] + plan.bias[h]);
    float feed[MAX_HIDDEN] = {};
    for (int k = 0; k < plan.to_hidden; ++k) {
        const BrainLink& l = plan.link[k];
        const float v = l.from < NUM_INPUTS ? inputs[l.from] : fire[l.from - FIRST_HIDDEN];
        feed[l.to - FIRST_HIDDEN] += v * l.weight;
    }
    for (int h = 0; h < plan.hidden; ++h) {
        state.hidden[h] += plan.rate[h] * (feed[h] - state.hidden[h]);
        fire[h] = std::tanh(state.hidden[h] + plan.bias[h]);   // what the outputs hear
    }

    for (int o = 0; o < NUM_OUTPUTS; ++o) outputs[o] = 0.0f;
    for (int k = plan.to_hidden; k < plan.links; ++k) {
        const BrainLink& l = plan.link[k];
        const float v = l.from < NUM_INPUTS ? inputs[l.from] : fire[l.from - FIRST_HIDDEN];
        outputs[l.to - FIRST_OUTPUT] += v * l.weight;
    }
    for (int o = 0; o < NUM_OUTPUTS; ++o) outputs[o] = std::clamp(outputs[o], -limits::MAX_WEIGHT, limits::MAX_WEIGHT);
}

}  // namespace luca
