#pragma once
// The simulation: owns the world and the creatures, advances one tick at a time.

#include <cstdint>
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
    double in_vent         = 0.0;  // part of energy_in: taken up from vents (chemosynthesis)
    double grown_mass      = 0.0;  // matter young bodies took from the soil to grow (not a flow of energy)
    std::size_t windfalls  = 0;    // lumps dropped this tick
    double energy_out      = 0.0;  // everything lost as heat
    double out_upkeep      = 0.0;  // breakdown of energy_out
    double out_thermal     = 0.0;
    double out_movement    = 0.0;
    double out_transfer    = 0.0;  // lost crossing membranes (1 - TRANSFER_EFFICIENCY)
    double out_decay       = 0.0;  // corpses weathering away uneaten
    double out_waste       = 0.0;  // taken in by a body already full
    double out_poison      = 0.0;  // burned by eaters dealing with a poisonous meal
    double out_fouling     = 0.0;  // lost to living in waste
    double out_waste_decay = 0.0;  // waste breaking down by itself
    // Energy that changed hands inside the world (not part of in/out):
    double eaten_living    = 0.0;  // arrived from living bodies
    double eaten_dead      = 0.0;  // arrived from corpses
    double excreted        = 0.0;  // upkeep given off as waste instead of heat
    double eaten_waste     = 0.0;  // arrived from waste
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
    std::size_t cleaners = 0;     // mostly taken from waste
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
    void flush_powers() { powers_.update(tick_, world_, chronicle_); }

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
    // All matter in the world: soil, corpses and living bodies. Constant.
    double total_matter() const;

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
    // Add a creature and keep the species books; all arrivals go through here.
    // body_temp < -273 means "as warm as the ground it lands on" (newcomers).
    void admit(float x, float y, const Mix& store, const Mix& body, uint32_t lineage, uint32_t species, uint32_t generation,
               Genome genome, const Phenotype& pheno, float body_temp = -1000.0f);
    void weather_corpses();
    void wash_soil();
    void spread_waste();
    void drop_windfalls();
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
    Rng         rng_;
    SpatialGrid grid_;
    uint64_t    tick_ = 0;
    uint32_t    next_lineage_ = 0;
    uint64_t    next_id_ = 1;
    Soil        soil_;
    Census      census_;
    TickStats   stats_;
    PowerSystem powers_;
    WorldEvents events_;
    Chronicle   chronicle_;
    SpeciesRegistry species_;

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
    std::vector<float> wash_;                // per cell: matter on its way downhill
    std::vector<float> cell_corpse_demand_;  // per cell
    std::vector<float> corpse_want_, corpse_gain_;
    std::vector<float> cell_waste_demand_;   // per cell
    std::vector<float> waste_want_, waste_gain_;
    std::vector<float> cell_vent_demand_;    // per cell
    std::vector<float> vent_want_, vent_gain_;
    std::vector<float> waste_flow_;          // per cell: waste on its way to the neighbours
    std::vector<float> drain_want_, drained_, poisoned_;   // drain in mass from the store; poison in energy
    std::vector<Mix> intake_;                               // chemicals taken in from living bodies this tick
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
