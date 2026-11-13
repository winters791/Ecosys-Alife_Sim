#pragma once
// The simulation: owns the world and the creatures, advances one tick at a time.

#include <cstdint>
#include <map>
#include <unordered_map>
#include <vector>

#include "config.hpp"
#include "chronicle.hpp"
#include "creatures.hpp"
#include "powers.hpp"
#include "rng.hpp"
#include "scenario.hpp"
#include "soil.hpp"
#include "species.hpp"
#include "events.hpp"
#include "foodweb.hpp"
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
    double out_transfer    = 0.0;  // lost at the mouth as heat (0 unless SPILL_LOST is 0: by default the loss is spilled, not burned)
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
    // Chemistry stage 4 (observation): units turned over by two-input enzymes.
    double two_flow        = 0.0;
    // Stoichiometry (docs/design/stoichiometry.md): flows of the mineral P (units).
    double p_in            = 0.0;  // brought up by vents or landed by meteors
    double p_taken_up      = 0.0;  // from the soil through the skin
    double p_given_off     = 0.0;  // meal P an eater could not hold, back to the soil
    double p_corpse_eaten  = 0.0;  // freed from corpses by eating them
    double p_corpse_weathered = 0.0;  // freed from corpses by weathering
    std::size_t p_limited      = 0;   // tries to grow or breed held back by P (with energy enough)
    std::size_t energy_limited = 0;   // tries held back by energy
    // Blueprints taken up through contact (see Simulation::step, phase 2a):
    std::size_t gene_transfers = 0;        // all of them
    std::size_t gene_transfers_cross = 0;  // between different species
    std::size_t gene_transfers_novel = 0;  // a kind of part the taker's genome lacked
    double tick_ms         = 0.0;  // wall-clock time of this tick

    // Probes (design doc 14.6.4; observation only, nothing reads them).
    // Headless --probe writes them to probe.csv.
    struct Probe {
        // Meals from the living, by what became of them (energy).
        double meal_living       = 0.0;  // everything that arrived (B, C, D; A goes to the soil and is worth 0)
        double kept_living       = 0.0;  // kept in the store
        double spill_share       = 0.0;  // spilled because an eater keeps only `efficiency`
        double spill_room        = 0.0;  // spilled for want of room in the store
        double spill_unburnable  = 0.0;  // chemicals the eater can't burn
        double meal_by_full      = 0.0;  // part of meal_living taken by bodies whose store was at least 95% full
        double meal_dead         = 0.0;  // the same from corpses: arrived
        double kept_dead         = 0.0;  //   and kept
        // Deaths, by how they came about.
        double deaths            = 0.0;
        double deaths_drained    = 0.0;  // something was draining it when it died
        double deaths_could_pay  = 0.0;  // its store held enough, but its enzymes could not release it in time
        double deaths_poison     = 0.0;  // poison from a meal was its largest cost
        double store_at_death    = 0.0;  // sum over deaths of its store's worth / its capacity, before it paid
        // 2.8 causes 2 and 3: standing gear, and time between meals.
        double mouth_upkeep      = 0.0;  // upkeep paid by bodies with a mouth
        double mouth_gear        = 0.0;  //   the part of it that is standing gear (Phenotype::gear_upkeep)
        double mover_upkeep      = 0.0;  // the same for bodies that can move
        double mover_gear        = 0.0;
        double meal_gaps         = 0.0;  // meals from the living that came after a gap (by bodies that had eaten before)
        double meal_gap_ticks    = 0.0;  //   the gaps, summed
        double mouth_deaths_fed  = 0.0;  // deaths of bodies with a mouth that had once eaten from the living
        double fast_at_death     = 0.0;  //   ticks since their last such meal, summed
        void add(const Probe& o) {
            meal_living += o.meal_living; kept_living += o.kept_living; spill_share += o.spill_share;
            spill_room += o.spill_room; spill_unburnable += o.spill_unburnable; meal_by_full += o.meal_by_full;
            meal_dead += o.meal_dead; kept_dead += o.kept_dead;
            deaths += o.deaths; deaths_drained += o.deaths_drained; deaths_could_pay += o.deaths_could_pay;
            deaths_poison += o.deaths_poison; store_at_death += o.store_at_death;
            mouth_upkeep += o.mouth_upkeep; mouth_gear += o.mouth_gear; mover_upkeep += o.mover_upkeep; mover_gear += o.mover_gear;
            meal_gaps += o.meal_gaps; meal_gap_ticks += o.meal_gap_ticks;
            mouth_deaths_fed += o.mouth_deaths_fed; fast_at_death += o.fast_at_death;
        }
    } probe;
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

    // Development tools (design doc 14.6.3); never player powers.
    // Fork: give the world new dice (its own and the events'), so one saved
    // world has as many different futures as there are fork seeds. The same
    // fork seed always gives the same future. Call between ticks.
    void fork(uint64_t fork_seed);
    // Knockout: every living member of a species dies now, between ticks, and
    // leaves its corpse where it stood, so both books still close. Returns how
    // many died. For "what if they vanished?" (2.9) on a copy of a world.
    std::size_t knockout(uint32_t species);

    // A scenario (scenario.hpp): a goal with a clock, checked with the census
    // every 50 ticks; the chronicle says when it starts and when it is decided.
    // Observation only. Starting one replaces any scenario already running.
    void start_scenario(uint32_t id);
    const ScenarioState& scenario() const { return scenario_; }

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
    // All matter in the world: soil, corpses, loose chemicals and living
    // bodies. It changes only by what vents bring up from below, so
    // total_matter() - matter_brought_up() is constant.
    double total_matter() const;
    // The P book: total_p() - p_brought_up() is constant.
    double total_p() const;
    double p_brought_up() const { return p_brought_up_; }
    // Lays the soil's P out afresh from the world's seed (P_MEAN). Used at
    // the start, and when a world saved without P is loaded.
    void lay_out_mineral();
    double matter_brought_up() const { return matter_brought_up_; }
    // The leavings ledger (observation only): energy taken up through the skin
    // that another body gave off, by (taker species << 32 | giver species),
    // as the leavings tag tells it. Since the start of this run (not saved).
    const std::unordered_map<uint64_t, double>& leavings_eaten() const { return leavings_eaten_; }
    // The food web's ledger (foodweb.hpp; observation only, fades, not saved).
    const FoodLedger& food_ledger() const { return food_; }
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
               Genome genome, const Phenotype& pheno, float body_temp = -1000.0f, float body_p = -1.0f, float store_p = 0.0f);
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
    void share_light_by_footprint(bool dark);
    // Where a child flung `dist` from (x, y) along (ux, uy) comes down: it
    // stops short of any step in the ground taller than birth_max_step.
    void land_child(float x, float y, float ux, float uy, float dist, float& out_x, float& out_y) const;
    void decide_movement(std::size_t i, int cell, const LocalEnv& env);
    void decide_sensor_brain(std::size_t i, int cell, const LocalEnv& env);   // SENSOR_BRAIN (14.9)
    float sample_channel(int channel, float x, float y, const Phenotype& p, float body_temp) const;

    struct Birth {
        float x, y;
        Mix store, body;                // its share of the parent's store, and its body, built from it
        float body_temp;                // a child starts as warm as its parent
        uint32_t lineage, species, generation;
        Genome genome;
        Phenotype pheno;
        float body_p = 0.0f, store_p = 0.0f;   // its P: body and store
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
    double p_brought_up_ = 0.0;        // all the P vents and meteors have brought from outside
    double arrived_p_ = 0.0;           // landed between ticks, booked in the next
    double matter_brought_up_ = 0.0;   // all the matter vents and meteors have brought from outside since tick 0
    double arrived_energy_ = 0.0;      // meteors landed between ticks (flush_powers): booked into the next tick
    double arrived_matter_ = 0.0;
    Chronicle   chronicle_;
    SpeciesRegistry species_;
    ScenarioState   scenario_;

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
    std::vector<float> arc_;   // how far a body turned this tick, as distance along its rim (SENSOR_BRAIN)
    std::vector<float> biomass_;             // per cell
    std::vector<float> wash_;                // per cell: matter on its way downhill
    std::vector<float> cell_corpse_demand_;  // per cell
    std::vector<float> corpse_want_, corpse_gain_;
    std::vector<float> cell_uptake_demand_;  // per cell and chemical (cell * NCHEM + c): shares of the stock asked for
    std::vector<float> uptake_want_;         // share of its cell's stock of each chemical it can burn
    std::vector<Mix>   uptake_gain_;         // what it takes up this tick
    std::vector<Mix>   spread_;              // per cell: loose chemicals on their way to the neighbours
    std::vector<uint64_t> loose_was_;        // spread_dissolved: which cells were giving
    struct LightAsk { float canopy; uint32_t who; float want; };
    std::vector<LightAsk> light_asks_;       // LIGHT_FOOTPRINT: one per body per cell it covers
    std::vector<int>      light_ask_cell_;   // the cell of each ask
    SpatialGrid           light_grid_;       // the asks by cell
    std::vector<LightAsk> light_cell_;       // one crowded cell's asks, tallest first
    static constexpr uint32_t NO_LEAVER = 0xFFFFFFFFu;
    std::vector<uint32_t> leaver_;           // per cell: the species its loose chemicals are tagged with
    std::vector<float>    left_;             // per cell: how much of their worth that tag covers
    struct TagIn { uint32_t species = NO_LEAVER; float amount = 0.0f; };
    std::vector<TagIn>    tag_in_;           // spread_dissolved: tags on their way to the neighbours
    std::unordered_map<uint64_t, double> leavings_eaten_;
    FoodLedger food_;
    void tag_leavings(int cell, uint32_t species, float energy);
    struct CorpseAsk { uint32_t taker; int cell; float want, gain; };
    std::vector<CorpseAsk> corpse_asks_;     // DEAD_IN_REACH: pulls on the dead of neighbouring cells, in body order
    std::vector<int>   scratch_cells_;       // cells whose per-cell sums (biomass_, demands) last tick wrote
    std::vector<float> spread_vent_;         // per cell: the vent's share of them (observation only)
    std::vector<float> drain_want_, drained_, poisoned_;   // drain in mass from the store; poison in energy
    std::vector<float> p_want_, p_gain_, intake_p_;         // stoichiometry: P asked for and got through the skin; P in meals from the living
    std::vector<float> cell_p_demand_;                      // per cell: shares of its soil P asked for
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
