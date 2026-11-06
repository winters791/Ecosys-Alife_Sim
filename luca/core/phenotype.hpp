#pragma once
// Phenotype: stats DERIVED from the genome, computed once at birth and
// never changed. The tick reads only these, which keeps it fast.
//
// You own this struct too — add fields as your rules need them.

#include "genome.hpp"

namespace luca {

struct Phenotype {
    float mass          = 0.0f;
    float surface_area  = 0.0f;
    float upkeep        = 0.0f;  // energy per tick just to stay alive
    float light_capture = 0.0f;  // how strongly it absorbs light (see energy.hpp)
    float max_speed     = 0.0f;  // world units per tick; 0 = cannot move
    float repro_threshold   = 0.0f;
    float offspring_share   = 0.0f;
    float thermal_optimum   = 0.0f;
    float thermal_tolerance = 0.0f;
    float heat_absorption   = 0.0f;  // from pigment
};

// Genome -> Phenotype. YOURS TO WRITE (see phenotype.cpp).
Phenotype compile_phenotype(const Genome& g);

}  // namespace luca
