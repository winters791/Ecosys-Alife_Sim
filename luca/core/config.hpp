#pragma once
// World-level settings. Anything about how a *creature* works lives in the
// phenotype compiler / energy rules instead, not here.
//
// UNITS (proposed — change them now if you want, not later):
//   1 tick          = 1 minute of world time
//   1 energy unit   = what one cell receives per tick under full sun
//   distances       = world units; 1 cell = cell_size world units

#include <cstdint>

namespace luca {

enum class LightMode { Uniform, Gradient };

struct SimConfig {
    uint64_t seed = 42;

    // World
    int   world_size  = 128;     // cells per side (heightmap is world_size x world_size)
    float cell_size   = 1.0f;    // world units per cell
    float max_height  = 40.0f;   // generated terrain height range [0, max_height]
    float height_limit = 100.0f; // god powers can't push terrain outside [0, height_limit]

    // Light. Gradient: full sun at y = 0, fading to gradient_min * sun at the far edge.
    LightMode light_mode   = LightMode::Gradient;
    float     sun_strength = 1.0f;   // energy per cell per tick at full sun
    float     gradient_min = 0.4f;   // fraction of full sun at the dim edge

    // Slope shade: ground that faces away from the sun (which stands over the
    // y = 0 edge) and ground sunk below its surroundings gets less light.
    // 1 = the full effect, 0 = terrain casts no shade.
    float slope_shade = 1.0f;

    // Day and night. Light follows the sun: full by day, zero at night, with a
    // short dawn and dusk. Noon is twice sun_strength, so the average over a
    // whole day is still sun_strength. 0 = no cycle (a constant, average sun).
    int day_length = 600;            // ticks per day

    // Matter. Building a body takes matter out of the soil; it comes back when
    // the body dies and its corpse is broken down. The total never changes.
    float nutrient_mean = 2.0f;      // average free matter per cell at the start
    float nutrient_patchiness = 0.8f; // 0 = even, 1 = rich patches and bare ground
    // Rain: every 64 ticks this fraction of a cell's free matter moves to its
    // lowest neighbour, on a steep slope (less on a gentle one, none on the flat).
    // Kept small: a parent needs a whole body's worth of matter in its own cell,
    // so soil that drains quickly starves the slopes (0.005 shrank life to a quarter).
    float wash_rate = 0.001f;

    // Temperature (°C): sea-level temperature minus altitude lapse minus latitude drop.
    float temp_sea_level     = 25.0f;
    float temp_lapse         = 0.4f;   // °C lost per unit of height
    float temp_latitude_drop = 20.0f;  // °C lost from y = 0 edge to far edge
    // Cold nights: the air everywhere is this many °C warmer in mid-afternoon
    // than just before dawn (half above the daily mean, half below). Bodies
    // feel it through their own body temperature, which lags the air more the
    // bigger they are (energy.hpp). 0 = no swing; no day cycle = no swing.
    float temp_day_swing     = 10.0f;

    // Gene transfer: each tick that a body is draining another, the chance
    // that it takes up the blueprint of one of that body's parts. 0 = never.
    float gene_transfer_chance = 0.0001f;

    // Windfalls: now and then a lump of food falls on a random cell and lies
    // there like a corpse with no matter in it (energy only, so the matter
    // books don't change). Anything with a mouth in that cell can eat it, and
    // it weathers away like a corpse; movers can smell it with the same sense
    // and lean they use for the dead. A second, patchy energy source besides
    // the sun, so finding food can pay. Rate is energy per cell per tick,
    // averaged over the map (sun_strength is 1): 0.001 is about 1% of the
    // light life takes in. Each lump holds windfall_size. 0 = none.
    float windfall_rate = 0.001f;
    float windfall_size = 250.0f;   // about one LUCA corpse

    // Population
    int   initial_population = 200;
    float initial_energy     = 300.0f;   // half of what a LUCA needs to divide
    // A child cannot land across a step in the ground taller than this (height
    // units between neighbouring cells): it stops at the foot or the brink.
    // Natural hills are gentler; a raised mountain wall is not.
    float birth_max_step     = 4.0f;

    // Output
    int metrics_interval = 100;        // ticks between metrics rows
};

}  // namespace luca
