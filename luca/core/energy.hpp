#pragma once
// Energy rules: how much a creature gains and pays each tick.
// YOURS TO WRITE (see energy.cpp).
//
// What the scaffolding guarantees around these functions:
//   * Light is finite. Each cell receives env.light energy per tick. Every
//     creature in the cell asks for light_demand(); if the total demand is
//     more than the cell's light, everyone gets the same fraction. So
//     crowding = competition = carrying capacity. Unabsorbed light is lost.
//   * Costs can never exceed the energy a creature has. If it can't pay, it
//     pays what it has, hits zero, and dies.
//   * Energy is audited: stored(now) must equal stored(before) + in - out.

#include "phenotype.hpp"

namespace luca {

struct LocalEnv {
    float light       = 0.0f;  // energy available in this cell this tick
    float temperature = 0.0f;  // °C
    float height      = 0.0f;  // terrain height
};

// Light energy this creature tries to absorb this tick (before sharing).
float light_demand(const Phenotype& p, const LocalEnv& env);

// Baseline cost of existing.
float upkeep_cost(const Phenotype& p);

// Extra cost for being outside its comfortable temperature range.
float thermal_cost(const Phenotype& p, const LocalEnv& env);

// Cost of moving `distance` world units while climbing `height_gain`
// (negative = going downhill).
float movement_cost(const Phenotype& p, float distance, float height_gain);

}  // namespace luca
