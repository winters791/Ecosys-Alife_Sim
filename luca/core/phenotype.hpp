#pragma once
// Phenotype: stats DERIVED from the genome, computed at birth and again each
// time a young body grows (see `grown`). The tick reads only these, which
// keeps it fast.
//
// You own this struct too — add fields as your rules need them.

#include "colour.hpp"
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
    Rgb   skin;                      // skin colour, from hue and pigment (colour.hpp); drawing only
    float heat_follow       = 1.0f;  // fraction of the gap to the air its body temperature closes per tick
    float grown             = 1.0f;  // fraction of adult mass it has built so far; 1 = adult, able to breed
    float birth_size        = 1.0f;  // fraction of adult mass its children are born at
    // Stoichiometry (docs/design/stoichiometry.md): the mineral P its body is
    // built of. p_need is what this body holds in its tissue at its present
    // size; p_adult the same grown; p_store_cap the most P its store keeps.
    // All 0 while the P_BASE law is 0 (off).
    float p_need            = 0.0f;
    float p_adult           = 0.0f;
    float p_store_cap       = 0.0f;
    float gear_upkeep       = 0.0f;  // observation only: the part of `upkeep` that is standing gear (mouths,
                                     // motors, senses, bite, brain, enzymes), paid used or not (2.8 cause 2)

    // Contact (the one interaction primitive, see energy.hpp)
    float absorb   = 0.0f;   // how hard it pulls energy across a membrane it touches
    float membrane = 0.0f;   // how well its own membrane resists being pulled from
    float toxicity   = 0.0f; // energy an eater loses per unit of energy it drains from this body
    float bite       = 1.0f; // multiplies its pull on living bodies (1 = plain)
    float efficiency = 0.0f; // fraction of what it pulls, from the living or the dead, that it keeps

    float energy_capacity = 0.0f;  // the most energy the body can hold (a store full of D)
    float pool_capacity   = 0.0f;  // the most chemical its store can hold, units of mass (energy_capacity / TOP_LEVEL)

    // Chemistry (chem.hpp): its enzymes, each written the way it runs when
    // burning: `in` (the richer side) to `out` (the poorer side), the second
    // slot NO_CHEM for a one-to-one reaction (genome.hpp); `step` the energy
    // released per unit (0 for one with equal levels both sides, which never
    // runs); `rate` the most units it can turn over a tick, each way.
    // Charging runs them out -> in.
    uint8_t enzymes = 0;
    uint8_t enz_in[limits::MAX_ENZYMES][2] = {};
    uint8_t enz_out[limits::MAX_ENZYMES][2] = {};
    float   enz_step[limits::MAX_ENZYMES] = {};
    float   enz_rate[limits::MAX_ENZYMES] = {};
    bool    any_two = false; // it carries a two-input enzyme
    // Which chemicals it can burn: bit c is set when some enzyme has c on its
    // richer side (of a downhill reaction). It
    // takes these up from its cell and keeps them from a meal; anything else
    // in its store it gives off (chemistry stage 2, energy.hpp).
    uint8_t burns = 0;
    bool can_burn(int c) const { return (burns >> c) & 1u; }
    float reach           = 0.0f;  // touch distance from its centre, world units

    // Behaviour
    int   sense_cells = 0;         // how far its sensors look, in cells; 0 = blind
    bool  has_brain   = false;     // the wiring itself is compiled separately: see BrainPlan

    // Summed size of each part type (for the renderer and metrics).
    float part_total[static_cast<int>(PartType::Count)] = {};
};

// Genome -> Phenotype. YOURS TO WRITE (see phenotype.cpp).
// `grown` (0.1..1) builds a young body: the adult scaled down to that
// fraction of its mass (see phenotype.cpp).
Phenotype compile_phenotype(const Genome& g, float grown = 1.0f);

// The brain, laid out for running: a fixed-size list of links, those feeding
// hidden neurons first, then those feeding outputs. Kept beside the phenotype,
// not in it, because only bodies that move ever read it.
// One point a sensor reads (SENSOR_BRAIN, 14.9): a channel, an angle from the
// body's front, how far out (world units) and its size. A sensor part off the
// midline is a mirrored pair: two of these, at +theta and -theta.
struct SensorPoint {
    uint8_t channel = 0;
    float   angle = 0.0f;   // radians from the heading; positive = to the left
    float   range = 0.0f;
    float   size = 0.0f;
};
constexpr int MAX_SENSOR_POINTS = 8;

struct BrainPlan {
    uint8_t sensors = 0;
    SensorPoint sensor[MAX_SENSOR_POINTS];
    uint8_t hidden = 0;          // hidden neurons
    uint8_t to_hidden = 0;       // links [0, to_hidden) feed hidden neurons
    uint8_t links = 0;           // links [to_hidden, links) feed outputs
    float   rate[MAX_HIDDEN] = {};   // 1 / tau: how far a neuron's state moves each tick
    float   bias[MAX_HIDDEN] = {};
    BrainLink link[MAX_LINKS];
};
// What a brain carries from one tick to the next: its hidden neurons' states
// (a CTRNN neuron fires tanh(state + bias)).
struct BrainState {
    float hidden[MAX_HIDDEN] = {};
};

BrainPlan compile_brain(const Genome& g);

// One tick of thought: inputs (NUM_INPUTS values) -> outputs (NUM_OUTPUTS
// values, each within +-limits::MAX_WEIGHT). Updates the hidden state.
void think(const BrainPlan& plan, BrainState& state, const float* inputs, float* outputs);

}  // namespace luca
