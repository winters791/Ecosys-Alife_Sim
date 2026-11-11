#pragma once
// The environment: heightmap terrain plus light and temperature fields,
// all on the same square grid of cells.

#include <vector>

#include "config.hpp"
#include "energy.hpp"

namespace luca {

// Per-cell layers a brush can paint on.
enum class BrushField { Height, Heat };

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
    void brush(BrushField field, float x, float y, float radius, float amount);

    // Global sun multiplier (1 = the configured sun). May be pushed below
    // zero by stacked edits; the light field treats that as darkness.
    void  add_sun(float delta);
    float sun_scale() const { return sun_scale_ > 0.0f ? sun_scale_ : 0.0f; }

    // Global temperature offset in °C (ice ages, greenhouse eras).
    void  add_climate(float delta);
    float climate_offset() const { return climate_; }

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
    float light(int cell) const { return light_[cell]; }
    float temperature(int cell) const { return temperature_[cell]; }

    LocalEnv env(int cell) const { return {light_[cell], temperature_[cell], height_[cell]}; }

    // Keep a position inside the world.
    float clamp_coord(float v) const;

    const std::vector<float>& heights() const { return height_; }
    const std::vector<float>& lights() const { return light_; }
    const std::vector<float>& temperatures() const { return temperature_; }

private:
    // Recompute the derived fields (light, temperature) from the layers.
    void  refresh_light();
    void  refresh_temperature(int x0, int y0, int x1, int y1);  // inclusive cell box
    float latitude(int row) const;  // 0 at the sunny edge (y = 0), 1 at the far edge

    int   n_ = 0;
    float cell_size_ = 1.0f;
    SimConfig cfg_;
    float sun_scale_ = 1.0f;
    float climate_   = 0.0f;
    uint32_t height_rev_ = 0, climate_rev_ = 0;
    std::vector<float> height_;       // layer: terrain
    std::vector<float> heat_;         // layer: local temperature offset
    std::vector<float> light_;        // derived
    std::vector<float> temperature_;  // derived
};

}  // namespace luca
