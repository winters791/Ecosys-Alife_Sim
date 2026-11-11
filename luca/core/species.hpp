#pragma once
// Species: clusters in genome space, found by the game (design doc, section 9).
//
// Reproduction is asexual, so species are labels the game works out. They
// feed the chronicle, the metrics and the viewer, and use no randomness.
// One rule of life reads them: the kin sense (OUT_KIN in genome.hpp) lets a
// sighted creature tell its own species from strangers. So the threshold and
// weights below now shape behaviour a little, not only what the player sees.
//
// How it works (NEAT-style):
//   * every species has a representative genome: its founder's;
//   * a newborn takes its parent's species, unless its genome has drifted
//     further than SPECIES_THRESHOLD from that representative;
//   * then it joins a living daughter of the parent's species if one is close
//     enough, and otherwise founds a new species.
// That is one distance check for nearly every birth; a newborn that has
// drifted costs one more per living daughter of its parent's species.
//
// Not built yet: the periodic check for one species forming two clusters
// (9.3 step 3), e.g. either side of a mountain.

#include <cstdint>
#include <string>
#include <vector>

#include "genome.hpp"

namespace luca {

// How different two genomes are: 0 for identical, about 1 for "a different
// kind of creature". The same function is meant to drive fertility later.
// The weights are in species.cpp.
float genome_distance(const Genome& a, const Genome& b);

constexpr float    SPECIES_THRESHOLD   = 1.0f;  // distance at which a newborn leaves its species
constexpr uint32_t SPECIES_ESTABLISHED = 10;    // alive at once before the chronicle takes notice
constexpr uint32_t NO_SPECIES          = 0xFFFFFFFFu;

struct Species {
    uint32_t    id = 0;
    uint32_t    parent = NO_SPECIES;   // the species it branched from; none for LUCA and newcomers
    std::string name;
    Genome      representative;        // the founder's genome
    uint64_t    born_tick = 0;
    uint64_t    extinct_tick = 0;      // meaningful only when population == 0
    uint32_t    population = 0;
    uint32_t    peak = 0;              // most alive at once
    bool        established = false;   // ever reached SPECIES_ESTABLISHED
    std::vector<uint32_t> living_daughters;  // branched from this one and not yet extinct (pruned lazily)

    bool alive() const { return population > 0; }
};

class SpeciesRegistry {
public:
    // A species with no ancestor in this world (the first LUCAs).
    uint32_t found_root(const Genome& g, uint64_t tick, std::string name = {});

    // The species of a newborn, given its parent's. May found a new one.
    uint32_t classify(uint32_t parent_species, const Genome& child, uint64_t tick);

    // The species of a creature arriving from outside: the nearest living
    // species if one is close enough, otherwise a new root. Not for hot paths.
    uint32_t classify_newcomer(const Genome& g, uint64_t tick);

    // Bookkeeping. born() returns true the moment a species becomes
    // established; died() returns true when its last member dies.
    bool born(uint32_t species);
    bool died(uint32_t species, uint64_t tick);

    const std::vector<Species>& all() const { return species_; }
    const Species& get(uint32_t id) const { return species_[id]; }
    std::size_t living() const { return living_; }
    std::size_t living_established() const;

private:
    uint32_t found(uint32_t parent, const Genome& g, uint64_t tick, std::string name);

    std::vector<Species> species_;
    std::size_t living_ = 0;
};

// A pronounceable name, unique per id for the first 51,200 species ("Velox").
std::string species_name(uint32_t id);

// What sets `child` apart from `parent`, as the end of a sentence:
// "has grown a mouth", "is smaller". Empty if nothing stands out.
std::string describe_difference(const Genome& child, const Genome& parent);

}  // namespace luca
