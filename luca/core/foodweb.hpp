#pragma once
// The food web (design doc 4.3), built from the energy that actually moved:
// nothing is labelled predator, plant or scavenger. Observation only.
//
// The sim keeps a ledger of energy taken in, by the species that took it and
// where it came from (FoodLedger): drained from a living body of some
// species, taken up from leavings some species gave off (the leavings tag),
// or from a source that is not a species: the sun, a vent, the dead, or loose
// chemicals nobody is tagged with. The ledger fades (FADE every census, so it
// reflects about the last few thousand ticks), and food_web() turns it into
// nodes and edges, with each species' trophic level worked out from the
// flows: the sun and vents are level 0, the dead and leavings level 1, and a
// species sits one above the mean of what it eats, weighted by energy.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace luca {

class Simulation;

enum class FoodKind : uint8_t { Living = 0, Leavings = 1, Source = 2 };
enum class FoodSource : uint32_t { Sun = 0, Dead = 1, Vent = 2, Loose = 3 };   // untagged loose chemicals

struct FoodLedger {
    static constexpr double FADE = 0.98;   // per census (50 ticks): a half-life of about 1,700 ticks

    std::vector<std::array<double, 4>> source;     // per species: from each FoodSource, faded

    static uint64_t key(uint32_t taker, FoodKind kind, uint32_t from) {
        return (static_cast<uint64_t>(taker) << 32) | (static_cast<uint64_t>(kind) << 30) | (from & 0x3FFFFFFFu);
    }
    // Species to species flows (Living, Leavings), by key(), faded. A flat
    // open-addressed table: this is called once a contact a tick, and a
    // node-based map cost a few percent of the whole tick.
    void add(uint32_t taker, FoodKind kind, uint32_t from, double energy) {
        if (energy <= 0.0) return;
        const uint64_t k = key(taker, kind, from);
        if (k == last_key_ && last_ != NONE) { val_[last_] += energy; return; }
        if ((used_ + 1) * 2 > keys_.size()) grow();
        std::size_t i = slot_of(k);
        if (keys_[i] == EMPTY) { keys_[i] = k; val_[i] = 0.0; ++used_; }
        val_[i] += energy;
        last_key_ = k;
        last_ = i;
    }
    void add_source(uint32_t taker, FoodSource from, double energy) {
        if (energy <= 0.0) return;
        if (taker >= source.size()) source.resize(taker + 1, {0.0, 0.0, 0.0, 0.0});
        source[taker][static_cast<std::size_t>(from)] += energy;
    }
    void fade() {
        // Rebuilt from what survives, so dropped entries leave no holes.
        std::vector<uint64_t> keys;
        std::vector<double> vals;
        keys.reserve(used_);
        vals.reserve(used_);
        each([&](uint64_t k, double e) {
            if (e * FADE >= 1e-3) { keys.push_back(k); vals.push_back(e * FADE); }
        });
        rehash(keys_.size());
        for (std::size_t j = 0; j < keys.size(); ++j) {
            const std::size_t i = slot_of(keys[j]);
            keys_[i] = keys[j];
            val_[i] = vals[j];
        }
        used_ = keys.size();
        for (auto& s : source)
            for (double& e : s) e = e * FADE < 1e-3 ? 0.0 : e * FADE;
    }
    // Calls f(key, energy) for every flow, in no particular order.
    template <class F>
    void each(F&& f) const {
        for (std::size_t i = 0; i < keys_.size(); ++i)
            if (keys_[i] != EMPTY) f(keys_[i], val_[i]);
    }

private:
    static constexpr uint64_t EMPTY = ~uint64_t{0};
    static constexpr std::size_t NONE = ~std::size_t{0};
    std::vector<uint64_t> keys_;
    std::vector<double>   val_;
    std::size_t used_ = 0;
    uint64_t    last_key_ = EMPTY;
    std::size_t last_ = NONE;

    std::size_t slot_of(uint64_t k) const {   // linear probing; keys_.size() is a power of two
        const std::size_t mask = keys_.size() - 1;
        std::size_t i = static_cast<std::size_t>((k * 0x9E3779B97F4A7C15ull) >> 20) & mask;
        while (keys_[i] != EMPTY && keys_[i] != k) i = (i + 1) & mask;
        return i;
    }
    void rehash(std::size_t size) {
        keys_.assign(size, EMPTY);
        val_.assign(size, 0.0);
        used_ = 0;
        last_ = NONE;
        last_key_ = EMPTY;
    }
    void grow() {
        std::vector<uint64_t> keys = std::move(keys_);
        std::vector<double> vals = std::move(val_);
        rehash(keys.empty() ? 1024 : keys.size() * 2);
        for (std::size_t j = 0; j < keys.size(); ++j)
            if (keys[j] != EMPTY) {
                const std::size_t i = slot_of(keys[j]);
                keys_[i] = keys[j];
                val_[i] = vals[j];
                ++used_;
            }
    }
};

struct FoodWebNode {
    uint32_t species = 0;
    uint32_t alive = 0;
    double   level = 1.0;    // trophic level
    double   intake = 0.0;   // energy a tick, all sources
};

struct FoodWebEdge {
    FoodKind kind = FoodKind::Source;
    uint32_t from = 0;       // a species, or a FoodSource when kind is Source
    uint32_t to = 0;         // the species that took it
    double   energy = 0.0;   // a tick
};

struct FoodWeb {
    std::vector<FoodWebNode> nodes;   // living species with at least min_alive, most intake first
    std::vector<FoodWebEdge> edges;   // into those nodes, from sources or from listed species; most energy first
};

// Species with fewer than `min_alive` living members are left out, and so are
// flows from them.
FoodWeb food_web(const Simulation& sim, uint32_t min_alive);

}  // namespace luca
