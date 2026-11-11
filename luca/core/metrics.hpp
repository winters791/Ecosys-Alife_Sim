#pragma once
// Metrics: one CSV row every N ticks, plus an optional end-of-run snapshot
// (every creature + the heightmap) for plotting a map.

#include <cstdint>
#include <fstream>
#include <string>

#include "sim.hpp"

namespace luca {

// Sums TickStats over one metrics interval and checks energy conservation:
//   stored(end) == stored(start) + energy_in - energy_out
class IntervalTotals {
public:
    void begin(const Simulation& sim);
    void add(const TickStats& s);

    double stored_start = 0.0;
    double energy_in = 0.0, energy_out = 0.0;
    double out_upkeep = 0.0, out_thermal = 0.0, out_movement = 0.0;
    double out_transfer = 0.0, out_decay = 0.0, out_waste = 0.0, out_poison = 0.0;
    double eaten_living = 0.0, eaten_dead = 0.0;
    double out_fouling = 0.0, out_waste_decay = 0.0, excreted = 0.0, eaten_waste = 0.0;
    double in_windfall = 0.0;
    std::size_t windfalls = 0;
    std::size_t births = 0, deaths = 0;
    std::size_t gene_transfers = 0, gene_transfers_cross = 0, gene_transfers_novel = 0;
    double total_ms = 0.0;
    uint64_t ticks = 0;
};

class MetricsWriter {
public:
    bool open(const std::string& path);
    // Writes one row; returns the audit's relative error for this interval.
    double write_row(const Simulation& sim, const IntervalTotals& t);

private:
    std::ofstream out_;
};

// End-of-run snapshot for map plots.
bool write_creature_snapshot(const Simulation& sim, const std::string& path);
bool write_world_snapshot(const Simulation& sim, const std::string& path);

// Every species there has ever been: the tree of life as a table (parent ids).
bool write_species_table(const Simulation& sim, const std::string& path);

}  // namespace luca
