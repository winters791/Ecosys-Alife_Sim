#include "species.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace luca {

namespace {

// --- Distance weights (tune here) ---------------------------------------------
// Each term is scaled so that "clearly a different creature" in that one
// respect alone comes to roughly 1.
constexpr float W_RADIUS    = 2.0f;   // per e-fold of body size (x1.65 radius alone = 1)
constexpr float W_PIGMENT   = 2.0f;   // pale to dark is 0..1
constexpr float W_OPTIMUM   = 0.1f;   // per degree: 10 degrees apart = 1
constexpr float W_TOLERANCE = 0.05f;  // per degree
constexpr float W_THRESHOLD = 1.0f;   // per e-fold of reproduction threshold
constexpr float W_SHARE     = 1.0f;
constexpr float W_THROW     = 0.4f;   // per cell of seed-throwing range
constexpr float W_BITE      = 0.8f;   // per unit of bite
constexpr float W_DIGESTION = 2.0f;   // none to full is 0..1
constexpr float W_PART_KIND = 0.6f;   // one has a kind of part the other lacks entirely
constexpr float W_PART_SIZE = 0.5f;   // per unit of total size of one kind
constexpr float W_BRAIN     = 0.4f;   // one has a brain, the other none
constexpr float W_WEIGHTS   = 0.06f;  // per unit of link weight that differs (a link one lacks counts in full)
constexpr float W_NEURON    = 0.3f;   // per hidden neuron one has and the other lacks
// Shape genes are left out: today they only change the drawing.

constexpr int NUM_TYPES = static_cast<int>(PartType::Count);

struct PartSums {
    float size[NUM_TYPES] = {};
};

PartSums sum_parts(const Genome& g) {
    PartSums s;
    for (const Part& p : g.parts) s.size[static_cast<int>(p.type)] += p.size;
    return s;
}

}  // namespace

float genome_distance(const Genome& a, const Genome& b) {
    // Euclidean, not a plain sum: twenty genes each a hair apart is ordinary
    // variation inside a species, one gene far apart is a different creature.
    float sq = 0.0f;
    auto term = [&sq](float weight, float diff) { sq += (weight * diff) * (weight * diff); };

    term(W_RADIUS,    std::log(a.core_radius / b.core_radius));
    term(W_PIGMENT,   a.pigment - b.pigment);
    term(W_OPTIMUM,   a.thermal_optimum - b.thermal_optimum);
    term(W_TOLERANCE, a.thermal_tolerance - b.thermal_tolerance);
    term(W_THRESHOLD, std::log(a.repro_threshold / b.repro_threshold));
    term(W_SHARE,     a.offspring_share - b.offspring_share);
    term(W_THROW,     a.seed_throw - b.seed_throw);
    term(W_BITE,      a.bite - b.bite);
    term(W_DIGESTION, a.digestion - b.digestion);

    const PartSums pa = sum_parts(a), pb = sum_parts(b);
    for (int t = 0; t < NUM_TYPES; ++t) {
        term(W_PART_SIZE, pa.size[t] - pb.size[t]);
        if ((pa.size[t] > 0.0f) != (pb.size[t] > 0.0f)) term(W_PART_KIND, 1.0f);
    }

    if (a.brain.empty() != b.brain.empty()) {
        term(W_BRAIN, 1.0f);
    } else if (!a.brain.empty()) {
        // Wiring: add up, link by link, how far apart the two brains are.
        // Brains are small (at most MAX_LINKS links), so a plain search is fine.
        float sum = 0.0f;
        for (const BrainLink& la : a.brain.links) {
            float other = 0.0f;
            for (const BrainLink& lb : b.brain.links)
                if (lb.from == la.from && lb.to == la.to) { other = lb.weight; break; }
            sum += std::abs(la.weight - other);
        }
        for (const BrainLink& lb : b.brain.links) {
            bool shared = false;
            for (const BrainLink& la : a.brain.links) shared = shared || (la.from == lb.from && la.to == lb.to);
            if (!shared) sum += std::abs(lb.weight);
        }
        term(W_WEIGHTS, sum);
        term(W_NEURON, static_cast<float>(a.brain.hidden) - static_cast<float>(b.brain.hidden));
    }
    return std::sqrt(sq);
}

uint32_t SpeciesRegistry::found(uint32_t parent, const Genome& g, uint64_t tick, std::string name) {
    Species s;
    s.id = static_cast<uint32_t>(species_.size());
    s.parent = parent;
    s.name = name.empty() ? species_name(s.id) : std::move(name);
    s.representative = g;
    s.born_tick = tick;
    species_.push_back(std::move(s));
    if (parent != NO_SPECIES) species_[parent].living_daughters.push_back(species_.back().id);
    return species_.back().id;
}

uint32_t SpeciesRegistry::found_root(const Genome& g, uint64_t tick, std::string name) {
    return found(NO_SPECIES, g, tick, std::move(name));
}

uint32_t SpeciesRegistry::classify(uint32_t parent_species, const Genome& child, uint64_t tick) {
    if (genome_distance(child, species_[parent_species].representative) <= SPECIES_THRESHOLD)
        return parent_species;

    // Drifted too far. Siblings drift the same way, so look for a daughter
    // species that already holds creatures like this one before founding another.
    // Extinct daughters are dropped from the list as we pass, so this walks
    // living species only; the tree itself is kept by Species::parent.
    std::vector<uint32_t>& daughters = species_[parent_species].living_daughters;
    uint32_t best = NO_SPECIES;
    float best_d = SPECIES_THRESHOLD;
    std::size_t keep = 0;
    for (std::size_t k = 0; k < daughters.size(); ++k) {
        const Species& s = species_[daughters[k]];
        // Alive, or founded this very tick and still waiting for its founder to be added.
        if (!s.alive() && s.born_tick != tick) continue;
        daughters[keep++] = s.id;
        const float d = genome_distance(child, s.representative);
        if (d < best_d) {
            best_d = d;
            best = s.id;
        }
    }
    daughters.resize(keep);
    return best != NO_SPECIES ? best : found(parent_species, child, tick, {});
}

uint32_t SpeciesRegistry::classify_newcomer(const Genome& g, uint64_t tick) {
    uint32_t best = NO_SPECIES;
    float best_d = SPECIES_THRESHOLD;
    for (const Species& s : species_) {
        if (!s.alive()) continue;
        const float d = genome_distance(g, s.representative);
        if (d <= best_d) {
            best_d = d;
            best = s.id;
        }
    }
    return best != NO_SPECIES ? best : found_root(g, tick);
}

bool SpeciesRegistry::born(uint32_t species) {
    Species& s = species_[species];
    if (s.population++ == 0) ++living_;
    s.peak = std::max(s.peak, s.population);
    if (s.established || s.population < SPECIES_ESTABLISHED) return false;
    s.established = true;
    return true;
}

bool SpeciesRegistry::died(uint32_t species, uint64_t tick) {
    Species& s = species_[species];
    if (--s.population > 0) return false;
    s.extinct_tick = tick;
    --living_;
    return true;
}

std::size_t SpeciesRegistry::living_established() const {
    std::size_t n = 0;
    for (const Species& s : species_) n += (s.alive() && s.established) ? 1 : 0;
    return n;
}

std::string species_name(uint32_t id) {
    static const char* const onset[16] = {"V", "M", "T", "K", "S", "L", "R", "N",
                                          "D", "B", "G", "P", "Z", "H", "F", "Th"};
    static const char* const middle[16] = {"l", "r", "n", "m", "t", "k", "s", "d",
                                           "v", "b", "g", "p", "z", "x", "th", "nd"};
    static const char* const vowel[5] = {"a", "e", "i", "o", "u"};
    static const char* const ending[8] = {"x", "n", "s", "r", "l", "m", "a", "is"};
    constexpr uint32_t COMBOS = 16 * 5 * 16 * 5 * 8;  // 51,200

    // Multiplying by a number with no factor in common with COMBOS visits
    // every combination once before repeating, so neighbours in id don't
    // sound alike and no two of the first 51,200 share a name.
    uint32_t k = static_cast<uint32_t>((static_cast<uint64_t>(id) * 7919u + 20901u) % COMBOS);
    std::string name;
    name += onset[k % 16];  k /= 16;
    name += vowel[k % 5];   k /= 5;
    name += middle[k % 16]; k /= 16;
    name += vowel[k % 5];   k /= 5;
    name += ending[k % 8];
    if (id >= COMBOS) name += " " + std::to_string(id / COMBOS + 1);
    return name;
}

std::string describe_difference(const Genome& child, const Genome& parent) {
    static const char* const noun[NUM_TYPES] = {"a sun patch", "a mouth", "a tail", "legs",
                                                "an eye", "armour", "a fat store"};
    static const char* const lost[NUM_TYPES] = {"its sun patch", "its mouth", "its tail", "its legs",
                                                "its eyes", "its armour", "its fat store"};
    const PartSums c = sum_parts(child), p = sum_parts(parent);
    for (int t = 0; t < NUM_TYPES; ++t)
        if (c.size[t] > 0.0f && p.size[t] <= 0.0f) return std::string("has grown ") + noun[t];
    for (int t = 0; t < NUM_TYPES; ++t)
        if (c.size[t] <= 0.0f && p.size[t] > 0.0f) return std::string("has lost ") + lost[t];

    const float ratio = child.core_radius / parent.core_radius;
    if (ratio > 1.15f) return "is larger";
    if (ratio < 1.0f / 1.15f) return "is smaller";
    const float dt = child.thermal_optimum - parent.thermal_optimum;
    if (dt > 3.0f) return "likes it warmer";
    if (dt < -3.0f) return "likes it colder";
    const float dp = child.pigment - parent.pigment;
    if (dp > 0.15f) return "is darker";
    if (dp < -0.15f) return "is paler";
    return {};
}

}  // namespace luca
