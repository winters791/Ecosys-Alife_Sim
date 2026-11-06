#pragma once
// Mutation: parent genome -> child genome. YOURS TO WRITE (see mutation.cpp).
// Must use only the Rng passed in, so the sim stays deterministic.

#include "genome.hpp"
#include "rng.hpp"

namespace luca {

Genome mutate(const Genome& parent, Rng& rng);

}  // namespace luca
