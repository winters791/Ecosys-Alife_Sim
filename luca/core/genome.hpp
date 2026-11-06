#pragma once
// The genome: raw, inheritable data. Nothing derived lives here.
// It is the only thing mutation touches and the only thing a creature
// file exports. Everything else is recomputed from it.

#include <cstdint>
#include <vector>

namespace luca {

enum class PartType : uint8_t {
    Photo,      // turns light into energy
    Absorber,   // pulls energy/matter on contact (mouth)
    Flagellum,  // movement, good in water
    Legs,       // movement, good on land and slopes
    Sensor,     // sensing range
    Armour,     // resists absorption
    Storage,    // fat reserves
    Count
};

const char* part_name(PartType t);

struct Part {
    PartType type = PartType::Photo;
    float size  = 1.0f;
    float theta = 0.0f;  // position on blob surface (azimuth, radians)
    float phi   = 0.0f;  // position on blob surface (polar, radians)
};

struct Genome {
    float core_radius = 1.0f;
    float shape[8]    = {};      // lumps / elongation, consumed by the shader
    float pigment     = 0.3f;    // 0 = pale, 1 = dark
    float thermal_optimum   = 20.0f;  // °C
    float thermal_tolerance = 10.0f;  // ± °C of comfortable range
    float repro_threshold   = 10.0f;  // energy needed before dividing
    float offspring_share   = 0.5f;   // fraction of energy handed to the child
    std::vector<Part>  parts;
    std::vector<float> brain;    // empty for LUCA
};

// The last universal common ancestor: one core blob, one photosynthetic
// patch, no movement, no brain. Every lineage descends from this.
Genome make_luca();

// Sum of sizes of all parts of one type (handy for phenotype + metrics).
float total_part_size(const Genome& g, PartType t);

}  // namespace luca
