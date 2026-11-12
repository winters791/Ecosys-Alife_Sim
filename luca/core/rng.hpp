#pragma once
// Deterministic RNG: xoshiro256** seeded through splitmix64.
// Same seed + same build + same machine => identical simulation.
// Never use std::rand or std::random_device inside the sim.
//
// Counter-based draws (design doc 14.10.4, built 2026-11-12): the sim keeps
// no running stream. Every draw is made from a key, a hash of
// (draw seed, purpose, tick, creature id, ...), with an Rng seeded from that
// key on the spot when it needs more than one number. So what a body draws
// depends only on who it is and when, never on how many draws came before it
// or on where it sits in the creature arrays: re-sorting them, or running a
// phase on several threads, leaves every draw the same.

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

// What a draw is for. Part of every key, so two uses never share a number.
// Append new ones at the end: renumbering changes every run.
enum class Draw : uint64_t {
    Start = 1,      // where the first LUCAs land (a = which one)
    Wander,         // a mover's wander step (a = creature id)
    Transfer,       // whether a contact passes on a blueprint (a = taker id, b = giver id)
    Birth,          // a child's mutations and where it lands (a = parent id)
    Windfall,       // how many lumps fall this tick, and where
    Event,          // whether the world acts this tick, and how
    EventTrigger,   // an event started by hand (a = events so far)
};

// splitmix64's finaliser: a good 64-bit mix.
inline uint64_t mix64(uint64_t z) {
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

// The key for one draw (or one Rng's worth of draws).
inline uint64_t draw_key(uint64_t seed, Draw purpose, uint64_t tick, uint64_t a = 0, uint64_t b = 0) {
    constexpr uint64_t G = 0x9E3779B97F4A7C15ull;
    uint64_t h = mix64(seed + G * static_cast<uint64_t>(purpose));
    h = mix64(h ^ (tick + G));
    h = mix64(h ^ (a + 2 * G));
    return mix64(h ^ (b + 3 * G));
}

// One uniform number in [0, 1) straight from a key.
inline float key_uniform(uint64_t key) { return static_cast<float>(key >> 40) * (1.0f / 16777216.0f); }

}  // namespace luca
