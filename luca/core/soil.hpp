#pragma once
// The soil: what life leaves in each cell. Powers never write here; only
// living, dying and decay do.
//
//   nutrient       free matter, ready to be built into a body
//   corpse_matter  matter locked in dead bodies until they are broken down
//   corpse_energy  the energy still in those bodies (food for whatever can take it)
//   waste          a chemical the living give off (energy, no matter; see energy.hpp):
//                  it sickens every body in the cell and feeds the few that can take it up
//
// Matter is conserved: nutrient + corpse_matter + the mass of everything
// alive is the same number for the whole run.

#include <vector>

namespace luca {

struct Soil {
    std::vector<float> nutrient;
    std::vector<float> corpse_matter;
    std::vector<float> corpse_energy;
    std::vector<float> waste;

    void resize(std::size_t cells) {
        nutrient.assign(cells, 0.0f);
        corpse_matter.assign(cells, 0.0f);
        corpse_energy.assign(cells, 0.0f);
        waste.assign(cells, 0.0f);
    }
};

}  // namespace luca
