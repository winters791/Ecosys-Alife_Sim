#include "mutation.hpp"

namespace luca {

// =============================================================================
// TODO(Uday): MUTATION — this is yours.
//
// PLACEHOLDER: perfect copying. With no mutation every creature stays an
// exact LUCA forever, so the population will plateau but nothing evolves.
//
// Operators to build (rough relative frequencies):
//   * Tweak (most common): nudge each float gene with small Gaussian noise,
//     e.g. g.core_radius += rng.normal() * sigma, applied with some
//     per-gene probability.
//   * Add a part (less common): random type, small size, random position.
//   * Delete a part (less common).
//   * Duplicate a part (less common): copy an existing part, maybe slightly
//     moved. This is how real evolution invents new structure.
//   * Change a part's type (rare).
//
// Things to decide:
//   * Rates: hard-code for now; later they can become a heritable gene
//     (evolvable mutation rate) or a god-mode "law" knob.
//   * Limits: cap the number of parts so genomes can't bloat forever
//     (or make parts expensive enough that they don't).
//   * Clamping: if you don't clamp in the phenotype compiler, clamp here.
// =============================================================================

Genome mutate(const Genome& parent, Rng& /*rng*/) {
    return parent;  // PLACEHOLDER: no mutation
}

}  // namespace luca
