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
    std::vector<float>     energy;
    std::vector<uint32_t>  age;
    std::vector<Phenotype> pheno;

    // Cold: identity and inheritance
    std::vector<uint32_t>  lineage;     // id of the founding ancestor
    std::vector<uint32_t>  generation;  // number of divisions since the founder
    std::vector<Genome>    genome;

    std::size_t size() const { return x.size(); }

    void reserve(std::size_t n) {
        x.reserve(n); y.reserve(n); energy.reserve(n); age.reserve(n); pheno.reserve(n);
        lineage.reserve(n); generation.reserve(n); genome.reserve(n);
    }

    void add(float px, float py, float e, uint32_t lin, uint32_t gen, Genome g, const Phenotype& p) {
        x.push_back(px);
        y.push_back(py);
        energy.push_back(e);
        age.push_back(0);
        pheno.push_back(p);
        lineage.push_back(lin);
        generation.push_back(gen);
        genome.push_back(std::move(g));
    }

    // O(1) removal: move the last creature into slot i. Changes order, but
    // deterministically, so replays still match.
    void remove_swap(std::size_t i) {
        swap_pop(x, i); swap_pop(y, i); swap_pop(energy, i); swap_pop(age, i);
        swap_pop(pheno, i); swap_pop(lineage, i); swap_pop(generation, i); swap_pop(genome, i);
    }

private:
    template <class T>
    static void swap_pop(std::vector<T>& v, std::size_t i) {
        if (i + 1 != v.size()) v[i] = std::move(v.back());
        v.pop_back();
    }
};

}  // namespace luca
