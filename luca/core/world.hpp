#pragma once
// The environment: heightmap terrain plus light and temperature fields,
// all on the same square grid of cells.

#include <cstdint>
#include <vector>

#include "config.hpp"
#include "energy.hpp"

namespace luca {

// Per-cell layers a brush can paint on.
enum class BrushField { Height, Heat, Vent };

class World {
public:
    void generate(const SimConfig& cfg);

    // --- God-power edits ----------------------------------------------------
    // These are the ONLY ways the world changes after generate(). They touch
    // the environment, never creatures. Deltas add up, so overlapping edits
    // compose in any order.

    // Soft round brush centred on world position (x, y): adds `amount` at the
    // centre, fading smoothly to zero at `radius` (world units).
    //   Height: terrain height, kept inside [0, height_limit].
    //   Heat:   a local temperature offset in °C (hot springs, cold sinks).
    //   Vent:   vent strength, kept inside [0, vent_max]; also warms the ground.
    void brush(BrushField field, float x, float y, float radius, float amount);

    // Global sun multiplier (1 = the configured sun). May be pushed below
    // zero by stacked edits; the light field treats that as darkness.
    void  add_sun(float delta);
    float sun_scale() const { return sun_scale_ > 0.0f ? sun_scale_ : 0.0f; }

    // Global temperature offset in °C (ice ages, greenhouse eras).
    void  add_climate(float delta);
    float climate_offset() const { return climate_; }

    // --- Time of day ----------------------------------------------------------
    // Not a power and not random: the sun's position is a pure function of the
    // tick. The sim calls this at the start of every tick.
    void  set_time(uint64_t tick);
    float daylight() const { return daylight_; }      // 0 at night .. 2 at noon; averages 1 over a day
    float day_phase() const { return day_phase_; }    // 0 sunrise, 0.25 noon, 0.5 sunset, 0.75 midnight
    int   day_length() const { return cfg_.day_length; }
    // Cold nights: °C added to every cell's daily mean temperature right now.
    // Warmest in mid-afternoon, coldest just before dawn; averages 0 over a day.
    float air_swing() const { return air_swing_; }

    // Bumped whenever a field changes, so a renderer knows when to re-upload.
    uint32_t height_revision() const { return height_rev_; }
    uint32_t climate_revision() const { return climate_rev_; }  // light or temperature

    int   size() const { return n_; }
    int   num_cells() const { return n_ * n_; }
    float cell_size() const { return cell_size_; }
    float extent() const { return n_ * cell_size_; }  // world width in world units

    // Cell containing world position (x, y); clamped to the grid.
    int cell_index(float x, float y) const;
    int cell_row(int cell) const { return cell / n_; }

    float height(int cell) const { return height_[cell]; }
    float light(int cell) const { return light_[cell] * daylight_; }  // right now
    float noon_light(int cell) const { return light_[cell]; }
    float shade(int cell) const { return shade_[cell]; }   // 1 = open ground .. less in the shade of the land

    // Where water runs: the lowest of a cell's eight neighbours, or -1 if none
    // is lower, and how steep the way down is (0 flat .. 1 steep).
    int   downhill(int cell) const { return downhill_[cell]; }
    float steepness(int cell) const { return steep_[cell]; }
    float temperature(int cell) const { return temperature_[cell] + air_swing_; }  // right now
    // Vents: strength 0..vent_max, and the energy a cell's vent gives each tick.
    float vent(int cell) const { return vent_[cell]; }
    float vent_energy(int cell) const { return vent_[cell] * cfg_.vent_power; }
    bool  any_vents() const { return vent_cells_ > 0; }
    uint32_t vent_revision() const { return vent_rev_; }
    float mean_temperature(int cell) const { return temperature_[cell]; }        // averaged over a day

    LocalEnv env(int cell) const { return {light_[cell] * daylight_, temperature_[cell] + air_swing_, height_[cell]}; }

    // Smooth patchy noise in [0, 1] for a cell, different for each `salt`.
    // Used to lay out the starting soil.
    float patch_noise(int cell, uint32_t salt) const;

    // Keep a position inside the world.
    float clamp_coord(float v) const;

    const std::vector<float>& heights() const { return height_; }
    const std::vector<float>& lights() const { return light_; }  // at noon
    const std::vector<float>& temperatures() const { return temperature_; }  // daily means (add air_swing() for now)
    const std::vector<float>& vents() const { return vent_; }

private:
    // Recompute the derived fields (light, temperature) from the layers.
    void  refresh_light();
    // Shade and drainage depend only on the shape of the land: recomputed for
    // the cells in (and just around) a box whenever terrain changes there.
    void  refresh_terrain(int x0, int y0, int x1, int y1);
    void  refresh_temperature(int x0, int y0, int x1, int y1);  // inclusive cell box
    float latitude(int row) const;  // 0 at the sunny edge (y = 0), 1 at the far edge

    int   n_ = 0;
    float cell_size_ = 1.0f;
    SimConfig cfg_;
    float sun_scale_ = 1.0f;
    float climate_   = 0.0f;
    float daylight_  = 1.0f;
    float day_phase_ = 0.25f;
    float air_swing_ = 0.0f;
    uint32_t height_rev_ = 0, climate_rev_ = 0, vent_rev_ = 0;
    int      vent_cells_ = 0;         // cells with any vent at all
    std::vector<float> height_;       // layer: terrain
    std::vector<float> heat_;         // layer: local temperature offset
    std::vector<float> vent_;         // layer: vent strength
    std::vector<float> shade_;        // derived from height: fraction of the sun that reaches the ground
    std::vector<int>   downhill_;     // derived from height: lowest neighbour, or -1
    std::vector<float> steep_;        // derived from height: 0..1
    std::vector<float> light_;        // derived: light at noon
    std::vector<float> temperature_;  // derived
};

}  // namespace luca
