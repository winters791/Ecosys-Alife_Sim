#pragma once
// Deterministic RNG: xoshiro256** seeded through splitmix64.
// Same seed + same build + same machine => identical simulation.
// Never use std::rand or std::random_device inside the sim.

#include <cmath>
#include <cstdint>

namespace luca {

class Rng {
public:
    explicit Rng(uint64_t seed = 1) { seed_with(seed); }

    void seed_with(uint64_t seed) {
        uint64_t z = seed;
        for (auto& s : s_) s = splitmix64(z);
    }

    uint64_t next_u64() {
        const uint64_t result = rotl(s_[1] * 5, 7) * 9;
        const uint64_t t = s_[1] << 17;
        s_[2] ^= s_[0];
        s_[3] ^= s_[1];
        s_[1] ^= s_[2];
        s_[0] ^= s_[3];
        s_[2] ^= t;
        s_[3] = rotl(s_[3], 45);
        return result;
    }

    // Uniform float in [0, 1).
    float uniform() { return static_cast<float>(next_u64() >> 40) * (1.0f / 16777216.0f); }

    float uniform(float lo, float hi) { return lo + (hi - lo) * uniform(); }

    // Standard normal (mean 0, sd 1), Box-Muller without caching so the
    // number of draws per call is always the same (keeps streams aligned).
    float normal() {
        float u1 = uniform();
        if (u1 < 1e-7f) u1 = 1e-7f;
        const float u2 = uniform();
        return std::sqrt(-2.0f * std::log(u1)) * std::cos(6.28318531f * u2);
    }

    // Uniform integer in [0, n).
    uint32_t below(uint32_t n) {
        return static_cast<uint32_t>(((next_u64() >> 32) * static_cast<uint64_t>(n)) >> 32);
    }

    // True with probability p.
    bool chance(float p) { return uniform() < p; }

private:
    static uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
    static uint64_t splitmix64(uint64_t& x) {
        uint64_t z = (x += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    uint64_t s_[4];
};

}  // namespace luca
