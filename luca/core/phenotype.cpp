#include "phenotype.hpp"

namespace luca {

// =============================================================================
// TODO(Uday): THE PHENOTYPE COMPILER — this is yours.
//
// What's here now is a deliberately dumb PLACEHOLDER so the whole pipeline
// runs end to end. Every creature costs the same regardless of its body, so
// there are NO trade-offs yet, and evolution has nothing to work with.
//
// What to build:
//   1. Clamp the genome to sane ranges first. Mutation will produce negative
//      radii, shares > 1, part sizes < 0. Decide whether to clamp here or in
//      mutate() — but do it in exactly one place.
//   2. Scaling laws for the core blob:
//        mass         ∝ core_radius^3
//        surface_area ∝ core_radius^2
//        upkeep       ∝ mass^0.75          (Kleiber's law)
//   3. Every part adds mass and upkeep. Nothing is free — a gene that's pure
//      upside will take over every lineage.
//   4. light_capture from Photo parts, probably limited by surface area
//      (patches have to sit somewhere on the skin).
//   5. max_speed from Flagellum/Legs, divided by mass.
//   6. heat_absorption from pigment.
//   7. Copy through the reproduction and thermal genes.
//
// Rule of thumb: every number you set here should have a cost somewhere.
// =============================================================================

Phenotype compile_phenotype(const Genome& g) {
    Phenotype p;

    // PLACEHOLDER: constant body, constant upkeep.
    p.mass         = 1.0f;
    p.surface_area = 1.0f;
    p.upkeep       = 0.5f;

    // PLACEHOLDER: capture = total photo patch size, no surface-area limit.
    p.light_capture = total_part_size(g, PartType::Photo);

    // PLACEHOLDER: nobody moves yet.
    p.max_speed = 0.2f;

    p.repro_threshold   = g.repro_threshold;
    p.offspring_share   = g.offspring_share;
    p.thermal_optimum   = g.thermal_optimum;
    p.thermal_tolerance = g.thermal_tolerance;
    p.heat_absorption   = g.pigment;

    return p;
}

}  // namespace luca
