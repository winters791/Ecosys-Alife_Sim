#pragma once
// Phenotype: stats DERIVED from the genome, computed once at birth and
// never changed. The tick reads only these, which keeps it fast.
//
// You own this struct too — add fields as your rules need them.

#include "genome.hpp"

namespace luca {

struct Phenotype {
    float mass          = 0.0f;
    float radius        = 0.0f;  // core radius: how tall it stands in the canopy
    float surface_area  = 0.0f;
    float upkeep        = 0.0f;  // energy per tick just to stay alive
    float light_capture = 0.0f;  // how strongly it absorbs light (see energy.hpp)
    float max_speed     = 0.0f;  // world units per tick; 0 = cannot move
    float drag          = 1.0f;  // moving cost and speed, against a round body's (body.hpp)
    float canopy        = 0.0f;  // how high its sun patches stand: core radius, plus their stalks
    float repro_threshold   = 0.0f;
    float offspring_share   = 0.0f;
    float dormancy          = 0.0f;  // how deeply it can sleep, 0..1 (see energy.hpp)
    float throw_range       = 0.0f;  // furthest it can fling a child beyond touching, world units
    float thermal_optimum   = 0.0f;
    float thermal_tolerance = 0.0f;
    float heat_absorption   = 0.0f;  // from pigment
    float heat_follow       = 1.0f;  // fraction of the gap to the air its body temperature closes per tick

    // Contact (the one interaction primitive, see energy.hpp)
    float absorb   = 0.0f;   // how hard it pulls energy across a membrane it touches
    float membrane = 0.0f;   // how well its own membrane resists being pulled from
    float toxicity   = 0.0f; // energy an eater loses per unit of energy it drains from this body
    float bite       = 1.0f; // multiplies its pull on living bodies (1 = plain)
    float efficiency = 0.0f; // fraction of what it pulls, from the living or the dead, that it keeps
    float detox      = 0.0f; // 0..1: how hard it takes up waste (see energy.hpp)

    float energy_capacity = 0.0f;  // the most energy the body can hold
    float body_energy     = 0.0f;  // energy locked in the body itself; becomes the corpse
    float reach           = 0.0f;  // touch distance from its centre, world units

    // Behaviour
    int   sense_cells = 0;         // how far its sensors look, in cells; 0 = blind
    bool  has_brain   = false;     // the wiring itself is compiled separately: see BrainPlan

    // Summed size of each part type (for the renderer and metrics).
    float part_total[static_cast<int>(PartType::Count)] = {};
};

// Genome -> Phenotype. YOURS TO WRITE (see phenotype.cpp).
Phenotype compile_phenotype(const Genome& g);

// The brain, laid out for running: a fixed-size list of links, those feeding
// hidden neurons first, then those feeding outputs. Kept beside the phenotype,
// not in it, because only bodies that move ever read it.
struct BrainPlan {
    uint8_t hidden = 0;          // hidden neurons
    uint8_t to_hidden = 0;       // links [0, to_hidden) feed hidden neurons
    uint8_t links = 0;           // links [to_hidden, links) feed outputs
    BrainLink link[MAX_LINKS];
};
// What a brain carries from one tick to the next: its hidden neurons' values.
struct BrainState {
    float hidden[MAX_HIDDEN] = {};
};

BrainPlan compile_brain(const Genome& g);

// One tick of thought: inputs (NUM_INPUTS values) -> outputs (NUM_OUTPUTS
// values, each within +-limits::MAX_WEIGHT). Updates the hidden state.
void think(const BrainPlan& plan, BrainState& state, const float* inputs, float* outputs);

}  // namespace luca
