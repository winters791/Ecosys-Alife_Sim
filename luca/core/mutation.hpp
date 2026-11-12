#pragma once
// Mutation: parent genome -> child genome. YOURS TO WRITE (see mutation.cpp).
// Must use only the Rng passed in, so the sim stays deterministic. The sim
// seeds it from a key (rng.hpp, draw_key: the parent's id and the tick).

#include "genome.hpp"
#include "rng.hpp"

namespace luca {

Genome mutate(const Genome& parent, Rng& rng);

// Gene transfer: copy the blueprint of one of `from`'s parts, picked at
// random, into `to` as a small bud. Returns false (and changes nothing) if
// `from` has no parts or `to` has no room. `copied` is set to the part's type.
bool transfer_part(const Genome& from, Genome& to, Rng& rng, PartType& copied);

}  // namespace luca
