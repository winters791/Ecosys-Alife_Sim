#include "phenotype.hpp"

#include <algorithm>
#include <cmath>

namespace luca {

namespace {  // only visible inside this file

constexpr int NUM_PARTS = static_cast<int>(PartType::Count);

// Order MUST match the PartType enum in genome.hpp:
//                                           Photo  Absorber Flagellum Legs  Sensor Armour Storage
constexpr float MASS_PER_SIZE[NUM_PARTS] = { 0.1f,  0.2f,    0.15f,    0.3f, 0.05f, 0.6f,  0.4f };
constexpr float COST_PER_SIZE[NUM_PARTS] = { 0.1f,  0.15f,   0.2f,     0.2f, 0.3f,  0.05f, 0.02f };

constexpr float BASE_UPKEEP    = 0.3f;
constexpr float PHOTO_PER_AREA = 1.0f;   // max photo patch size per unit of skin

constexpr float FLAGELLUM_THRUST = 0.5f; // world units per tick, per unit size, per unit mass
constexpr float LEGS_THRUST      = 0.4f;
constexpr float MAX_SPEED        = 2.0f; // hard physical ceiling (a law, not a population cap)

// Gene ranges. Clamping happens here, and only here.
constexpr float MIN_RADIUS = 0.3f;
constexpr float MAX_RADIUS = 5.0f;

}  // namespace

// =============================================================================
// THE PHENOTYPE COMPILER (Uday's)
//
// Done:
//   1. Clamping (radius, part sizes, pigment, reproduction, thermal tolerance)
//   2. Body scaling: mass ∝ r³ (+ parts), surface area ∝ r²
//   3. Upkeep: Kleiber's law on total mass + per-part running costs
//   4. Light capture: photo patch total, capped by available skin
//   5. Max speed: thrust from flagella + legs, divided by mass, capped
//   6. Heat absorption: pigment × surface area / mass (small bodies swing more)
//
// Later:
//   * Extra upkeep for wide thermal tolerance, so generalists pay
//   * Split max_speed into swim_speed (flagella) and walk_speed (legs) once
//     the world has water
//
// Rule of thumb: every number you set here should have a cost somewhere.
// =============================================================================

Phenotype compile_phenotype(const Genome& g) {
    Phenotype p;

    // --- 1. Clamp genes that mutation can push out of range -----------------
    const float r       = std::clamp(g.core_radius, MIN_RADIUS, MAX_RADIUS);
    const float pigment = std::clamp(g.pigment, 0.0f, 1.0f);

    // --- 2 + 3. One pass over the parts: mass, upkeep, per-type totals -------
    float mass            = r * r * r;  // core blob
    float part_upkeep     = 0.0f;
    float photo_total     = 0.0f;
    float flagellum_total = 0.0f;
    float legs_total      = 0.0f;

    for (const Part& part : g.parts) {
        const int   t    = static_cast<int>(part.type);
        const float size = std::max(0.0f, part.size);  // negative parts would be free energy
        mass        += size * MASS_PER_SIZE[t];
        part_upkeep += size * COST_PER_SIZE[t];

        if (part.type == PartType::Photo)     photo_total     += size;
        if (part.type == PartType::Flagellum) flagellum_total += size;
        if (part.type == PartType::Legs)      legs_total      += size;
    }

    p.mass         = mass;
    p.surface_area = r * r;
    p.upkeep       = BASE_UPKEEP * std::pow(mass, 0.75f) + part_upkeep;  // Kleiber + parts
    // LUCA (r = 1, one photo patch of size 1): mass 1.1, upkeep ≈ 0.42.

    // --- 4. Light capture: patches need skin to sit on ----------------------
    p.light_capture = std::min(photo_total, PHOTO_PER_AREA * p.surface_area);

    // --- 5. Movement: thrust / mass, with a physical ceiling ----------------
    const float thrust = flagellum_total * FLAGELLUM_THRUST + legs_total * LEGS_THRUST;
    p.max_speed = std::min(thrust / p.mass, MAX_SPEED);
    // LUCA + one size-1 flagellum: mass 1.25, speed 0.4.

    // --- 6. Heat: absorbed over skin, spread over body mass ------------------
    p.heat_absorption = pigment * p.surface_area / p.mass;
    // LUCA: 0.3 × 1 / 1.1 ≈ 0.27.

    // --- 7. Copy-through genes (clamped) -------------------------------------
    p.repro_threshold   = std::clamp(g.repro_threshold, 1.0f, 100.0f);
    p.offspring_share   = std::clamp(g.offspring_share, 0.05f, 0.95f);
    p.thermal_optimum   = g.thermal_optimum;
    p.thermal_tolerance = std::max(0.0f, g.thermal_tolerance);

    return p;
}

}  // namespace luca
