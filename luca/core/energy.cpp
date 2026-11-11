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

float upkeep_cost(const Phenotype& p, uint32_t age) {
    const float a = static_cast<float>(age) * (1.0f / AGE_SCALE);
    return METABOLIC_RATE * p.upkeep * (1.0f + a * a);
}

float thermal_cost(const Phenotype& p, const LocalEnv& env) {
    const float felt   = env.temperature + PIGMENT_WARMTH * p.heat_absorption * env.light;
    const float excess = std::abs(felt - p.thermal_optimum) - p.thermal_tolerance;
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

float corpse_demand(const Phenotype& p) {
    return CORPSE_RATE * p.absorb;
}

}  // namespace luca
