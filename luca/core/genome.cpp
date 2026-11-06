#include "genome.hpp"

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
        default:                  return "unknown";
    }
}

Genome make_luca() {
    Genome g;
    g.core_radius       = 1.0f;
    g.pigment           = 0.3f;
    g.thermal_optimum   = 20.0f;
    g.thermal_tolerance = 10.0f;
    g.repro_threshold   = 10.0f;
    g.offspring_share   = 0.5f;
    g.parts.push_back(Part{PartType::Photo, 1.0f, 0.0f, 0.0f});
    return g;
}

float total_part_size(const Genome& g, PartType t) {
    float s = 0.0f;
    for (const Part& p : g.parts)
        if (p.type == t) s += p.size;
    return s;
}

}  // namespace luca
