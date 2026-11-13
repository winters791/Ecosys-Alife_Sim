#include "genome.hpp"
#include "laws.hpp"

#include <algorithm>
#include <cmath>

namespace luca {

const char* part_name(PartType t) {
    switch (t) {
        case PartType::Photo:     return "photo";
        case PartType::Absorber:  return "absorber";
        case PartType::Flagellum: return "flagellum";
        case PartType::Legs:      return "legs";
        case PartType::Sensor:    return "sensor";
        case PartType::Armour:    return "armour";
        case PartType::Storage:   return "storage";
        case PartType::Poison:    return "poison";
        default:                  return "unknown";
    }
}

Genome make_luca() {
    Genome g;
    g.core_radius       = 1.0f;
    g.pigment           = 0.3f;
    g.hue               = 0.09f;   // tan
    g.thermal_optimum   = 20.0f;
    g.thermal_tolerance = 10.0f;
    g.repro_threshold   = 600.0f;  // a child's body costs 244; this leaves both enough to last a night
    g.offspring_share   = 0.5f;
    g.seed_throw        = 0.0f;    // its children land right beside it
    g.birth_size        = 0.5f;    // its children are born at half its mass and grow the rest
    g.parts.push_back(Part{PartType::Photo, 1.0f, 0.0f, 0.0f, 0.0f});
    // Its whole chemistry: A <-> D. Light runs it up (A from the soil into D in
    // its store), and it burns D back to A to live. 0.015 a tick is 3.3 energy
    // a tick each way, several times what it burns or catches at noon.
    g.enzymes.push_back(Enzyme{CHEM_A, CHEM_D, 0.015f});
    return g;
}

Enzyme canonical_enzyme(Enzyme e) {
    auto in_range = [](uint8_t c) { return static_cast<uint8_t>(std::min<int>(c, NCHEM - 1)); };
    e.x = in_range(e.x);
    e.y = in_range(e.y);
    if (e.x2 == NO_CHEM || e.y2 == NO_CHEM) {
        e.x2 = e.y2 = NO_CHEM;
        if (e.x == e.y) e.y = static_cast<uint8_t>((e.x + 1) % NCHEM);
        if (e.x > e.y) std::swap(e.x, e.y);
        return e;
    }
    e.x2 = in_range(e.x2);
    e.y2 = in_range(e.y2);
    if (e.x > e.x2) std::swap(e.x, e.x2);
    if (e.y > e.y2) std::swap(e.y, e.y2);
    if (e.x == e.y && e.x2 == e.y2) {   // the same on both sides: nothing happens; mend one end
        e.y2 = static_cast<uint8_t>((e.y2 + 1) % NCHEM);
        if (e.y > e.y2) std::swap(e.y, e.y2);
    }
    const float lo = LEVEL[e.x] + LEVEL[e.x2], hi = LEVEL[e.y] + LEVEL[e.y2];
    if (lo > hi || (lo == hi && (e.x > e.y || (e.x == e.y && e.x2 > e.y2)))) {
        std::swap(e.x, e.y);
        std::swap(e.x2, e.y2);
    }
    return e;
}

std::string enzyme_words(const Enzyme& e) {
    std::string s = chem_name(e.x);
    if (e.two()) s += std::string("+") + chem_name(e.x2);
    s += std::string("-") + chem_name(e.y);
    if (e.two()) s += std::string("+") + chem_name(e.y2);
    return s;
}

void Brain::connect(int from, int to, float weight) {
    for (BrainLink& l : links)
        if (l.from == from && l.to == to) {
            l.weight = weight;
            return;
        }
    links.push_back({static_cast<uint8_t>(from), static_cast<uint8_t>(to), weight});
}

void Brain::remove_hidden(int k) {
    if (k < 0 || k >= hidden) return;
    const int gone = hidden_node(k);
    std::erase_if(links, [&](const BrainLink& l) { return l.from == gone || l.to == gone; });
    for (BrainLink& l : links) {   // the neurons after it move down one
        if (l.from > gone) --l.from;
        if (l.to > gone) --l.to;
    }
    for (int j = k; j + 1 < hidden; ++j) { tau[j] = tau[j + 1]; bias[j] = bias[j + 1]; }
    tau[hidden - 1] = 1.0f;
    bias[hidden - 1] = 0.0f;
    --hidden;
}

float total_part_size(const Genome& g, PartType t) {
    float s = 0.0f;
    for (const Part& p : g.parts)
        if (p.type == t) s += p.size;
    return s;
}

void clamp_genome(Genome& g) {
    using namespace limits;
    g.core_radius = std::clamp(g.core_radius, MIN_RADIUS, MAX_RADIUS);
    for (float& s : g.shape) s = std::clamp(s, -1.0f, 1.0f);
    g.pigment           = std::clamp(g.pigment, 0.0f, 1.0f);
    g.hue               = std::isfinite(g.hue) ? g.hue - std::floor(g.hue) : 0.09f;   // a wheel: wraps round
    g.thermal_optimum   = std::clamp(g.thermal_optimum, -60.0f, 80.0f);
    g.thermal_tolerance = std::clamp(g.thermal_tolerance, MIN_TOLERANCE, MAX_TOLERANCE);
    g.repro_threshold   = std::clamp(g.repro_threshold, MIN_THRESHOLD, MAX_THRESHOLD);
    g.offspring_share   = std::clamp(g.offspring_share, MIN_SHARE, MAX_SHARE);
    g.seed_throw        = std::clamp(g.seed_throw, 0.0f, MAX_THROW);
    g.bite              = std::clamp(g.bite, 0.0f, MAX_BITE);
    g.dormancy          = std::clamp(g.dormancy, 0.0f, 1.0f);
    g.elongation        = std::clamp(g.elongation, 0.0f, 1.0f);
    g.squareness        = std::clamp(g.squareness, 0.0f, 1.0f);
    g.birth_size        = std::clamp(g.birth_size, MIN_BIRTH, MAX_BIRTH);
    // Ablation switches (laws.hpp): a gene switched off can never be carried.
    if (g_laws.USE_BITE == 0.0f) g.bite = 0.0f;
    if (g.parts.size() > static_cast<std::size_t>(MAX_PARTS)) g.parts.resize(MAX_PARTS);
    for (Part& p : g.parts) {
        if (static_cast<int>(p.type) >= static_cast<int>(PartType::Count)) p.type = PartType::Photo;
        p.size = std::clamp(p.size, MIN_PART, MAX_PART);
        p.stalk = can_have_stalk(p.type) ? std::clamp(p.stalk, 0.0f, MAX_STALK) : 0.0f;
        p.theta = std::remainder(p.theta, 6.2831853f);  // -pi .. pi
    }
    // Enzymes: canonical (see canonical_enzyme), rate within bounds.
    if (g.enzymes.size() > static_cast<std::size_t>(MAX_ENZYMES)) g.enzymes.resize(MAX_ENZYMES);
    for (Enzyme& e : g.enzymes) {
        e = canonical_enzyme(e);
        e.rate = std::isfinite(e.rate) ? std::clamp(e.rate, MIN_ENZYME_RATE, MAX_ENZYME_RATE) : MIN_ENZYME_RATE;
    }
    // The brain: drop links that point at nothing or run the wrong way, and
    // keep its size within bounds.
    Brain& b = g.brain;
    b.hidden = static_cast<uint8_t>(std::min<int>(b.hidden, MAX_HIDDEN));
    const int nodes = FIRST_HIDDEN + b.hidden;
    std::erase_if(b.links, [&](const BrainLink& l) {
        const bool from_ok = l.from < NUM_INPUTS || (l.from >= FIRST_HIDDEN && l.from < nodes);
        const bool to_ok = l.to >= FIRST_OUTPUT && l.to < nodes;
        return !from_ok || !to_ok;
    });
    if (b.links.size() > static_cast<std::size_t>(MAX_LINKS)) b.links.resize(MAX_LINKS);
    for (BrainLink& l : b.links) l.weight = std::clamp(l.weight, -MAX_WEIGHT, MAX_WEIGHT);
    if (b.links.empty()) b.hidden = 0;
    for (int k = 0; k < MAX_HIDDEN; ++k) {
        const bool used = k < b.hidden;
        b.tau[k] = used && std::isfinite(b.tau[k]) ? std::clamp(b.tau[k], MIN_TAU, MAX_TAU) : 1.0f;
        b.bias[k] = used && std::isfinite(b.bias[k]) ? std::clamp(b.bias[k], -MAX_WEIGHT, MAX_WEIGHT) : 0.0f;
    }
}

}  // namespace luca
