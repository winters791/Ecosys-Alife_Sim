#pragma once
// Creature storage, data-oriented: one array per field, not one object per
// creature. The tick streams through the hot arrays (x, y, energy, pheno)
// in order; the genome array is only touched at birth.

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "genome.hpp"
#include "phenotype.hpp"

namespace luca {

struct Creatures {
    // Hot: read/written every tick
    std::vector<float>     x, y;
    std::vector<float>     hx, hy;      // unit heading (which way it last moved)
    std::vector<float>     moved;       // distance covered last tick (others can sense this)
    std::vector<float>     sleep;       // 0 awake .. 1 fully dormant (see energy.hpp)
    std::vector<float>     energy;
    std::vector<uint32_t>  age;
    std::vector<Phenotype> pheno;

    // Where its energy has come from over its life. Observation only (food
    // web, inspector, chronicle); the sim never reads these to decide anything.
    std::vector<float>     fed_light, fed_living, fed_dead;

    // Cold: identity and inheritance
    std::vector<uint64_t>  id;          // unique for the whole run; never reused
    std::vector<uint32_t>  lineage;     // id of the founding ancestor
    std::vector<uint32_t>  species;     // see species.hpp; read by the kin sense only
    std::vector<uint32_t>  generation;  // number of divisions since the founder
    std::vector<Genome>    genome;

    // The brain, ready to run, and what it remembers. Only movers touch these.
    std::vector<BrainPlan>  brain;
    std::vector<BrainState> brain_state;

    std::size_t size() const { return x.size(); }

    void reserve(std::size_t n) {
        x.reserve(n); y.reserve(n); hx.reserve(n); hy.reserve(n); moved.reserve(n); sleep.reserve(n);
        energy.reserve(n); age.reserve(n); pheno.reserve(n);
        fed_light.reserve(n); fed_living.reserve(n); fed_dead.reserve(n);
        id.reserve(n); lineage.reserve(n); species.reserve(n); generation.reserve(n); genome.reserve(n);
        brain.reserve(n); brain_state.reserve(n);
    }

    void add(float px, float py, float e, uint64_t uid, uint32_t lin, uint32_t sp, uint32_t gen, Genome g, const Phenotype& p) {
        x.push_back(px);
        y.push_back(py);
        hx.push_back(0.0f);
        hy.push_back(1.0f);
        moved.push_back(0.0f);
        sleep.push_back(0.0f);
        fed_light.push_back(0.0f);
        fed_living.push_back(0.0f);
        fed_dead.push_back(0.0f);
        id.push_back(uid);
        energy.push_back(e);
        age.push_back(0);
        pheno.push_back(p);
        lineage.push_back(lin);
        species.push_back(sp);
        generation.push_back(gen);
        brain.push_back(p.has_brain ? compile_brain(g) : BrainPlan{});
        brain_state.emplace_back();
        genome.push_back(std::move(g));
    }

    // O(1) removal: move the last creature into slot i. Changes order, but
    // deterministically, so replays still match.
    void remove_swap(std::size_t i) {
        swap_pop(x, i); swap_pop(y, i); swap_pop(hx, i); swap_pop(hy, i); swap_pop(moved, i); swap_pop(sleep, i);
        swap_pop(energy, i); swap_pop(age, i);
        swap_pop(fed_light, i); swap_pop(fed_living, i); swap_pop(fed_dead, i); swap_pop(id, i);
        swap_pop(pheno, i); swap_pop(lineage, i); swap_pop(species, i); swap_pop(generation, i); swap_pop(genome, i);
        swap_pop(brain, i); swap_pop(brain_state, i);
    }

private:
    template <class T>
    static void swap_pop(std::vector<T>& v, std::size_t i) {
        if (i + 1 != v.size()) v[i] = std::move(v.back());
        v.pop_back();
    }
};

}  // namespace luca
