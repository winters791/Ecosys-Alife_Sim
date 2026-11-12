#pragma once
// The creature designer's helpers (viewer: designer.gd). Nothing here changes
// a rule: every number comes from the real phenotype compiler and energy
// rules, so a design is judged exactly as the world will judge it.
//
//   assess_design   what a body costs to keep, part by part, and what the sun
//                   would give it: the energy budget the designer shows
//   design_templates  hand-made starting points (also the viewer's --parade)
//   probe_brain     what a brain answers to a set of senses held steady
//   bank_species    a sample of a living species, for the seed vault (4.9)
//   Trial           a small walled-off world to test a design in

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "creature_file.hpp"
#include "phenotype.hpp"

namespace luca {

class Simulation;

// One line of the upkeep bill: what the body would save a tick without this
// (its mass included, so a heavy part's share of Kleiber upkeep is its own).
// "body" is everything the other lines don't explain: the core blob itself.
struct CostLine {
    std::string kind;   // body, part, enzyme, brain, tolerance, bite, dormancy, throw, repair, fidelity, shape
    int index = -1;     // which part or enzyme, else -1
    float upkeep = 0.0f;
};

struct DesignReport {
    Phenotype p;
    float upkeep = 0.0f;        // energy a tick at birth (what the world charges, not the raw gene sum)
    float upkeep_old = 0.0f;    // the same at age 10,000 ticks: wear, slowed by repair
    std::vector<CostLine> costs;   // biggest first
    float light_income = 0.0f;  // mean energy a tick from the sun, sitting still alone in a cell
    float thermal = 0.0f;       // mean energy a tick lost to living outside its comfort range
    float burn_power = 0.0f;    // the most its enzymes can release a tick (it dies if costs outrun this)
    float charge_power = 0.0f;  // the most light its enzymes can store a tick (from soil A)
    float body_energy = 0.0f;   // what its grown body is worth
    float child_cost = 0.0f;    // what one child's body costs its parent (at birth size)
    bool mover = false;         // it has a motor and a brain: it will move, folding its sun patches
    std::vector<std::string> warnings;
};

// `sun`: mean light a tick in the cell (1 = the brightest edge of a default
// world, averaged over a day); `air`: the daily mean air temperature, °C.
DesignReport assess_design(const Genome& g, float sun, float air);

std::vector<std::pair<std::string, Genome>> design_templates();

// Run a brain `ticks` ticks from rest with the inputs held steady (NUM_INPUTS
// values). Writes NUM_OUTPUTS outputs and MAX_HIDDEN hidden firings.
void probe_brain(const Genome& g, const float* inputs, int ticks, float* outputs, float* hidden);

// Up to `max` living members of a species, evenly spread over the creature
// arrays, or its founder's blueprint once it is extinct.
CreatureFile bank_species(const Simulation& sim, uint32_t species, int max = VAULT_SAMPLE);

// A test tank: a flat little world of uniform light and temperature, with or
// without wild LUCAs, where `count` bodies of a creature file land by meteor
// at the centre. Deterministic for a given seed.
struct TrialOptions {
    int count = 12;
    bool with_luca = true;      // wild LUCAs to compete with, or to eat
    float sun = 1.0f;
    float temperature = 20.0f;
    int size = 32;              // cells per side
    uint64_t seed = 1;
};

class Trial {
public:
    Trial(const CreatureFile& cargo, const TrialOptions& o);
    ~Trial();
    void step(int ticks);
    const Simulation& sim() const { return *sim_; }
    // Is creature i one of the design's lineage (the ones that landed and
    // their descendants)?
    bool guest(std::size_t i) const;
    int guests() const;
    int natives() const;
    uint32_t generations() const;   // the most divisions any living guest is from its lander
    uint64_t guest_births() const { return guest_births_; }

private:
    std::unique_ptr<Simulation> sim_;
    uint32_t lineage_ = 0;
    uint64_t guest_births_ = 0;
};

}  // namespace luca
