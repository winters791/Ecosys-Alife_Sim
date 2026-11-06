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
    n_ = cfg.world_size;
    cell_size_ = cfg.cell_size;
    const int cells = n_ * n_;
    height_.assign(cells, 0.0f);
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

    // --- Light and temperature ----------------------------------------------
    for (int y = 0; y < n_; ++y) {
        // 0 at the sunny edge (y = 0), 1 at the far edge.
        const float lat = n_ > 1 ? static_cast<float>(y) / static_cast<float>(n_ - 1) : 0.0f;
        const float light_frac = cfg.light_mode == LightMode::Gradient
                                     ? 1.0f - (1.0f - cfg.gradient_min) * lat
                                     : 1.0f;
        for (int x = 0; x < n_; ++x) {
            const int i = y * n_ + x;
            light_[i] = cfg.sun_strength * light_frac;
            temperature_[i] = cfg.temp_sea_level
                            - cfg.temp_lapse * height_[i]
                            - cfg.temp_latitude_drop * lat;
        }
    }
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
