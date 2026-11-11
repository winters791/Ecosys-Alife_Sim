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
    Poison,     // makes the body costly to eat
    Count
};

const char* part_name(PartType t);

// Only mouths (tentacles) and sun patches (fronds) grow on stalks; on any
// other part a stalk would do nothing, so the genome never holds one.
inline bool can_have_stalk(PartType t) { return t == PartType::Absorber || t == PartType::Photo; }

struct Part {
    PartType type = PartType::Photo;
    float size  = 1.0f;
    float theta = 0.0f;  // where on the outline it sits: radians from the body's front, 0 front, pi back
    float phi   = 0.0f;  // unused (left over from a 3D surface position); kept so old genomes load
    float stalk = 0.0f;  // how far out on a stalk it sits, in world units; 0 = on the skin
};

// --- The brain ---------------------------------------------------------------
// A small network that is itself evolved: how many neurons it has and what is
// wired to what are part of the genome, like the list of body parts.
//
//   inputs  ->  (hidden neurons, any number from none to MAX_HIDDEN)  ->  outputs
//
// A link carries one node's value to another, times a weight. With no hidden
// neurons the brain is a plain set of reflexes: "lean towards light, more so
// when hungry". Hidden neurons (tanh) let it combine senses, and because a
// hidden neuron may listen to hidden neurons (its own value from the tick
// before, or another's), a brain can also remember.
//
// Nothing here says "chase" or "flee". An output is only how hard to lean
// towards (+) or away from (-) one thing the body can sense; selection finds
// the wiring. Every neuron, every link and every unit of weight costs upkeep
// (phenotype.cpp), so a brain grows only where thinking pays for itself.
enum BrainInput : int {
    IN_BIAS = 0,   // always 1: a link from here is a fixed urge
    IN_HUNGER,     // 0 full .. 1 starving
    IN_LIGHT,      // light falling on it now
    IN_CROWD,      // living mass in its own cell
    IN_COLD,       // -1 too hot .. +1 too cold for its comfort
    IN_CORPSE,     // dead matter in its own cell
    IN_MOVED,      // how fast it went last tick
    NUM_INPUTS
};
enum BrainOutput : int {
    // Leaning: towards where there is more of something. The first five follow
    // the slope of a field and need a Sensor part to be felt at all.
    OUT_LIGHT = 0,
    OUT_BIOMASS,   // living bodies (prey, or a crowd)
    OUT_CORPSE,    // the dead
    OUT_COMFORT,   // its comfortable temperature
    OUT_NUTRIENT,  // free matter in the soil
    // The danger / kin sense, also needing a Sensor: each body nearby pulls (+)
    // or pushes (-) by OUT_KIN if it is the creature's own species, OUT_BIGGER
    // times how much bigger it is (negative when smaller), and OUT_MOVING times
    // how fast it is moving.
    OUT_KIN,
    OUT_BIGGER,
    OUT_MOVING,
    OUT_WANDER,    // a random direction
    OUT_INERTIA,   // the way it was already going
    OUT_THROTTLE,  // how fast to go: speed is 0.5 + this, clamped to 0..1
    NUM_OUTPUTS
};
constexpr int MAX_HIDDEN = 8;
constexpr int MAX_LINKS  = 48;

// Node numbers used by links: inputs first, then outputs, then hidden neurons.
constexpr int FIRST_OUTPUT = static_cast<int>(NUM_INPUTS);
constexpr int FIRST_HIDDEN = static_cast<int>(NUM_INPUTS) + static_cast<int>(NUM_OUTPUTS);
constexpr int input_node(BrainInput i) { return i; }
constexpr int output_node(BrainOutput o) { return FIRST_OUTPUT + o; }
constexpr int hidden_node(int k) { return FIRST_HIDDEN + k; }

struct BrainLink {
    uint8_t from = 0;   // an input or a hidden neuron
    uint8_t to   = 0;   // a hidden neuron or an output
    float   weight = 0.0f;
};

struct Brain {
    uint8_t hidden = 0;             // number of hidden neurons
    std::vector<BrainLink> links;   // no links = no brain

    bool empty() const { return links.empty(); }
    void clear() { hidden = 0; links.clear(); }
    // Set the weight from one node to another (adding the link if need be).
    void connect(int from, int to, float weight);
    // Take out hidden neuron k and every link that touches it.
    void remove_hidden(int k);
};

struct Genome {
    float core_radius = 1.0f;
    float shape[8]    = {};      // lumps / elongation, consumed by the shader
    float pigment     = 0.3f;    // 0 = pale, 1 = dark
    float thermal_optimum   = 20.0f;  // °C
    float thermal_tolerance = 10.0f;  // ± °C of comfortable range
    float repro_threshold   = 10.0f;  // energy needed before dividing
    float offspring_share   = 0.5f;   // fraction of energy handed to the child
    float bite              = 0.0f;   // drains the living faster: pull is multiplied by 1 + bite
    float digestion         = 0.0f;   // 0..1: keeps more of what it eats (70% up to 95%)
    float dormancy          = 0.0f;   // 0..1: how deeply it sleeps through dark and cold
    float seed_throw        = 0.0f;   // how far it can fling a child, in cells; 0 = the child lands touching it
    float detox             = 0.0f;   // 0..1: takes up the waste in its cell as food
    // Body outline (2026-11-11). A LUCA is round: both 0.
    float elongation        = 0.0f;   // 0 round .. 1 three times as long as it is wide
    float squareness        = 0.0f;   // 0 rounded .. 1 a box with rounded corners
    std::vector<Part>  parts;
    Brain brain;                 // empty = no behaviour; see above
};

// Gene ranges. clamp_genome() is the one place they are enforced.
namespace limits {
constexpr float MIN_RADIUS = 0.3f,  MAX_RADIUS = 5.0f;
constexpr float MIN_PART   = 0.05f, MAX_PART   = 4.0f;
constexpr int   MAX_PARTS  = 12;     // upkeep already prices parts; this just bounds the file size
constexpr float MIN_TOLERANCE = 0.5f, MAX_TOLERANCE = 30.0f;
constexpr float MIN_THRESHOLD = 2.0f, MAX_THRESHOLD = 2000.0f;
constexpr float MIN_SHARE = 0.05f,  MAX_SHARE = 0.95f;
constexpr float MAX_WEIGHT = 3.0f;
constexpr float MAX_THROW  = 6.0f;   // cells
constexpr float MAX_BITE   = 3.0f;   // at most four times the plain pull
constexpr float MAX_STALK  = 1.0f;   // world units: a part can sit at most this far out
}  // namespace limits

// Pull every gene back inside its range. Called after mutation and on import.
void clamp_genome(Genome& g);

// The last universal common ancestor: one core blob, one photosynthetic
// patch, no movement, no brain. Every lineage descends from this.
Genome make_luca();

// Sum of sizes of all parts of one type (handy for phenotype + metrics).
float total_part_size(const Genome& g, PartType t);

}  // namespace luca
