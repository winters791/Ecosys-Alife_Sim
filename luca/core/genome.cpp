#include "genome.hpp"

#include <algorithm>

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
    g.thermal_optimum   = 20.0f;
    g.thermal_tolerance = 10.0f;
    g.repro_threshold   = 600.0f;  // a child's body costs 244; this leaves both enough to last a night
    g.offspring_share   = 0.5f;
    g.seed_throw        = 0.0f;    // its children land right beside it
    g.parts.push_back(Part{PartType::Photo, 1.0f, 0.0f, 0.0f});
    return g;
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
    g.thermal_optimum   = std::clamp(g.thermal_optimum, -60.0f, 80.0f);
    g.thermal_tolerance = std::clamp(g.thermal_tolerance, MIN_TOLERANCE, MAX_TOLERANCE);
    g.repro_threshold   = std::clamp(g.repro_threshold, MIN_THRESHOLD, MAX_THRESHOLD);
    g.offspring_share   = std::clamp(g.offspring_share, MIN_SHARE, MAX_SHARE);
    g.seed_throw        = std::clamp(g.seed_throw, 0.0f, MAX_THROW);
    g.bite              = std::clamp(g.bite, 0.0f, MAX_BITE);
    g.digestion         = std::clamp(g.digestion, 0.0f, 1.0f);
    g.dormancy          = std::clamp(g.dormancy, 0.0f, 1.0f);
    if (g.parts.size() > static_cast<std::size_t>(MAX_PARTS)) g.parts.resize(MAX_PARTS);
    for (Part& p : g.parts) {
        if (static_cast<int>(p.type) >= static_cast<int>(PartType::Count)) p.type = PartType::Photo;
        p.size = std::clamp(p.size, MIN_PART, MAX_PART);
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
}

}  // namespace luca
