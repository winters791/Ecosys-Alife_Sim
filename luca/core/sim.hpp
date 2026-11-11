#pragma once
// The simulation: owns the world and the creatures, advances one tick at a time.

#include <cstdint>
#include <vector>

#include "config.hpp"
#include "chronicle.hpp"
#include "creatures.hpp"
#include "powers.hpp"
#include "rng.hpp"
#include "spatial_grid.hpp"
#include "world.hpp"

namespace luca {

// What happened during one tick. Energy totals are exact (double) so the
// audit can check conservation.
struct TickStats {
    uint64_t    tick = 0;
    std::size_t births = 0;
    std::size_t deaths = 0;
    double energy_in       = 0.0;  // light absorbed
    double energy_out      = 0.0;  // everything paid (lost as heat)
    double out_upkeep      = 0.0;  // breakdown of energy_out
    double out_thermal     = 0.0;
    double out_movement    = 0.0;
    double tick_ms         = 0.0;  // wall-clock time of this tick
};

class Simulation {
public:
    explicit Simulation(const SimConfig& cfg);

    // Scatter the initial LUCAs uniformly over the world.
    void seed_population();

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

    const SimConfig& config() const { return cfg_; }
    const World&     world() const { return world_; }
    const Creatures& creatures() const { return c_; }
    uint64_t         tick() const { return tick_; }
    const TickStats& last_stats() const { return stats_; }
    const Chronicle& chronicle() const { return chronicle_; }
    const PowerSystem& powers() const { return powers_; }

    // Total energy currently stored in living creatures.
    double stored_energy() const;

private:
    // Population-level event detectors; they only write to the chronicle.
    void detect_events();

    struct Birth {
        float x, y, energy;
        uint32_t lineage, generation;
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
    TickStats   stats_;
    PowerSystem powers_;
    Chronicle   chronicle_;

    // Detector state
    std::size_t peak_pop_ = 0;        // highest population since the last die-off
    std::size_t crash_from_ = 0;      // the peak we fell from; 0 = not in a die-off
    uint64_t    crash_tick_ = 0;      // when that die-off was logged
    std::size_t next_milestone_ = 1000;
    bool        extinct_logged_ = false;

    // Scratch buffers, reused every tick to avoid allocation.
    std::vector<int>   cell_of_;
    std::vector<float> demand_;
    std::vector<float> cell_demand_;
    std::vector<float> gain_;
    std::vector<float> cost_upkeep_, cost_thermal_, cost_move_;
    std::vector<float> new_x_, new_y_;
    std::vector<uint32_t> dead_;
    std::vector<Birth> births_;
};

}  // namespace luca
