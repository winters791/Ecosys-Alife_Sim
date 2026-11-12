#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace luca {

namespace {

// Integer hash -> deterministic lattice noise (no RNG state needed).
uint32_t hash2(int x, int y, uint32_t seed) {
    uint32_t h = seed ^ 0x9E3779B9u;
    h ^= static_cast<uint32_t>(x) * 0x85EBCA6Bu;
    h = (h << 13) | (h >> 19);
    h ^= static_cast<uint32_t>(y) * 0xC2B2AE35u;
    h *= 0x27D4EB2Fu;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    return h;
}

float lattice(int x, int y, uint32_t seed) {
    return static_cast<float>(hash2(x, y, seed) & 0xFFFFFFu) / 16777215.0f;
}

float smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }

// --- Slope shade (laws of this world; tune here) -------------------------------
// The sun stands over the y = 0 edge. Ground that falls away from it is in its
// own shadow; ground sunk below the land around it is in everyone else's.
constexpr float FACING_LOSS  = 0.15f;  // light lost per unit of height dropped per cell, away from the sun
constexpr float FACING_MAX   = 0.5f;
constexpr int   HORIZON      = 3;      // cells: how far a valley floor looks for its rim
constexpr float VALLEY_LOSS  = 0.06f;  // light lost per unit of depth below that rim
constexpr float VALLEY_MAX   = 0.4f;
// --- Drainage -------------------------------------------------------------------
constexpr float STEEP_DROP   = 2.0f;   // a drop this tall to the lowest neighbour counts as fully steep

float value_noise(float x, float y, uint32_t seed) {
    const int   x0 = static_cast<int>(std::floor(x));
    const int   y0 = static_cast<int>(std::floor(y));
    const float tx = smoothstep(x - x0);
    const float ty = smoothstep(y - y0);
    const float a = lattice(x0, y0, seed),     b = lattice(x0 + 1, y0, seed);
    const float c = lattice(x0, y0 + 1, seed), d = lattice(x0 + 1, y0 + 1, seed);
    const float top = a + (b - a) * tx;
    const float bot = c + (d - c) * tx;
    return top + (bot - top) * ty;
}

float fbm(float x, float y, uint32_t seed, int octaves) {
    float sum = 0.0f, norm = 0.0f, amp = 1.0f, freq = 1.0f;
    for (int o = 0; o < octaves; ++o) {
        sum  += amp * value_noise(x * freq, y * freq, seed + 1013u * static_cast<uint32_t>(o));
        norm += amp;
        amp  *= 0.5f;
        freq *= 2.0f;
    }
    return sum / norm;
}

}  // namespace

void World::generate(const SimConfig& cfg) {
    cfg_ = cfg;
    n_ = cfg.world_size;
    cell_size_ = cfg.cell_size;
    sun_scale_ = 1.0f;
    climate_ = 0.0f;
    set_time(0);
    const int cells = n_ * n_;
    height_.assign(cells, 0.0f);
    heat_.assign(cells, 0.0f);
    vent_.assign(cells, 0.0f);
    vent_cells_ = 0;
    shade_.assign(cells, 1.0f);
    downhill_.assign(cells, -1);
    steep_.assign(cells, 0.0f);
    light_.assign(cells, 0.0f);
    temperature_.assign(cells, 0.0f);

    // --- Terrain: fractal value noise, ~4 big features across the map -------
    const uint32_t seed = static_cast<uint32_t>(cfg.seed ^ (cfg.seed >> 32));
    const float base = 4.0f / static_cast<float>(n_);
    float lo = 1e9f, hi = -1e9f;
    for (int y = 0; y < n_; ++y)
        for (int x = 0; x < n_; ++x) {
            const float h = fbm(x * base, y * base, seed, 5);
            height_[y * n_ + x] = h;
            lo = std::min(lo, h);
            hi = std::max(hi, h);
        }
    const float range = (hi - lo) > 1e-6f ? (hi - lo) : 1.0f;
    for (float& h : height_) h = (h - lo) / range * cfg.max_height;

    refresh_terrain(0, 0, n_ - 1, n_ - 1);
    refresh_light();
    refresh_temperature(0, 0, n_ - 1, n_ - 1);
}

void World::set_time(uint64_t tick) {
    if (cfg_.day_length <= 0) { daylight_ = 1.0f; day_phase_ = 0.25f; air_swing_ = 0.0f; return; }
    const uint64_t len = static_cast<uint64_t>(cfg_.day_length);
    day_phase_ = static_cast<float>(tick % len) / static_cast<float>(len);
    // A sine pushed through a clamp: flat day, flat night, short twilight.
    // Scaled to 0..2 so it averages 1 over a day: sun_strength stays "energy
    // per cell per tick" in the long run, it just all arrives in daytime.
    daylight_ = 2.0f * std::clamp(0.5f + 1.5f * std::sin(6.2831853f * day_phase_), 0.0f, 1.0f);
    // Cold nights: the ground warms all day and cools all night, so the air
    // lags the sun by an eighth of a day: warmest at phase 0.375, coldest at
    // 0.875 (just before dawn). A plain sine, so the daily mean is unchanged.
    air_swing_ = 0.5f * cfg_.temp_day_swing * std::sin(6.2831853f * (day_phase_ - 0.125f));
}

float World::patch_noise(int cell, uint32_t salt) const {
    const uint32_t seed = static_cast<uint32_t>(cfg_.seed ^ (cfg_.seed >> 32)) + salt * 0x9E3779B1u;
    const float base = 7.0f / static_cast<float>(n_);
    return fbm(static_cast<float>(cell % n_) * base, static_cast<float>(cell / n_) * base, seed, 4);
}

float World::latitude(int row) const {
    return n_ > 1 ? static_cast<float>(row) / static_cast<float>(n_ - 1) : 0.0f;
}

void World::refresh_light() {
    ++climate_rev_;
    const float sun = cfg_.sun_strength * sun_scale();
    for (int y = 0; y < n_; ++y) {
        const float frac = cfg_.light_mode == LightMode::Gradient
                               ? 1.0f - (1.0f - cfg_.gradient_min) * latitude(y)
                               : 1.0f;
        for (int x = 0; x < n_; ++x) light_[y * n_ + x] = sun * frac * shade_[y * n_ + x];
    }
}

void World::refresh_terrain(int x0, int y0, int x1, int y1) {
    // A changed cell alters the shade of cells up to HORIZON away.
    x0 = std::max(x0 - HORIZON, 0);
    y0 = std::max(y0 - HORIZON, 0);
    x1 = std::min(x1 + HORIZON, n_ - 1);
    y1 = std::min(y1 + HORIZON, n_ - 1);
    auto h = [&](int x, int y) { return height_[std::clamp(y, 0, n_ - 1) * n_ + std::clamp(x, 0, n_ - 1)]; };
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            const int i = y * n_ + x;
            const float here = height_[i];

            // Facing: how fast the ground drops as you walk away from the sun.
            const float away = std::max(0.0f, 0.5f * (h(x, y - 1) - h(x, y + 1)));
            const float facing = std::min(FACING_LOSS * away, FACING_MAX);
            // Valley: how far below its rim (four points HORIZON cells out) it lies.
            const float rim = 0.25f * (h(x - HORIZON, y) + h(x + HORIZON, y) + h(x, y - HORIZON) + h(x, y + HORIZON));
            const float valley = std::min(VALLEY_LOSS * std::max(0.0f, rim - here), VALLEY_MAX);
            shade_[i] = 1.0f - std::clamp(cfg_.slope_shade, 0.0f, 1.0f) * (facing + valley);

            // Drainage: the lowest of the eight neighbours, if any is lower.
            int low = -1;
            float low_h = here;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    const int nx = x + dx, ny = y + dy;
                    if ((dx == 0 && dy == 0) || nx < 0 || ny < 0 || nx >= n_ || ny >= n_) continue;
                    if (height_[ny * n_ + nx] < low_h) {
                        low_h = height_[ny * n_ + nx];
                        low = ny * n_ + nx;
                    }
                }
            downhill_[i] = low;
            steep_[i] = low >= 0 ? std::min((here - low_h) / STEEP_DROP, 1.0f) : 0.0f;
        }
}

void World::refresh_temperature(int x0, int y0, int x1, int y1) {
    ++climate_rev_;
    for (int y = y0; y <= y1; ++y) {
        const float row_base = cfg_.temp_sea_level + climate_ - cfg_.temp_latitude_drop * latitude(y);
        for (int x = x0; x <= x1; ++x) {
            const int i = y * n_ + x;
            temperature_[i] = row_base - cfg_.temp_lapse * height_[i] + heat_[i] + cfg_.vent_heat * vent_[i];
        }
    }
}

void World::brush(BrushField field, float x, float y, float radius, float amount) {
    radius = std::max(radius, cell_size_);  // always reaches at least one cell centre's neighbourhood
    const int x0 = std::clamp(static_cast<int>(std::floor((x - radius) / cell_size_)), 0, n_ - 1);
    const int x1 = std::clamp(static_cast<int>(std::floor((x + radius) / cell_size_)), 0, n_ - 1);
    const int y0 = std::clamp(static_cast<int>(std::floor((y - radius) / cell_size_)), 0, n_ - 1);
    const int y1 = std::clamp(static_cast<int>(std::floor((y + radius) / cell_size_)), 0, n_ - 1);
    std::vector<float>& layer = field == BrushField::Height ? height_ : field == BrushField::Heat ? heat_ : vent_;
    for (int cy = y0; cy <= y1; ++cy)
        for (int cx = x0; cx <= x1; ++cx) {
            const float dx = (static_cast<float>(cx) + 0.5f) * cell_size_ - x;
            const float dy = (static_cast<float>(cy) + 0.5f) * cell_size_ - y;
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d >= radius) continue;
            const float w = 0.5f * (1.0f + std::cos(3.14159265f * d / radius));
            float& v = layer[cy * n_ + cx];
            const bool had_vent = field == BrushField::Vent && v > 0.0f;
            v += amount * w;
            if (field == BrushField::Height) v = std::clamp(v, 0.0f, cfg_.height_limit);
            if (field == BrushField::Vent) {
                v = std::clamp(v, 0.0f, cfg_.vent_max);
                if (v < 1e-3f) v = 0.0f;   // the faint edge of a closed vent is no vent
                vent_cells_ += (v > 0.0f) - had_vent;
            }
        }
    refresh_temperature(x0, y0, x1, y1);
    if (field == BrushField::Vent) ++vent_rev_;
    if (field == BrushField::Height) {
        ++height_rev_;
        refresh_terrain(x0, y0, x1, y1);
        refresh_light();
    }
}

void World::add_sun(float delta) {
    sun_scale_ += delta;
    refresh_light();
}

void World::add_climate(float delta) {
    climate_ += delta;
    refresh_temperature(0, 0, n_ - 1, n_ - 1);
}

int World::cell_index(float x, float y) const {
    int cx = static_cast<int>(x / cell_size_);
    int cy = static_cast<int>(y / cell_size_);
    cx = std::clamp(cx, 0, n_ - 1);
    cy = std::clamp(cy, 0, n_ - 1);
    return cy * n_ + cx;
}

float World::clamp_coord(float v) const {
    const float max_v = extent() - 1e-3f;
    return std::clamp(v, 0.0f, max_v);
}

}  // namespace luca
