#pragma once
// The simulation: owns the world and the creatures, advances one tick at a time.

#include <cstdint>
#include <map>
#include <vector>

#include "config.hpp"
#include "chronicle.hpp"
#include "creatures.hpp"
#include "powers.hpp"
#include "rng.hpp"
#include "soil.hpp"
#include "species.hpp"
#include "events.hpp"
#include "spatial_grid.hpp"
#include "world.hpp"

namespace luca {

// What happened during one tick. Energy totals are exact (double) so the
// audit can check conservation.
struct TickStats {
    uint64_t    tick = 0;
    std::size_t births = 0;
    std::size_t deaths = 0;
    double energy_in       = 0.0;  // everything that enters the world: light absorbed plus windfalls
    double in_windfall     = 0.0;  // part of energy_in: food dropped on the map (see drop_windfalls)
    double in_vent         = 0.0;  // part of energy_in: made by vents (VENT_CHEM out of the cell's A)
    double in_meteor       = 0.0;  // part of energy_in: bodies (and their stores) landed by the seed power
    double matter_in       = 0.0;  // matter arriving from outside: brought up by vents, or landed by meteors (units)
    double grown_mass      = 0.0;  // matter young bodies took from the soil to grow (not a flow of energy)
    std::size_t windfalls  = 0;    // lumps dropped this tick
    double energy_out      = 0.0;  // everything lost as heat
    double out_upkeep      = 0.0;  // breakdown of energy_out
    double out_thermal     = 0.0;
    double out_movement    = 0.0;
    double out_transfer    = 0.0;  // lost crossing membranes (0 since chemistry stage 2: the loss is spilled, not burned)
    double out_decay       = 0.0;  // corpses weathering away uneaten
    double out_waste       = 0.0;  // taken in by a body already full (0 since chemistry stage 2: spilled instead)
    double out_poison      = 0.0;  // burned by eaters dealing with a poisonous meal
    double out_fouling     = 0.0;  // lost to living among chemicals it can't burn
    double out_waste_decay = 0.0;  // loose chemicals breaking down to A by themselves
    // Energy that changed hands inside the world (not part of in/out):
    double eaten_living    = 0.0;  // arrived from living bodies
    double eaten_dead      = 0.0;  // arrived from corpses
    double excreted        = 0.0;  // given off into the cell: meals spilled, chemicals it can't burn
    double eaten_waste     = 0.0;  // taken up from the cell's loose chemicals
    // Chemistry stage 4 (observation): units turned over by two-input enzymes,
    // and the part of it done by neutral exchanges (energy.hpp, convert).
    double two_flow        = 0.0;
    double neutral_flow    = 0.0;
    // Blueprints taken up through contact (see Simulation::step, phase 2a):
    std::size_t gene_transfers = 0;        // all of them
    std::size_t gene_transfers_cross = 0;  // between different species
    std::size_t gene_transfers_novel = 0;  // a kind of part the taker's genome lacked
    double tick_ms         = 0.0;  // wall-clock time of this tick
};

// Who lives how, counted from where each creature's energy has actually come
// from. Nothing in the sim is labelled "predator": these are observations.
struct Census {
    std::size_t producers = 0;    // mostly light
    std::size_t hunters = 0;      // mostly taken from living bodies
    std::size_t scavengers = 0;   // mostly taken from corpses
    std::size_t cleaners = 0;     // mostly taken up from loose chemicals in the cell
    std::size_t chemotrophs = 0;  // mostly taken from vents
    std::size_t mixed = 0;        // no single source above 60%
    std::size_t young = 0;        // hasn't eaten enough yet to say
    std::size_t growing = 0;      // not yet full size (any way of life); can't breed yet
    std::size_t movers = 0;       // can move
    std::size_t sighted = 0;      // has a sensor
    std::size_t armoured = 0;
    std::size_t poisonous = 0;
};

class Simulation {
    friend struct SnapshotIO;   // snapshot.cpp: saves and restores the whole state
public:
    explicit Simulation(const SimConfig& cfg);

    // Scatter the initial LUCAs uniformly over the world.
    void seed_population();

    // Drop one creature into the world from outside (a test, and later a
    // creature file or a life-bearing meteor). Only the genome comes in; its
    // stats are compiled here, under this world's physics. Its body is new
    // matter and energy arriving from outside, so do this between audits.
    // Returns the creature's id.
    uint64_t add_creature(float x, float y, float energy, const Genome& genome, float grown = 1.0f);
    uint64_t next_id() const { return next_id_; }   // the id the next creature will get (it decides its birth facing)

    // Advance one tick.
    void step();

    // Cast a god power. It takes effect at the start of tick `cmd.tick` (or
    // of the next tick, if that is already past). Powers change the world,
    // never creatures.
    void cast(const PowerCommand& cmd) { powers_.cast(cmd); }

    // Apply powers that are due right now instead of waiting for the next
    // step(), so painting works while time is paused. The result is identical
    // to letting step() do it: the next step() finds nothing left to add.
    // A meteor (the seed power) lands at once too; its energy and matter are
    // booked into the next tick's stats, so the audit still balances.
    void flush_powers() {
        powers_.update(tick_, world_, chronicle_);
        land_arrivals(true);
    }

    // The world acting on its own (events.hpp). The rate can change at any
    // time; what was cast stays in the power log either way.
    void  set_event_rate(float rate) { events_.set_rate(rate); }
    float event_rate() const { return events_.rate(); }
    const WorldEvents& events() const { return events_; }
    // Start one event of a kind now, whatever the rate (tests, scripts).
    void trigger_event(EventKind kind) { events_.trigger(kind, tick_, world_, powers_, chronicle_); }

    const SimConfig& config() const { return cfg_; }
    const World&     world() const { return world_; }
    const Creatures& creatures() const { return c_; }
    uint64_t         tick() const { return tick_; }
    const TickStats& last_stats() const { return stats_; }
    const Chronicle& chronicle() const { return chronicle_; }
    const PowerSystem& powers() const { return powers_; }
    const Soil&      soil() const { return soil_; }
    const std::vector<float>& biomass() const { return biomass_; }  // living mass per cell, last tick
    // Camouflage (colour.hpp): the colour of a cell's ground, and how plainly
    // creature i shows against the ground it is on (VISIBLE_MIN..1; moving gives it away).
    Rgb   ground_colour_at(int cell) const;
    float shows(std::size_t i) const;
    const Census&    census() const { return census_; }             // refreshed every 50 ticks
    const SpeciesRegistry& species() const { return species_; }     // every species there has ever been

    // Following a species (design doc 4.8). Observation only: a followed
    // species gets no help. What happens to it is written to alerts() ("Your
    // Velox have lost their eyes."), not to the chronicle.
    void follow(uint32_t species, bool on = true);
    bool following(uint32_t species) const;
    std::vector<uint32_t> followed() const;
    const Chronicle& alerts() const { return alerts_; }

    // All energy held in the world: in living creatures, locked in their
    // bodies, and left in corpses.
    double stored_energy() const;
    // All matter in the world: soil, corpses, loose chemicals and living
    // bodies. It changes only by what vents bring up from below, so
    // total_matter() - matter_brought_up() is constant.
    double total_matter() const;
    double matter_brought_up() const { return matter_brought_up_; }
    // Living bodies whose genome holds a blueprint taken up by gene transfer (snapshot.hpp).
    std::size_t borrowed_count() const { return borrowed_.size(); }

private:
    // Population-level event detectors; they only write to the chronicle.
    void detect_events();

    void take_census();
    // Every SPLIT_INTERVAL ticks: split any big species that has become two
    // separate clusters of genomes (design doc 9.3 step 3). No randomness.
    void check_splits();
    // Every WATCH_INTERVAL ticks, for followed species only: what changed.
    void watch_followed();
    void alert(uint32_t species, const std::string& text);
    // What a species looks like right now, for following it.
    struct Survey {
        uint32_t count = 0;
        uint32_t traits = 0;   // bit per part type most of them carry, and bit 8: most can move
        float    mass = 0.0f;  // mean adult mass
    };
    Survey survey(uint32_t species, uint32_t old_traits) const;
    // Each species' habits, for the species card (observation only).
    void take_habits();
    // Add a creature and keep the species books; all arrivals go through here.
    // body_temp < -273 means "as warm as the ground it lands on" (newcomers).
    void admit(float x, float y, const Mix& store, const Mix& body, uint32_t lineage, uint32_t species, uint32_t generation,
               Genome genome, const Phenotype& pheno, float body_temp = -1000.0f);
    void weather_corpses();
    void wash_soil();
    void spread_dissolved();
    void drop_windfalls();
    // Vents turn their cell's A into VENT_CHEM, loose in the cell (energy.hpp).
    void run_vents();
    // Life-bearing meteors (the seed power, powers.hpp): land the bodies of
    // every one that has come due. Their energy and matter are new.
    // `between_ticks`: from flush_powers, so the books go to the next tick.
    void land_arrivals(bool between_ticks);
    void land_meteor(const PowerCommand& cmd, bool between_ticks);
    void share_light_by_canopy();
    // Where a child flung `dist` from (x, y) along (ux, uy) comes down: it
    // stops short of any step in the ground taller than birth_max_step.
    void land_child(float x, float y, float ux, float uy, float dist, float& out_x, float& out_y) const;
    void decide_movement(std::size_t i, int cell, const LocalEnv& env);

    struct Birth {
        float x, y;
        Mix store, body;                // its share of the parent's store, and its body, built from it
        float body_temp;                // a child starts as warm as its parent
        uint32_t lineage, species, generation;
        Genome genome;
        Phenotype pheno;
    };

    SimConfig   cfg_;
    World       world_;
    Creatures   c_;
    // Every random draw is keyed from this (rng.hpp, draw_key); there is no
    // running stream, so draws do not depend on the order bodies are visited.
    uint64_t    draw_seed_ = 0;
    SpatialGrid grid_;
    uint64_t    tick_ = 0;
    uint32_t    next_lineage_ = 0;
    uint64_t    next_id_ = 1;
    Soil        soil_;
    Census      census_;
    TickStats   stats_;
    PowerSystem powers_;
    WorldEvents events_;
    double matter_brought_up_ = 0.0;   // all the matter vents and meteors have brought from outside since tick 0
    double arrived_energy_ = 0.0;      // meteors landed between ticks (flush_powers): booked into the next tick
    double arrived_matter_ = 0.0;
    Chronicle   chronicle_;
    SpeciesRegistry species_;

    // Bodies whose genome has taken up a blueprint by gene transfer since they
    // were built, by creature id. Their stats were compiled from an earlier
    // genome (and their brain and body plans from the one they were born
    // with), so a snapshot needs both to rebuild them exactly. No rule reads it.
    struct Borrowed {
        Genome birth;   // the genome it was born with: its brain and body plans
        Genome built;   // the genome its stats were last compiled from (birth, or its genome at its last growth step)
    };
    std::map<uint64_t, Borrowed> borrowed_;

    struct Watch {
        uint32_t species = 0;
        Survey   seen;          // what it looked like when last reported on
    };
    std::vector<Watch> watch_;  // followed species
    Chronicle          alerts_;

    // Detector state
    std::size_t peak_pop_ = 0;        // highest population since the last die-off
    std::size_t crash_from_ = 0;      // the peak we fell from; 0 = not in a die-off
    uint64_t    crash_tick_ = 0;      // when that die-off was logged
    std::size_t next_milestone_ = 1000;
    std::size_t day_peak_ = 0;        // highest population so far today
    uint64_t    day_seen_ = 0;
    bool        extinct_logged_ = false;
    bool        movers_logged_ = false, hunters_logged_ = false, scavengers_logged_ = false, cleaners_logged_ = false, chemotrophs_logged_ = false;
    bool        sighted_logged_ = false, armoured_logged_ = false, poisonous_logged_ = false, transfer_logged_ = false;

    // Scratch buffers, reused every tick to avoid allocation.
    std::vector<int>   cell_of_;
    std::vector<float> demand_;
    std::vector<float> cell_demand_;
    std::vector<float> gain_;
    std::vector<float> cost_upkeep_, cost_thermal_, cost_move_;
    std::vector<float> new_x_, new_y_, new_hx_, new_hy_;
    std::vector<float> biomass_;             // per cell
    std::vector<float> seen_;                // per cell: living mass as others sense it, each body weighed by how it shows (colour.hpp)
    std::vector<float> shows_;               // per creature: how plainly it shows against its ground, VISIBLE_MIN..1
    std::vector<float> wash_;                // per cell: matter on its way downhill
    std::vector<float> cell_corpse_demand_;  // per cell
    std::vector<float> corpse_want_, corpse_gain_;
    std::vector<float> cell_uptake_demand_;  // per cell and chemical (cell * NCHEM + c): shares of the stock asked for
    std::vector<float> uptake_want_;         // share of its cell's stock of each chemical it can burn
    std::vector<Mix>   uptake_gain_;         // what it takes up this tick
    std::vector<Mix>   spread_;              // per cell: loose chemicals on their way to the neighbours
    std::vector<float> spread_vent_;         // per cell: the vent's share of them (observation only)
    std::vector<float> drain_want_, drained_, poisoned_;   // drain in mass from the store; poison in energy
    std::vector<Mix> intake_;                               // chemicals taken in from living bodies this tick
    // Who took most from each body this tick: the one credited if it dies
    // (the record book only; nothing in the sim reads it).
    std::vector<uint32_t> top_taker_;
    std::vector<float>    top_take_;
    struct Contact { uint32_t taker, giver; float flow; };
    std::vector<Contact> contacts_;
    struct Near { float x, y, reach; uint32_t who; };  // one creature, in cell order
    std::vector<Near> near_;
    std::vector<uint32_t> dead_;
    struct Tall { float radius; uint32_t who; };   // radius here is canopy height (Phenotype::canopy)
    std::vector<Tall> canopy_;               // one crowded cell's bodies, tallest first
    std::vector<Birth> births_;
};

}  // namespace luca
