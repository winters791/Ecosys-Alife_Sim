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
// Matter is conserved: nutrient + the mass of every corpse + loose chemicals +
// every living body and store changes only by what vents bring up from below
// (Simulation::matter_brought_up).

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

    void resize(std::size_t cells) {
        nutrient.assign(cells, 0.0f);
        corpse.assign(cells, Mix{});
        corpse_energy.assign(cells, 0.0f);
        dissolved.assign(cells, Mix{});
        waste.assign(cells, 0.0f);
        vent_made.assign(cells, 0.0f);
    }

    void add_corpse(std::size_t cell, const Mix& m) {
        corpse[cell].add(m);
        corpse_energy[cell] = corpse[cell].energy();
    }
    // Takes `mass` from the cell's corpses, in proportion to what they hold.
    Mix take_corpse(std::size_t cell, float mass) {
        Mix out = take_share(corpse[cell], mass);
        corpse_energy[cell] = corpse[cell].energy();
        return out;
    }
};

}  // namespace luca
