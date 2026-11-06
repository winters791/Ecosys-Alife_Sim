#pragma once
// The environment: heightmap terrain plus light and temperature fields,
// all on the same square grid of cells.

#include <vector>

#include "config.hpp"
#include "energy.hpp"

namespace luca {

class World {
public:
    void generate(const SimConfig& cfg);

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
    int   n_ = 0;
    float cell_size_ = 1.0f;
    std::vector<float> height_;
    std::vector<float> light_;
    std::vector<float> temperature_;
};

}  // namespace luca
