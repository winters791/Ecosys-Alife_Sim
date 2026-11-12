#pragma once
// Sprite data shared by the world view, the species card and the creature
// designer: one creature's instance (20 floats) and parts (PARTS_PER_CREATURE
// texels), laid out as documented in luca_world.h and read by blob.gdshader.

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "phenotype.hpp"

namespace sprite {

// A fixed random number in [0, 1) per creature, so each blob breathes and
// wiggles on its own beat for its whole life.
inline float hash01(uint64_t id) {
    uint64_t h = id * 0x9E3779B97F4A7C15ull;
    h ^= h >> 29;
    h *= 0xBF58476D1CE4E5B9ull;
    h ^= h >> 32;
    return static_cast<float>(h & 0xFFFFu) / 65536.0f;
}

// One colour per species, as a hue: steps of the golden ratio keep species
// with neighbouring ids far apart on the colour wheel.
inline float species_hue(uint32_t id) {
    const float h = 0.13f + static_cast<float>(id % 4096u) * 0.61803399f;
    return h - std::floor(h);
}

// One creature's sprite data, as documented in luca_world.h.
inline void write_sprite(float* f, const luca::Genome& g, const luca::Phenotype& p, float x, float y, float hx, float hy,
                  float sleep, float energy, float hue, float phase) {
    auto part = [&](luca::PartType t) { return p.part_total[static_cast<int>(t)]; };
    // Rows of the 3x4 transform (row-major: f[0], f[4], f[8] are basis x ...).
    // f[0] is the radius as grown so far, so the young are drawn small.
    f[0] = p.radius;        f[1] = part(luca::PartType::Storage);   f[2] = g.hue;  f[3] = x;
    f[4] = g.elongation;    f[5] = sleep;                           f[6] = hx;     f[7] = hue;
    f[8] = g.squareness;    f[9] = static_cast<float>(g.parts.size()); f[10] = hy; f[11] = y;
    // Colour channel: raw traits. The shader decides how they look.
    f[12] = std::clamp(g.pigment, 0.0f, 1.0f);
    f[13] = p.surface_area > 0.0f ? std::clamp(p.light_capture / p.surface_area, 0.0f, 1.0f) : 0.0f;
    f[14] = p.repro_threshold > 0.0f ? std::clamp(energy / p.repro_threshold, 0.0f, 1.0f) : 0.0f;
    f[15] = p.has_brain ? std::clamp(p.max_speed * 4.0f, 0.0f, 1.0f) : 0.0f;
    // Custom data.
    f[16] = phase;
    f[17] = g.shape[0];
    f[18] = g.shape[1];
    f[19] = part(luca::PartType::Poison);
}

// One creature's parts: PARTS_PER_CREATURE texels of (type + 1, size, angle, stalk).
inline void write_parts(float* t, const luca::Genome& g, int per_creature) {
    const std::size_t k_max = std::min(g.parts.size(), static_cast<std::size_t>(per_creature));
    for (std::size_t k = 0; k < k_max; ++k, t += 4) {
        t[0] = static_cast<float>(static_cast<int>(g.parts[k].type) + 1);
        t[1] = g.parts[k].size;
        t[2] = g.parts[k].theta;
        t[3] = g.parts[k].stalk;
    }
}

}  // namespace sprite
