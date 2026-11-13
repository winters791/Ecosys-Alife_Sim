#pragma once
// The genome: raw, inheritable data. Nothing derived lives here.
// It is the only thing mutation touches and the only thing a creature
// file exports. Everything else is recomputed from it.

#include <cstdint>
#include <string>
#include <vector>

#include "chem.hpp"

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

// An enzyme (chemistry layer, design doc 2.1.1): a reversible reaction
// between a poorer side and a richer side, run at most `rate` units a tick
// each way. One-to-one, x <-> y; or two-input (chemistry stage 4),
// x + x2 <-> y + y2, one unit of each, so matter balances. Downhill (richer to
// poorer) it releases the difference in levels (chem.hpp) for the body to
// spend; uphill it stores light in the richer side. A reaction that releases
// nothing either way (equal levels, e.g. B + C <-> A + D) never runs: it only
// costs upkeep (neutral exchanges were deleted after the ablation pass,
// 14.10.8). A from or to A draws on or gives to the cell's
// soil. Every enzyme costs upkeep in proportion to its rate (phenotype.cpp).
constexpr uint8_t NO_CHEM = 0xFF;   // the second slot of a one-to-one enzyme
struct Enzyme {
    uint8_t x = CHEM_A, y = CHEM_D;   // the poorer and richer side (canonical_enzyme)
    float   rate = 0.0f;
    uint8_t x2 = NO_CHEM, y2 = NO_CHEM;   // second input and output; NO_CHEM on both for one-to-one
    bool two() const { return x2 != NO_CHEM; }
};
// The one way an enzyme is written: chemicals in range; both second slots set
// or neither; each side sorted; the poorer side first (by level; for a
// neutral reaction, the side that sorts first); the two sides never the same.
Enzyme canonical_enzyme(Enzyme e);
// E.g. "A-D" or "A+D-B+C" (poorer side first), for files and the viewer.
std::string enzyme_words(const Enzyme& e);

struct Part {
    PartType type = PartType::Photo;
    float size  = 1.0f;
    float theta = 0.0f;  // where on the outline it sits: radians from the body's front, 0 front, pi back
    float phi   = 0.0f;  // unused (left over from a 3D surface position); kept so old genomes load
    float stalk = 0.0f;  // how far out on a stalk it sits, in world units; 0 = on the skin
    uint8_t channel = 0; // a sensor's channel: which field it reads (SenseChannel); ignored on other parts
};

// The fields a sensor can read (14.9, stage 1). Adding a sense is adding a row
// here and in Simulation::sample_channel; no existing brain changes.
enum SenseChannel : int {
    CH_LIGHT = 0,
    CH_WARMTH,     // temperature against the body's own comfort, signed
    CH_CORPSE,     // dead matter's energy
    CH_NUTRIENT,   // free matter in the soil (chemical A)
    CH_LOOSE_B,
    CH_LOOSE_C,
    CH_LOOSE_D,
    CH_LIVING,     // living mass
    CH_COUNT
};
const char* channel_name(int c);

// --- The brain ---------------------------------------------------------------
// A small network that is itself evolved: how many neurons it has and what is
// wired to what are part of the genome, like the list of body parts.
//
//   inputs  ->  (hidden neurons, any number from none to MAX_HIDDEN)  ->  outputs
//
// A link carries one node's value to another, times a weight. With no hidden
// neurons the brain is a plain set of reflexes: "lean towards light, more so
// when hungry". Hidden neurons let it combine senses, and because a hidden
// neuron may listen to hidden neurons (its own value from the tick before, or
// another's), a brain can also remember.
//
// Hidden neurons are a CTRNN (continuous-time recurrent net, 2026-11-12).
// Each has a state y, a time constant tau (ticks) and a bias; it fires
// tanh(y + bias), and each tick its state moves 1/tau of the way towards what
// its links feed it (Euler, one tick a step). A neuron with tau = 1 forgets
// at once, as every neuron did before; a slow one smooths its inputs, holds
// on to what it felt hours ago, or with links back to itself and others can
// keep a rhythm. Tau and bias are genes of the neuron; both evolve.
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
    // Touch (2026-11-12): what crossed its membrane last tick. Every body
    // feels these; no sensor is needed.
    IN_EATING,     // what it took in from the living and the dead: 1 = twice its upkeep
    IN_HURT,       // what was drained out of it: 1 = its upkeep
    IN_NOISE,      // a number from a hash of (its id, the tick), -1 .. 1: wire it to wander (SENSOR_BRAIN)
    // The bus (SENSOR_BRAIN, 14.9): per channel, how much its sensors feel
    // (level) and how much more on the left than the right (contrast). The
    // brain never names a part.
    IN_BUS,
    NUM_INPUTS = IN_BUS + 2 * CH_COUNT
};
constexpr int LEGACY_INPUTS = IN_NOISE;   // inputs before 2026-11-13
constexpr int bus_level(int channel)    { return IN_BUS + 2 * channel; }
constexpr int bus_contrast(int channel) { return IN_BUS + 2 * channel + 1; }
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
    // SENSOR_BRAIN (14.9): the brain drives the body directly and the lean
    // outputs above are not read.
    OUT_THRUST,    // 0 .. 1 of the body's top speed along its heading (negative = 0)
    OUT_TURN,      // -1 .. 1 of the most it can turn a tick (positive = to its left)
    NUM_OUTPUTS
};
constexpr int LEGACY_OUTPUTS = OUT_THRUST;
constexpr int MAX_HIDDEN = 8;
constexpr int MAX_LINKS  = 48;

// Node numbers used by links: inputs first, then outputs, then hidden neurons.
// Fixed offsets, so a new input or output renumbers no saved brain (2026-11-13).
// Before that they followed the counts: outputs at 9 and hidden at 20 (LEGACY_*
// in snapshot.cpp translates old files).
constexpr int FIRST_OUTPUT = 32;
constexpr int FIRST_HIDDEN = 48;
static_assert(NUM_INPUTS <= FIRST_OUTPUT && FIRST_OUTPUT + NUM_OUTPUTS <= FIRST_HIDDEN, "node ranges overlap");
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
    // Per hidden neuron (only the first `hidden` mean anything).
    float tau[MAX_HIDDEN];          // time constant in ticks, 1 .. limits::MAX_TAU
    float bias[MAX_HIDDEN];         // added to its state before it fires

    Brain() { reset_neurons(); }
    bool empty() const { return links.empty(); }
    void clear() { hidden = 0; links.clear(); reset_neurons(); }
    void reset_neurons() {
        for (int k = 0; k < MAX_HIDDEN; ++k) { tau[k] = 1.0f; bias[k] = 0.0f; }
    }
    // Set the weight from one node to another (adding the link if need be).
    void connect(int from, int to, float weight);
    // Take out hidden neuron k and every link that touches it.
    void remove_hidden(int k);
};

struct Genome {
    float core_radius = 1.0f;
    float shape[8]    = {};      // lumps / elongation, consumed by the shader
    float pigment     = 0.3f;    // 0 = pale, 1 = dark
    float hue         = 0.09f;   // skin colour on the wheel, 0..1 and round again (0 red, 1/3 green, 2/3 blue); colour.hpp
    float thermal_optimum   = 20.0f;  // °C
    float thermal_tolerance = 10.0f;  // ± °C of comfortable range
    float repro_threshold   = 10.0f;  // energy needed before dividing
    float offspring_share   = 0.5f;   // fraction of energy handed to the child
    float bite              = 0.0f;   // drains the living faster: pull is multiplied by 1 + bite
    float dormancy          = 0.0f;   // 0..1: how deeply it sleeps through dark and cold
    float seed_throw        = 0.0f;   // how far it can fling a child, in cells; 0 = the child lands touching it
    // Body outline (2026-11-11). A LUCA is round: both 0.
    float elongation        = 0.0f;   // 0 round .. 1 three times as long as it is wide
    float squareness        = 0.0f;   // 0 rounded .. 1 a box with rounded corners
    // Copying and upkeep of the body itself (2026-11-12).
    float birth_size        = 0.5f;   // fraction of adult mass a child is born at; it grows the rest (2026-11-12)
    std::vector<Part>  parts;
    std::vector<Enzyme> enzymes;   // its chemistry (see Enzyme)
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
constexpr float MAX_WEIGHT = 3.0f;    // also the most a neuron's bias can be
constexpr float MIN_TAU    = 1.0f,  MAX_TAU = 1000.0f;   // a neuron's time constant, ticks (a day is 600)
constexpr float MAX_THROW  = 6.0f;   // cells
constexpr float MAX_BITE   = 3.0f;   // at most four times the plain pull
constexpr float MAX_STALK  = 1.0f;   // world units: a part can sit at most this far out
constexpr float MIN_BIRTH  = 0.1f,  MAX_BIRTH  = 1.0f;   // born at a tenth of adult mass, up to full size
constexpr int   MAX_ENZYMES = 8;
constexpr float MIN_ENZYME_RATE = 0.0005f, MAX_ENZYME_RATE = 0.2f;   // units of mass a tick
}  // namespace limits

// Pull every gene back inside its range. Called after mutation and on import.
void clamp_genome(Genome& g);

// The last universal common ancestor: one core blob, one photosynthetic
// patch, no movement, no brain. Every lineage descends from this.
Genome make_luca();

// Sum of sizes of all parts of one type (handy for phenotype + metrics).
float total_part_size(const Genome& g, PartType t);

}  // namespace luca
