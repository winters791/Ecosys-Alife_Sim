#include "energy.hpp"

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
constexpr float METABOLIC_RATE = 0.75f;  // was 1.0; tuned 2026-11-11 for a fuller world (three-seed sweep)

// Energy gained per unit of light capture under full sun.
constexpr float PHOTO_EFFICIENCY = 1.0f;

// A patch only works spread out and held still. A body that covered
// FULL_STRIDE or more last tick catches just (1 - MOVING_SHADE) of its light;
// in between it is proportional. This is what makes a lineage choose between
// staying put in the sun and going to find food.
constexpr float MOVING_SHADE = 0.8f;
// Was 0.08; 0.02 since wandering became a drifting direction (2026-11-11).
// At 0.08 a slow drifter kept 90% of its light, so roaming grazers lost
// almost nothing by moving and overgrazed the world to a quarter of its size.
constexpr float FULL_STRIDE  = 0.005f;  // world units per tick

// Dormancy. A night is about 240 ticks of a 600-tick day, so at full depth a
// sleeper saves most of a night's upkeep and loses roughly the first hour and
// a half of morning light while it wakes.
constexpr float DORMANT_SAVING = 0.9f;        // fraction of upkeep saved at full depth
constexpr float FALL_ASLEEP    = 1.0f / 20.0f;  // sleep gained per tick while it is dark or too cold
constexpr float WAKE_UP        = 1.0f / 90.0f;  // sleep lost per tick once it is light and warm enough

// Wear: upkeep is multiplied by 1 + (age / AGE_SCALE)^2. Nothing dies "of old
// age"; an old body just costs more than it can earn, and its matter is freed.
constexpr float AGE_SCALE = 10000.0f;  // was 5000: a body now costs 244 to build, so it should last

// Dark skin under sun feels warmer: °C per unit of heat_absorption per unit light.
constexpr float PIGMENT_WARMTH = 8.0f;
// Being this many °C outside the comfort range doubles upkeep; it grows as the square.
constexpr float THERMAL_SCALE = 8.0f;

// Moving costs mass x speed^2 (per tick, distance = speed), climbing costs
// mass x height gained. Coming downhill gives nothing back.
constexpr float MOVE_COST  = 3.0f;
constexpr float CLIMB_COST = 1.0f;

// Flinging a child: mass x distance^2, like moving, but paid once per birth.
constexpr float THROW_COST = 2.0f;

// Waste (see energy.hpp). At these numbers a LUCA alone on open ground sits
// in about 4 units of waste and loses ~0.01 a tick to it (3% of its upkeep);
// in a crowded cell it is several times that.
constexpr float WASTE_HARM     = 0.0025f;  // energy per tick per unit of waste per unit of skin
constexpr float WASTE_EAT_RATE = 0.3f;     // waste taken up per tick at full detox

// Contact.
constexpr float CONTACT_RATE = 1.5f;  // energy per tick for a pull of 1 against no membrane
constexpr float CORPSE_RATE  = 2.0f;  // the dead have no membrane left

// Size matters at the mouth: the pull is scaled by (taker radius / giver
// radius)^SIZE_POWER, up to SIZE_EDGE_MAX. Equal bodies are unchanged; a body
// half its victim's size pulls a quarter as hard, so being bigger than your
// neighbours is protection, and a small thing on a big host can only sip.
constexpr float SIZE_POWER    = 2.0f;
constexpr float SIZE_EDGE_MAX = 2.0f;

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
    return METABOLIC_RATE * p.upkeep * (1.0f + a * a);
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
    return p.mass * (MOVE_COST * distance * distance + CLIMB_COST * std::max(0.0f, height_gain));
}

float throw_cost(float child_mass, float distance) {
    return THROW_COST * child_mass * distance * distance;
}

float contact_flow(const Phenotype& taker, const Phenotype& giver) {
    // pull^2 / (pull + membrane): smooth, so a tiny absorber still leaks a
    // little out of a big host, and armour never makes anything immune.
    const float a = taker.absorb;
    if (a <= 0.0f) return 0.0f;
    const float edge = std::min(std::pow(taker.radius / giver.radius, SIZE_POWER), SIZE_EDGE_MAX);
    return CONTACT_RATE * taker.bite * edge * a * a / (a + giver.membrane);
}

float fouling_cost(const Phenotype& p, float waste_here) {
    return WASTE_HARM * waste_here * p.surface_area;
}

float waste_demand(const Phenotype& p) {
    return WASTE_EAT_RATE * p.detox;
}

float corpse_demand(const Phenotype& p) {
    return CORPSE_RATE * p.absorb;
}

}  // namespace luca
