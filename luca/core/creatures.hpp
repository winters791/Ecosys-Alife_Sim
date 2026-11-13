#pragma once
// Creature storage, data-oriented: one array per field, not one object per
// creature. The tick streams through the hot arrays (x, y, energy, pheno)
// in order; the genome array is only touched at birth.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "body.hpp"
#include "genome.hpp"
#include "phenotype.hpp"

namespace luca {

struct Creatures {
    // Hot: read/written every tick
    std::vector<float>     x, y;
    std::vector<float>     hx, hy;      // unit heading (which way it last moved)
    std::vector<float>     moved;       // distance covered last tick (others can sense this)
    std::vector<float>     ate, hurt;   // energy taken in from bodies, and drained out of it, last tick (touch)
    std::vector<float>     rest;        // the sleep level its brain aims for, 0..1 (SENSOR_BRAIN, stage 3)
    std::vector<float>     shut;        // how shut its mouths are, 0 open .. 1 (the brain's mouth output last tick; SENSOR_BRAIN)
    std::vector<float>     unfold;      // 0 leaves folded (it just moved) .. 1 fully open; see UNFOLD_TICKS
    std::vector<float>     wander;      // angle of its wandering urge; drifts a little each tick
    std::vector<float>     sleep;       // 0 awake .. 1 fully dormant (see energy.hpp)
    std::vector<float>     body_temp;   // °C; lags the air, more so in big bodies (see energy.hpp)
    std::vector<float>     energy;      // what its store is worth: pool[i].energy(), kept in step by the sim
    std::vector<Mix>       pool;        // its store: the chemicals it holds (never A; chem.hpp)
    std::vector<Mix>       tissue;      // what its body is built of; tissue mass = pheno.mass
    std::vector<float>     tissue_p, pool_p;   // the mineral P in its body and in its store (stoichiometry)
    std::vector<uint32_t>  age;
    std::vector<Phenotype> pheno;

    // Where its energy has come from over its life. Observation only (food
    // web, inspector, chronicle); the sim never reads these to decide anything.
    std::vector<float>     fed_light, fed_living, fed_dead, fed_waste, fed_vent;
    // Its life story, for the species record book. Observation only too.
    std::vector<uint32_t>  kills;       // bodies it finished off
    std::vector<uint32_t>  children;    // young it has had
    std::vector<uint32_t>  last_meal;   // observation only: tick it last ate from the living, + 1 (0 = never; not saved)

    // Cold: identity and inheritance
    std::vector<uint64_t>  id;          // unique for the whole run; never reused
    std::vector<uint32_t>  lineage;     // id of the founding ancestor
    std::vector<uint32_t>  species;     // see species.hpp; read by the kin sense only
    std::vector<uint32_t>  generation;  // number of divisions since the founder
    std::vector<Genome>    genome;

    // The brain, ready to run, and what it remembers. Only movers touch these.
    std::vector<BrainPlan>  brain;
    std::vector<BrainState> brain_state;
    // Where its mouths and plates point (body.hpp). Only contact reads it.
    std::vector<BodyPlan>   body;

    std::size_t size() const { return x.size(); }

    void reserve(std::size_t n) {
        x.reserve(n); y.reserve(n); hx.reserve(n); hy.reserve(n); moved.reserve(n); ate.reserve(n); hurt.reserve(n); unfold.reserve(n); shut.reserve(n); rest.reserve(n); wander.reserve(n); sleep.reserve(n); body_temp.reserve(n);
        energy.reserve(n); pool.reserve(n); tissue.reserve(n); tissue_p.reserve(n); pool_p.reserve(n); age.reserve(n); pheno.reserve(n);
        fed_light.reserve(n); fed_living.reserve(n); fed_dead.reserve(n); fed_waste.reserve(n); fed_vent.reserve(n);
        kills.reserve(n); children.reserve(n); last_meal.reserve(n);
        id.reserve(n); lineage.reserve(n); species.reserve(n); generation.reserve(n); genome.reserve(n);
        brain.reserve(n); brain_state.reserve(n); body.reserve(n);
    }

    void add(float px, float py, const Mix& store, const Mix& body_mix, float temp, uint64_t uid, uint32_t lin, uint32_t sp, uint32_t gen, Genome g, const Phenotype& p,
             float body_p = 0.0f, float store_p = 0.0f) {
        x.push_back(px);
        y.push_back(py);
        // A body faces somewhere from birth: the way it starts to wander.
        // Sessile bodies keep this facing for life (it decides where their
        // mouths and plates point); movers turn to face the way they go.
        const float facing = initial_wander(uid);
        hx.push_back(std::cos(facing));
        hy.push_back(std::sin(facing));
        moved.push_back(0.0f);
        ate.push_back(0.0f);
        unfold.push_back(1.0f);
        shut.push_back(0.0f);
        rest.push_back(0.0f);
        hurt.push_back(0.0f);
        wander.push_back(facing);
        sleep.push_back(0.0f);
        body_temp.push_back(temp);
        fed_light.push_back(0.0f);
        fed_living.push_back(0.0f);
        fed_dead.push_back(0.0f);
        fed_waste.push_back(0.0f);
        fed_vent.push_back(0.0f);
        kills.push_back(0);
        children.push_back(0);
        last_meal.push_back(0);
        id.push_back(uid);
        energy.push_back(store.energy());
        pool.push_back(store);
        tissue.push_back(body_mix);
        tissue_p.push_back(body_p);
        pool_p.push_back(store_p);
        age.push_back(0);
        pheno.push_back(p);
        lineage.push_back(lin);
        species.push_back(sp);
        generation.push_back(gen);
        brain.push_back(p.has_brain ? compile_brain(g) : BrainPlan{});
        brain_state.emplace_back();
        body.push_back(p.absorb > 0.0f || p.part_total[static_cast<int>(PartType::Armour)] > 0.0f ? compile_body(g) : BodyPlan{});
        genome.push_back(std::move(g));
    }

    // O(1) removal: move the last creature into slot i. Changes order, but
    // deterministically, so replays still match.
    void remove_swap(std::size_t i) {
        swap_pop(x, i); swap_pop(y, i); swap_pop(hx, i); swap_pop(hy, i); swap_pop(moved, i); swap_pop(ate, i); swap_pop(hurt, i); swap_pop(unfold, i); swap_pop(shut, i); swap_pop(rest, i); swap_pop(wander, i); swap_pop(sleep, i); swap_pop(body_temp, i);
        swap_pop(energy, i); swap_pop(pool, i); swap_pop(tissue, i); swap_pop(tissue_p, i); swap_pop(pool_p, i); swap_pop(age, i);
        swap_pop(fed_light, i); swap_pop(fed_living, i); swap_pop(fed_dead, i); swap_pop(fed_waste, i); swap_pop(fed_vent, i); swap_pop(kills, i); swap_pop(children, i); swap_pop(last_meal, i); swap_pop(id, i);
        swap_pop(pheno, i); swap_pop(lineage, i); swap_pop(species, i); swap_pop(generation, i); swap_pop(genome, i);
        swap_pop(brain, i); swap_pop(brain_state, i); swap_pop(body, i);
    }

    // A starting wander angle from the id alone (a splitmix64 hash), so births
    // need no draw and every lineage doesn't set off one way.
    static float initial_wander(uint64_t uid) {
        uint64_t z = uid + 0x9e3779b97f4a7c15ull;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        z ^= z >> 31;
        return static_cast<float>(z >> 40) * (6.28318531f / 16777216.0f);
    }

private:

    template <class T>
    static void swap_pop(std::vector<T>& v, std::size_t i) {
        if (i + 1 != v.size()) v[i] = std::move(v.back());
        v.pop_back();
    }
};

}  // namespace luca
