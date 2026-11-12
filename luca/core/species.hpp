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
// Splits (9.3 step 3, 2026-11-12): every few thousand ticks the sim looks
// inside each big species for two separate clusters of genomes (e.g. either
// side of a mountain, or two ways of life in one place). If it finds them, the
// cluster further from the founder becomes a new species and its members are
// relabelled (Simulation::check_splits). Still no randomness.

#include <cstdint>
#include <map>
#include <string>
#include <utility>
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

// The best single member a species has had, for the record book.
struct Record {
    uint32_t value = 0;   // ticks lived, bodies finished off, children
    uint64_t who = 0;     // its creature id
    uint64_t tick = 0;    // when it died (the record stands from then)
};

// What a species' members are like, averaged over the living at a census
// (every 50 ticks). Kept after extinction as the last word on it.
struct Habits {
    uint32_t count = 0;        // members counted; 0 = never censused
    uint64_t tick = 0;
    float diet[5] = {};        // share of food from light, the living, the dead, waste, vents (members old enough to say)
    float movers = 0.0f;       // shares of members, 0..1
    float sighted = 0.0f;
    float sleepers = 0.0f;     // dormancy gene above 0.1
    float armoured = 0.0f;
    float poisonous = 0.0f;
    float radius = 0.0f;       // means
    float age = 0.0f;
    float speed = 0.0f;        // top speed of its movers
    float light = 0.0f;        // where they live: noon light, daily mean temperature and height of their cells
    float temperature = 0.0f;
    float height = 0.0f;
    float x = 0.0f, y = 0.0f;  // the centre of their range
    float spread = 0.0f;       // how far from that centre they live, on average
};

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
    bool        by_split = false;      // found by a split check, not founded by a newborn
    std::vector<uint32_t> living_daughters;  // branched from this one and not yet extinct (pruned lazily)

    // The record book. Observation only: the viewer reads it, no rule of life does.
    uint64_t ever_born = 0;
    uint64_t died_eaten = 0;   // died while something was draining it
    uint64_t died_other = 0;   // starved, froze or wore out
    uint64_t kills = 0;        // bodies its members finished off, of any species
    Record   eldest, deadliest, most_children;   // among members that have died
    Habits   habits;

    bool alive() const { return population > 0; }
};

class SpeciesRegistry {
    friend struct SnapshotIO;   // snapshot.cpp: saves and restores the whole state
public:
    // A species with no ancestor in this world (the first LUCAs).
    uint32_t found_root(const Genome& g, uint64_t tick, std::string name = {});

    // The species of a newborn, given its parent's. May found a new one.
    uint32_t classify(uint32_t parent_species, const Genome& child, uint64_t tick);

    // The species of a creature arriving from outside: the nearest living
    // species if one is close enough, otherwise a new root. Not for hot paths.
    // A new root takes `name` if one is given (a creature file's).
    uint32_t classify_newcomer(const Genome& g, uint64_t tick, std::string name = {});

    // Bookkeeping. born() returns true the moment a species becomes
    // established; died() returns true when its last member dies.
    bool born(uint32_t species);
    bool died(uint32_t species, uint64_t tick);

    // A split: a new daughter of `from`, represented by `rep`, with no members
    // yet. Members are moved over one by one with move_member().
    uint32_t split_off(uint32_t from, const Genome& rep, uint64_t tick);
    // Relabel one living creature. Returns true the moment `to` becomes established.
    bool move_member(uint32_t from, uint32_t to);
    // The record book (observation only). note_death() takes the dead body's
    // life story; note_kill() that one species' member finished off another's.
    void note_death(uint32_t species, uint64_t who, uint32_t age, uint32_t kills, uint32_t children, bool eaten, uint64_t tick);
    void note_kill(uint32_t killer, uint32_t victim);
    Habits& habits(uint32_t species) { return species_[species].habits; }
    // Kills by (killer species, victim species), for the food web.
    const std::map<std::pair<uint32_t, uint32_t>, uint64_t>& kill_web() const { return kill_web_; }

    const std::vector<Species>& all() const { return species_; }
    const Species& get(uint32_t id) const { return species_[id]; }
    std::size_t living() const { return living_; }
    std::size_t living_established() const;

private:
    uint32_t found(uint32_t parent, const Genome& g, uint64_t tick, std::string name);

    std::vector<Species> species_;
    std::size_t living_ = 0;
    std::map<std::pair<uint32_t, uint32_t>, uint64_t> kill_web_;
};

// A pronounceable name, unique per id for the first 51,200 species ("Velox").
std::string species_name(uint32_t id);

// A short given name for one creature, from its id ("Tobi"): flavour for
// record holders. Not unique; species names are.
std::string creature_name(uint64_t id);

// A part type in plain words: "a mouth", "armour".
const char* part_noun(PartType t);

// What sets `child` apart from `parent`, as the end of a sentence:
// "has grown a mouth", "is smaller". Empty if nothing stands out.
std::string describe_difference(const Genome& child, const Genome& parent);

}  // namespace luca
