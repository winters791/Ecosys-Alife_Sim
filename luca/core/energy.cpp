#include "energy.hpp"

namespace luca {

// =============================================================================
// TODO(Uday): THE ENERGY RULES — these are yours.
//
// All four functions are PLACEHOLDERS. Ideas for the real versions:
//
//   light_demand:  light_capture × env.light, maybe with diminishing returns
//                  so one huge patch isn't strictly better than two small ones.
//
//   upkeep_cost:   mostly p.upkeep, which your phenotype compiler sets from
//                  Kleiber scaling + per-part costs.
//
//   thermal_cost:  zero inside [optimum - tolerance, optimum + tolerance],
//                  rising (linearly? quadratically?) outside it. Pigment can
//                  shift the creature's *effective* temperature: dark skin
//                  feels warmer under sun. Wide tolerance should cost extra
//                  upkeep in the phenotype, or specialists never win.
//
//   movement_cost: ∝ mass × distance for flat ground, plus mass × g × height
//                  gain for climbing (maybe a partial refund going downhill).
//                  This is what makes cliffs barriers for heavy creatures.
//
// Keep these cheap: they run for every creature, every tick.
// =============================================================================

float light_demand(const Phenotype& p, const LocalEnv& env) {
    return p.light_capture * env.light;  // PLACEHOLDER
}

float upkeep_cost(const Phenotype& p) {
    return p.upkeep;  // PLACEHOLDER
}

float thermal_cost(const Phenotype& /*p*/, const LocalEnv& /*env*/) {
    return 0.0f;  // PLACEHOLDER: temperature doesn't matter yet
}

float movement_cost(const Phenotype& /*p*/, float /*distance*/, float /*height_gain*/) {
    return 0.0f;  // PLACEHOLDER: movement is free (nobody moves yet anyway)
}

}  // namespace luca
