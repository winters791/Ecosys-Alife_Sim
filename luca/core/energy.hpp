#pragma once

#include <algorithm>
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

// Folding (2026-11-12, in place of MOVER_SUN): a body that moves further than
// FOLD_STRIDE in a tick folds its sun patches shut, and they take
// UNFOLD_TICKS (a day) to open again, catching light in proportion. A body
// that moves now and then (to flee) loses a little; one that creeps every few
// ticks stays folded. 0 = off.
constexpr float FOLD_STRIDE  = 0.001f;
constexpr float UNFOLD_TICKS = 600.0f;
inline float next_unfold(float unfold, float moved) {
    if (UNFOLD_TICKS <= 0.0f) return 1.0f;
    return moved > FOLD_STRIDE ? 0.0f : std::min(1.0f, unfold + 1.0f / UNFOLD_TICKS);
}

// --- Dormancy --------------------------------------------------------------------
// A body with the dormancy gene shuts down when it is dark, or colder than it
// can bear. `sleep` runs from 0 (awake) up to the depth its gene allows.
// Asleep it burns far less, but it is helpless in proportion: it catches no
// light, cannot pull on anything, cannot move or breed, and it ages slowly.
// Falling asleep is quick and waking is slow, so a sleeper misses the dawn.
// Its membrane is unchanged: anything with a mouth can still drain it.

// How asleep the body is after this tick, given how asleep it was and how
// warm its body is (see body temperature below).
float next_sleep(const Phenotype& p, const LocalEnv& env, float body_temp, float sleep);
// Fraction of its upkeep and thermal cost a body still pays at this depth.
float cost_while_asleep(float sleep);
// Below this a body counts as awake: it may move and breed.
constexpr float AWAKE = 0.1f;
// Deeper than this it ages at an eighth of the usual pace.
constexpr float DEEP_SLEEP = 0.5f;

// Cost of existing. Bodies wear out, so it creeps up with age.
float upkeep_cost(const Phenotype& p, uint32_t age);

// --- Body temperature (cold nights) -----------------------------------------------
// The air swings warm and cold with the day (World::air_swing). A body does
// not feel the air directly: its own temperature moves towards what it would
// feel (the air, plus sun on dark skin) by Phenotype::heat_follow of the gap
// each tick. Big and fat bodies hold heat, so they feel a smoothed day; small
// thin ones feel every cold night. Nothing tells anything to be big.
float felt_temperature(const Phenotype& p, const LocalEnv& env);
float next_body_temp(const Phenotype& p, const LocalEnv& env, float body_temp);

// Extra cost for a body this warm being outside its comfortable range.
float thermal_cost(const Phenotype& p, float body_temp);

// Cost of moving `distance` world units in one tick while climbing
// `height_gain` (negative = going downhill). Part of it is paid per world
// unit, whatever the speed; the rest grows with speed squared.
float movement_cost(const Phenotype& p, float distance, float height_gain);

// Cost to the parent of flinging a child of `child_mass` a `distance` (world
// units) beyond where it would land by itself, touching the parent.
float throw_cost(float child_mass, float distance);

// --- Contact: the one interaction primitive ------------------------------------
// There is no "eat" or "attack". When two bodies touch, energy leaks from one
// to the other at a rate set by the taker's pull against the giver's
// membrane. A strong pull on a weak membrane looks like predation; a weak
// pull on a big host looks like parasitism. Called once per direction.
// `pull` is the taker's pull towards the giver and `membrane` the giver's
// resistance on the side it is touched from: both depend on where mouths and
// plates sit (BodyPlan, body.hpp).
float contact_flow(const Phenotype& taker, float pull, const Phenotype& giver, float membrane);

// --- Metabolism: the chemistry layer (chem.hpp) -----------------------------------
// A body has no energy of its own, only a store of chemicals. Everything it
// spends it gets by running its enzymes downhill on that store, and
// everything it gains from light or a vent it keeps by running them uphill.
// Each enzyme turns over at most its rate each way in one call. A reaction
// that ends at A gives the A to the cell's soil (`soil_a`); one that starts
// from A takes it from there, and only while the store has room.

// A two-input enzyme (stage 4) needs both its inputs, so it runs as far as
// the scarcer allows, and one that leaves the store fuller than it found it
// (A in from the soil) only as far as there is room.
//
// `two`, if given, gains the units turned over by two-input enzymes (observation).
// Releases up to `want` energy from the store. Returns what it released:
// less if the store or its enzymes can't give more.
float burn(const Phenotype& p, Mix& pool, float& soil_a, float want, float* two = nullptr);
// The most `burn` could release from this store this tick.
float power(const Phenotype& p, const Mix& pool, float soil_a);
// Stores up to `energy` in the store. Returns what it stored: less if there
// is no A to hand, no room, or no enzyme to do it.
float charge(const Phenotype& p, Mix& pool, float& soil_a, float energy, float* two = nullptr);
// Neutral exchanges (stage 4): each enzyme in Phenotype::neutral runs once a
// tick at its rate, as far as its inputs and room allow, turning chemicals
// the body can't burn into ones it can. Releases and costs nothing. Returns
// the units turned over.
float convert(const Phenotype& p, Mix& pool, float& soil_a);

// Energy per tick this creature tries to pull out of dead matter in its cell.
float corpse_demand(const Phenotype& p);

// Fraction of pulled energy that arrives in a plain body; the rest is lost as
// heat. This is why every step up a food chain supports fewer creatures. The
// same for every body since chemistry stage 3 (the digestion gene is gone);
// since stage 2 the rest spills into the cell rather than burning.
constexpr float TRANSFER_EFFICIENCY = 0.7f;

// --- Loose chemicals: chemistry stage 2 (2026-11-12) -----------------------------
// A cell holds chemicals outside any body (Soil::dissolved): what eaters spill
// and what bodies give off. There is no waste gene and no special waste: an
// eater keeps `efficiency` of the chemicals it can burn and spills the rest,
// with everything it can't burn, into its cell; a body gives off whatever its
// store holds that it can't burn. Every body takes up the chemicals it can
// burn through its skin: each tick it asks for `uptake_rate` of what the cell
// holds, and if the cell's bodies ask for more than all of it, it is shared
// out. Chemicals it can't burn sicken it (fouling_cost), so one body's waste
// is another's food and a third's poison. Loose chemicals spread to the cells
// around and slowly break down to A, their energy lost as heat.
constexpr float UPTAKE_RATE = 0.05f;   // share of the cell's stock taken up per tick per unit of skin
float uptake_rate(const Phenotype& p);
// Energy a body loses per tick to `foul_mass` units of loose chemicals it
// can't burn, in its cell.
float fouling_cost(const Phenotype& p, float foul_mass);
// --- Vents (2026-11-12; a chemical since chemistry stage 3) ------------------
// Each tick a vent brings up VENT_CHEM from below, loose in its cell,
// World::vent_energy worth of it. Energy and matter are both new (energy_in
// and in_vent; TickStats::matter_in). There is no gene for it: bodies that can burn VENT_CHEM take it
// up through their skin like any loose chemical, and bodies that can't are
// fouled by it. C, not D, so living on a vent needs an enzyme a plant lacks.
constexpr int VENT_CHEM = CHEM_C;
// Each field update (8 ticks), this fraction of a cell's loose chemicals moves
// to its four neighbours (a quarter each), and this fraction breaks down to A.
constexpr float WASTE_SPREAD = 0.2f;
constexpr float WASTE_DECAY  = 0.04f;

// Fraction of every corpse that weathers away each field update even if
// nothing eats it: its energy is lost as heat, its matter returns to the soil.
// Slow on purpose, so whatever breaks corpses down faster has a niche.
constexpr float CORPSE_WEATHERING = 0.004f;

}  // namespace luca
