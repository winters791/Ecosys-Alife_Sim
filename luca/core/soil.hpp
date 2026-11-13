#pragma once
// The soil: what life leaves in each cell. Powers never write here; only
// living, dying and decay do.
//
//   nutrient       free matter: chemical A, spent and worth nothing (chem.hpp).
//                  Light turns it into richer chemicals in a living store;
//                  burning them gives it back.
//   corpse         the chemicals locked in dead bodies until they are eaten or weather away
//   corpse_energy  what those chemicals are worth (kept in step with `corpse`
//                  by add_corpse / take_corpse; the brain smells it)
//   dissolved      chemicals loose in the cell, outside any body (never A): what
//                  eaters spill and bodies give off (energy.hpp). Food for bodies
//                  that can burn them, foul to the rest.
//   waste          what the loose chemicals are worth (kept in step with
//                  `dissolved` by Simulation::step; the viewer and metrics read it)
//   vent_made      observation only: how much of the cell's loose VENT_CHEM
//                  (energy.hpp) a vent made, as against spills and leavings.
//                  It drifts and breaks down with the rest, and uptake takes it
//                  in proportion, so food from a vent can be told apart.
//
//   mineral        the mineral P free in the soil (stoichiometry,
//                  docs/design/stoichiometry.md): no energy, built into bodies.
//                  Taken up through the skin, given back by eaters with more
//                  than they can hold. It does not drift; it washes downhill.
//   corpse_p       the P locked in the cell's corpses, freed as they are eaten
//                  (in proportion to the mass taken) or, more slowly, weather.
//
// Two bitsets, one bit a cell, say where corpses and loose chemicals may be,
// so the field passes walk those cells only, in ascending order (14.5 item 1).
// A bit may be set over an empty cell; a cell holding something always has
// its bit set. Anything that adds to `dissolved` calls mark_loose.
//
// Matter is conserved: nutrient + the mass of every corpse + loose chemicals +
// every living body and store changes only by what vents bring up from below
// (Simulation::matter_brought_up).

#include <bit>
#include <cstdint>
#include <vector>

#include "chem.hpp"

namespace luca {

struct Soil {
    std::vector<float> nutrient;
    std::vector<Mix>   corpse;
    std::vector<float> corpse_energy;
    std::vector<Mix>   dissolved;
    std::vector<float> waste;
    std::vector<float> vent_made;
    std::vector<float> mineral;
    std::vector<float> corpse_p;
    std::vector<uint64_t> corpse_bits;   // may hold a corpse
    std::vector<uint64_t> loose_bits;    // may hold loose chemicals (or vent_made)

    void mark_corpse(std::size_t cell) { corpse_bits[cell >> 6] |= uint64_t{1} << (cell & 63); }
    void mark_loose(std::size_t cell) { loose_bits[cell >> 6] |= uint64_t{1} << (cell & 63); }
    static void unmark(std::vector<uint64_t>& bits, std::size_t cell) { bits[cell >> 6] &= ~(uint64_t{1} << (cell & 63)); }
    // Calls f(cell) for every set bit, in ascending order. f may clear bits.
    template <class F>
    static void each(const std::vector<uint64_t>& bits, F&& f) {
        for (std::size_t w = 0; w < bits.size(); ++w)
            for (uint64_t b = bits[w]; b != 0; b &= b - 1) f((w << 6) + static_cast<std::size_t>(std::countr_zero(b)));
    }
    // Sets the bits from the fields as they are (after loading).
    void rebuild_bits() {
        corpse_bits.assign((corpse.size() + 63) / 64, 0);
        loose_bits.assign((corpse.size() + 63) / 64, 0);
        for (std::size_t c = 0; c < corpse.size(); ++c) {
            bool dead = corpse_p[c] != 0.0f;
            for (float v : corpse[c].v) dead = dead || v != 0.0f;
            if (dead) mark_corpse(c);
            const Mix& m = dissolved[c];
            bool any = vent_made[c] != 0.0f;
            for (float v : m.v) any = any || v != 0.0f;
            if (any) mark_loose(c);
        }
    }

    void resize(std::size_t cells) {
        corpse_bits.assign((cells + 63) / 64, 0);
        loose_bits.assign((cells + 63) / 64, 0);
        nutrient.assign(cells, 0.0f);
        corpse.assign(cells, Mix{});
        corpse_energy.assign(cells, 0.0f);
        dissolved.assign(cells, Mix{});
        waste.assign(cells, 0.0f);
        vent_made.assign(cells, 0.0f);
        mineral.assign(cells, 0.0f);
        corpse_p.assign(cells, 0.0f);
    }

    void add_corpse(std::size_t cell, const Mix& m, float p = 0.0f) {
        corpse[cell].add(m);
        corpse_p[cell] += p;
        corpse_energy[cell] = corpse[cell].energy();
        mark_corpse(cell);
    }
    // Takes `mass` from the cell's corpses, in proportion to what they hold.
    Mix take_corpse(std::size_t cell, float mass) {
        Mix out = take_share(corpse[cell], mass);
        corpse_energy[cell] = corpse[cell].energy();
        return out;
    }
    // The same, also taking the corpses' P in proportion to the mass taken
    // (all of it once the corpses are gone); adds it to `p`.
    Mix take_corpse(std::size_t cell, float mass, float& p) {
        const float before = corpse[cell].mass();
        Mix out = take_corpse(cell, mass);
        if (corpse_p[cell] > 0.0f && before > 0.0f) {
            const float got = out.mass() >= before ? corpse_p[cell] : corpse_p[cell] * (out.mass() / before);
            corpse_p[cell] -= got;
            p += got;
        }
        return out;
    }
};

}  // namespace luca
