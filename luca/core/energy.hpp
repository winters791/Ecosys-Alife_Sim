#pragma once
// Energy rules: how much a creature gains and pays each tick, and the one
// rule for what happens when two bodies touch.
//
// What the scaffolding guarantees around these functions:
//   * Light is finite. Each cell receives env.light energy per tick. Every
//     creature in the cell asks for light_demand(). If the total demand is
//     more than the cell's light, the canopy rule applies: the biggest body
//     (by core radius) takes what it asks for first, then the next biggest,
//     and smaller ones get what is left. Bodies of exactly the same size
//     share alike. So crowding = competition = carrying capacity, and size
//     buys sunlight. Unabsorbed light is lost.
//   * Costs can never exceed the energy a creature has. If it can't pay, it
//     pays what it has, hits zero, and dies.
//   * Energy is audited: stored(now) must equal stored(before) + in - out.

#include <cstdint>

#include "phenotype.hpp"

namespace luca {

struct LocalEnv {
    float light       = 0.0f;  // energy available in this cell this tick (0 at night)
    float temperature = 0.0f;  // °C
    float height      = 0.0f;  // terrain height
};

// Light energy this creature tries to absorb this tick (before sharing).
float light_demand(const Phenotype& p, const LocalEnv& env);

// Sunbathing and moving conflict: the fraction of its light a body still
// catches after covering `moved` world units last tick. 1 when it sat still.
float light_while_moving(float moved);

// --- Dormancy --------------------------------------------------------------------
// A body with the dormancy gene shuts down when it is dark, or colder than it
// can bear. `sleep` runs from 0 (awake) up to the depth its gene allows.
// Asleep it burns far less, but it is helpless in proportion: it catches no
// light, cannot pull on anything, cannot move or breed, and it ages slowly.
// Falling asleep is quick and waking is slow, so a sleeper misses the dawn.
// Its membrane is unchanged: anything with a mouth can still drain it.

// How asleep the body is after this tick, given how asleep it was.
float next_sleep(const Phenotype& p, const LocalEnv& env, float sleep);
// Fraction of its upkeep and thermal cost a body still pays at this depth.
float cost_while_asleep(float sleep);
// Below this a body counts as awake: it may move and breed.
constexpr float AWAKE = 0.1f;
// Deeper than this it ages at an eighth of the usual pace.
constexpr float DEEP_SLEEP = 0.5f;

// Cost of existing. Bodies wear out, so it creeps up with age.
float upkeep_cost(const Phenotype& p, uint32_t age);

// Extra cost for being outside its comfortable temperature range.
float thermal_cost(const Phenotype& p, const LocalEnv& env);

// Cost of moving `distance` world units in one tick while climbing
// `height_gain` (negative = going downhill).
float movement_cost(const Phenotype& p, float distance, float height_gain);

// Cost to the parent of flinging a child of `child_mass` a `distance` (world
// units) beyond where it would land by itself, touching the parent.
float throw_cost(float child_mass, float distance);

// --- Contact: the one interaction primitive ------------------------------------
// There is no "eat" or "attack". When two bodies touch, energy leaks from one
// to the other at a rate set by the taker's pull against the giver's
// membrane. A strong pull on a weak membrane looks like predation; a weak
// pull on a big host looks like parasitism. Called once per direction.
float contact_flow(const Phenotype& taker, const Phenotype& giver);

// Energy per tick this creature tries to pull out of dead matter in its cell.
float corpse_demand(const Phenotype& p);

// Fraction of pulled energy that arrives in a plain body; the rest is lost as
// heat. This is why every step up a food chain supports fewer creatures. The
// digestion gene raises it for its owner (Phenotype::efficiency), at a price.
constexpr float TRANSFER_EFFICIENCY = 0.7f;

// Fraction of every corpse that weathers away each field update even if
// nothing eats it: its energy is lost as heat, its matter returns to the soil.
// Slow on purpose, so whatever breaks corpses down faster has a niche.
constexpr float CORPSE_WEATHERING = 0.004f;

}  // namespace luca
