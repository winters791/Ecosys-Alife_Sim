#include "energy.hpp"
#include "laws.hpp"

#include <algorithm>
#include <cmath>

namespace luca {

// =============================================================================
// THE ENERGY RULES
// Written by Claude on 2026-11-11 at Uday's request (they were placeholders).
// Every constant here is a law of this world: change one and every lineage
// feels it. They are first guesses that give a stable world, not gospel.
//
// Keep these cheap: they run for every creature, every tick.
// =============================================================================

namespace {

// One knob for the pace of life: upkeep from the phenotype compiler is
// multiplied by this. At 0.75 a LUCA burns ~0.35 a tick against at most 1 a tick
// of sunlight, and a night costs it ~95.
inline const float& METABOLIC_RATE = g_laws.METABOLIC_RATE;  // law, 0.75f: was 1.0; tuned 2026-11-11 for a fuller world (three-seed sweep)

// Energy gained per unit of light capture under full sun.
inline const float& PHOTO_EFFICIENCY = g_laws.PHOTO_EFFICIENCY;  // law, 1.0f

// A patch only works spread out and held still. A body that covered
// FULL_STRIDE or more last tick catches just (1 - MOVING_SHADE) of its light;
// in between it is proportional. This is what makes a lineage choose between
// staying put in the sun and going to find food.
inline const float& MOVING_SHADE = g_laws.MOVING_SHADE;  // law, 0.8f
// Was 0.08; 0.02 since wandering became a drifting direction (2026-11-11).
// At 0.08 a slow drifter kept 90% of its light, so roaming grazers lost
// almost nothing by moving and overgrazed the world to a quarter of its size.
inline const float& FULL_STRIDE  = g_laws.FULL_STRIDE;  // law, 0.005f: world units per tick

// Dormancy. A night is about 240 ticks of a 600-tick day, so at full depth a
// sleeper saves most of a night's upkeep and loses roughly the first hour and
// a half of morning light while it wakes.
inline const float& DORMANT_SAVING = g_laws.DORMANT_SAVING;  // law, 0.9f: fraction of upkeep saved at full depth
inline const float& FALL_ASLEEP    = g_laws.FALL_ASLEEP;  // law, 1.0f / 20.0f: sleep gained per tick while it is dark or too cold
inline const float& WAKE_UP        = g_laws.WAKE_UP;  // law, 1.0f / 90.0f: sleep lost per tick once it is light and warm enough

// Wear: upkeep is multiplied by 1 + wear x (age / AGE_SCALE)^2, where wear is 1
// for an unrepaired body and less with the repair gene (phenotype.cpp). Nothing
// dies "of old age"; an old body just costs more than it can earn, and its
// matter is freed.
inline const float& AGE_SCALE = g_laws.AGE_SCALE;  // law, 10000.0f: was 5000: a body now costs 244 to build, so it should last

// Dark skin under sun feels warmer: °C per unit of heat_absorption per unit light.
inline const float& PIGMENT_WARMTH = g_laws.PIGMENT_WARMTH;  // law, 8.0f
// Being this many °C outside the comfort range doubles upkeep; it grows as the square.
inline const float& THERMAL_SCALE = g_laws.THERMAL_SCALE;  // law, 8.0f

// Moving costs mass x (TRANSPORT_COST x distance + MOVE_COST x speed^2) a
// tick, and climbing costs mass x height gained. Coming downhill gives
// nothing back.
// The distance term (added 2026-11-11) is the price of getting anywhere:
// creeping a cell costs nearly as much as walking it. With only speed^2,
// going a distance d at speed v cost mass x MOVE_COST x v x d, so creeping
// was almost free and slow drifting plants out-competed real movers.
inline const float& TRANSPORT_COST = g_laws.TRANSPORT_COST;  // law, 2.0f: per unit of mass per world unit
inline const float& MOVE_COST  = g_laws.MOVE_COST;  // law, 3.0f
inline const float& CLIMB_COST = g_laws.CLIMB_COST;  // law, 1.0f

// Flinging a child: mass x distance^2, like moving, but paid once per birth.
inline const float& THROW_COST = g_laws.THROW_COST;  // law, 2.0f

// Fouling (see energy.hpp): per unit of loose chemical it can't burn, per
// unit of skin. The old waste rule's 0.0025 per unit of energy, at 222 a unit.
inline const float& FOUL_HARM      = g_laws.FOUL_HARM;  // law, 0.5f

// Contact.
inline const float& CONTACT_RATE = g_laws.CONTACT_RATE;  // law, 1.5f: energy per tick for a pull of 1 against no membrane
inline const float& CORPSE_RATE  = g_laws.CORPSE_RATE;  // law, 2.0f: the dead have no membrane left

// Size matters at the mouth: the pull is scaled by (taker radius / giver
// radius)^SIZE_POWER, up to SIZE_EDGE_MAX. Equal bodies are unchanged; a body
// half its victim's size pulls a quarter as hard, so being bigger than your
// neighbours is protection, and a small thing on a big host can only sip.
inline const float& SIZE_POWER    = g_laws.SIZE_POWER;  // law, 2.0f
inline const float& SIZE_EDGE_MAX = g_laws.SIZE_EDGE_MAX;  // law, 2.0f

}  // namespace

float light_demand(const Phenotype& p, const LocalEnv& env) {
    return PHOTO_EFFICIENCY * p.light_capture * env.light;
}

float light_while_moving(float moved) {
    return 1.0f - MOVING_SHADE * std::min(moved * (1.0f / FULL_STRIDE), 1.0f);
}

float next_sleep(const Phenotype& p, const LocalEnv& env, float body_temp, float sleep) {
    if (p.dormancy <= 0.0f) return 0.0f;
    const bool shut_down = env.light <= 0.0f || body_temp < p.thermal_optimum - p.thermal_tolerance;
    return shut_down ? std::min(sleep + FALL_ASLEEP, p.dormancy) : std::max(sleep - WAKE_UP, 0.0f);
}

float cost_while_asleep(float sleep) {
    return 1.0f - DORMANT_SAVING * sleep;
}

float upkeep_cost(const Phenotype& p, uint32_t age) {
    const float a = static_cast<float>(age) * (1.0f / AGE_SCALE);
    return METABOLIC_RATE * p.upkeep * (1.0f + p.wear * a * a);   // repair slows wear (phenotype.cpp)
}

float felt_temperature(const Phenotype& p, const LocalEnv& env) {
    return env.temperature + PIGMENT_WARMTH * p.heat_absorption * env.light;
}

float next_body_temp(const Phenotype& p, const LocalEnv& env, float body_temp) {
    return body_temp + p.heat_follow * (felt_temperature(p, env) - body_temp);
}

float thermal_cost(const Phenotype& p, float body_temp) {
    const float excess = std::abs(body_temp - p.thermal_optimum) - p.thermal_tolerance;
    if (excess <= 0.0f) return 0.0f;
    const float k = excess * (1.0f / THERMAL_SCALE);
    return METABOLIC_RATE * p.upkeep * k * k;
}

float movement_cost(const Phenotype& p, float distance, float height_gain) {
    // A long body slips through more easily; a boxy one drags (body.hpp). Climbing is the same for all.
    return p.mass * (p.drag * (TRANSPORT_COST * distance + MOVE_COST * distance * distance) + CLIMB_COST * std::max(0.0f, height_gain));
}

float throw_cost(float child_mass, float distance) {
    return THROW_COST * child_mass * distance * distance;
}

float contact_flow(const Phenotype& taker, float pull, const Phenotype& giver, float membrane) {
    // pull^2 / (pull + membrane): smooth, so a tiny absorber still leaks a
    // little out of a big host, and armour never makes anything immune.
    const float a = pull;
    if (a <= 0.0f) return 0.0f;
    // A square is worked out as one multiply, as the compiler did when
    // SIZE_POWER was a constant: powf(r, 2) rounds differently from r * r for
    // about one r in 160, and runs must not change with the law at its default.
    const float ratio = taker.radius / giver.radius;
    const float edge = std::min(SIZE_POWER == 2.0f ? ratio * ratio : std::pow(ratio, SIZE_POWER), SIZE_EDGE_MAX);
    return CONTACT_RATE * taker.bite * edge * a * a / (a + membrane);
}

namespace {

// How many units of the reaction in -> out this store and soil allow, at most
// `cap`: each input as far as there is of it (A from the soil), and if the
// store ends up holding more than before, as far as there is room.
inline float runnable(const uint8_t in[2], const uint8_t out[2], const Mix& pool, float soil_a, float capacity, float cap) {
    float amount = cap;
    int net = 0;
    for (int s = 0; s < 2; ++s) {
        const uint8_t c = in[s];
        if (c == NO_CHEM) continue;
        const float twice = (s == 0 && in[1] == c) ? 0.5f : 1.0f;   // X + X needs two of X a unit
        amount = std::min(amount, (c == CHEM_A ? soil_a : pool.v[c]) * twice);
        net -= c != CHEM_A;
    }
    for (int s = 0; s < 2; ++s) net += out[s] != NO_CHEM && out[s] != CHEM_A;
    if (net > 0) amount = std::min(amount, (capacity - pool.mass()) / static_cast<float>(net));
    return amount;
}

inline void run(const uint8_t in[2], const uint8_t out[2], float amount, Mix& pool, float& soil_a) {
    for (int s = 0; s < 2; ++s) {
        if (in[s] == NO_CHEM) continue;
        (in[s] == CHEM_A ? soil_a : pool.v[in[s]]) -= amount;
    }
    for (int s = 0; s < 2; ++s) {
        if (out[s] == NO_CHEM) continue;
        (out[s] == CHEM_A ? soil_a : pool.v[out[s]]) += amount;
    }
}

}  // namespace

float burn(const Phenotype& p, Mix& pool, float& soil_a, float want, float* two) {
    float released = 0.0f;
    for (int k = 0; k < p.enzymes && released < want; ++k) {
        const float step = p.enz_step[k];
        if (step <= 0.0f) continue;
        const float amount = runnable(p.enz_in[k], p.enz_out[k], pool, soil_a, p.pool_capacity,
                                      std::min(p.enz_rate[k], (want - released) / step));
        if (amount <= 0.0f) continue;
        run(p.enz_in[k], p.enz_out[k], amount, pool, soil_a);
        released += amount * step;
        if (two && p.enz_in[k][1] != NO_CHEM) *two += amount;
    }
    return released;
}

float power(const Phenotype& p, const Mix& pool, float soil_a) {
    // Enzymes sharing an input draw on the same stock: run them on a copy.
    Mix left = pool;
    float total = 0.0f;
    for (int k = 0; k < p.enzymes; ++k) {
        const float step = p.enz_step[k];
        if (step <= 0.0f) continue;
        const float amount = runnable(p.enz_in[k], p.enz_out[k], left, soil_a, p.pool_capacity, p.enz_rate[k]);
        if (amount <= 0.0f) continue;
        run(p.enz_in[k], p.enz_out[k], amount, left, soil_a);
        total += amount * step;
    }
    return total;
}

float charge(const Phenotype& p, Mix& pool, float& soil_a, float energy, float* two) {
    float stored = 0.0f;
    for (int k = 0; k < p.enzymes && stored < energy; ++k) {
        const float step = p.enz_step[k];
        if (step <= 0.0f) continue;
        // Uphill: the poorer side in, the richer side out.
        const float amount = runnable(p.enz_out[k], p.enz_in[k], pool, soil_a, p.pool_capacity,
                                      std::min(p.enz_rate[k], (energy - stored) / step));
        if (amount <= 0.0f) continue;
        run(p.enz_out[k], p.enz_in[k], amount, pool, soil_a);
        stored += amount * step;
        if (two && p.enz_in[k][1] != NO_CHEM) *two += amount;
    }
    return stored;
}

float convert(const Phenotype& p, Mix& pool, float& soil_a) {
    float moved = 0.0f;
    for (int k = 0; k < p.enzymes; ++k) {
        if (!(p.neutral >> k & 1u)) continue;
        const float amount = runnable(p.enz_in[k], p.enz_out[k], pool, soil_a, p.pool_capacity, p.enz_rate[k]);
        if (amount <= 0.0f) continue;
        run(p.enz_in[k], p.enz_out[k], amount, pool, soil_a);
        moved += amount;
    }
    return moved;
}

float fouling_cost(const Phenotype& p, float foul_mass) {
    return FOUL_HARM * foul_mass * p.surface_area;
}

float uptake_rate(const Phenotype& p) {
    return p.burns ? UPTAKE_RATE * p.surface_area : 0.0f;
}

float corpse_demand(const Phenotype& p) {
    return CORPSE_RATE * p.absorb;
}

}  // namespace luca
