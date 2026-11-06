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
    float max_height  = 40.0f;   // terrain height range [0, max_height]

    // Light. Gradient: full sun at y = 0, fading to gradient_min * sun at the far edge.
    LightMode light_mode   = LightMode::Gradient;
    float     sun_strength = 1.0f;   // energy per cell per tick at full sun
    float     gradient_min = 0.2f;   // fraction of full sun at the dim edge

    // Temperature (°C): sea-level temperature minus altitude lapse minus latitude drop.
    float temp_sea_level     = 25.0f;
    float temp_lapse         = 0.4f;   // °C lost per unit of height
    float temp_latitude_drop = 20.0f;  // °C lost from y = 0 edge to far edge

    // Population
    int   initial_population = 200;
    float initial_energy     = 5.0f;
    float offspring_spread   = 1.5f;   // max distance a child lands from its parent

    // Output
    int metrics_interval = 100;        // ticks between metrics rows
};

}  // namespace luca
