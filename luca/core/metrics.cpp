#include "metrics.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace luca {

void IntervalTotals::begin(const Simulation& sim) {
    *this = IntervalTotals{};
    stored_start = sim.stored_energy();
}

void IntervalTotals::add(const TickStats& s) {
    energy_in    += s.energy_in;
    energy_out   += s.energy_out;
    out_upkeep   += s.out_upkeep;
    out_thermal  += s.out_thermal;
    out_movement += s.out_movement;
    births       += s.births;
    deaths       += s.deaths;
    total_ms     += s.tick_ms;
    ticks        += 1;
}

bool MetricsWriter::open(const std::string& path) {
    out_.open(path);
    if (!out_) return false;
    out_ << "tick,population,births,deaths,"
            "mean_energy,mean_radius,mean_photo,photo_bright,photo_dim,mean_parts,"
            "max_generation,lineages_alive,"
            "energy_in,energy_out,out_upkeep,out_thermal,out_movement,stored,audit_rel_error,"
            "ms_per_tick\n";
    return true;
}

double MetricsWriter::write_row(const Simulation& sim, const IntervalTotals& t) {
    const Creatures& c = sim.creatures();
    const std::size_t n = c.size();
    const float half = sim.world().extent() * 0.5f;

    double sum_e = 0, sum_r = 0, sum_photo = 0, sum_parts = 0;
    double photo_b = 0, photo_d = 0;
    std::size_t nb = 0, nd = 0;
    uint32_t max_gen = 0;
    std::unordered_set<uint32_t> lineages;
    for (std::size_t i = 0; i < n; ++i) {
        const float photo = total_part_size(c.genome[i], PartType::Photo);
        sum_e += c.energy[i];
        sum_r += c.genome[i].core_radius;
        sum_photo += photo;
        sum_parts += static_cast<double>(c.genome[i].parts.size());
        // "Bright" = the sunny half of the world (y < half) in gradient mode.
        if (c.y[i] < half) { photo_b += photo; ++nb; }
        else               { photo_d += photo; ++nd; }
        max_gen = std::max(max_gen, c.generation[i]);
        lineages.insert(c.lineage[i]);
    }
    const double inv = n ? 1.0 / static_cast<double>(n) : 0.0;

    const double stored = sim.stored_energy();
    const double expected = t.stored_start + t.energy_in - t.energy_out;
    const double audit = (stored - expected) / std::max(1.0, std::abs(stored));

    out_ << sim.tick() << ',' << n << ',' << t.births << ',' << t.deaths << ','
         << sum_e * inv << ',' << sum_r * inv << ',' << sum_photo * inv << ','
         << (nb ? photo_b / nb : 0.0) << ',' << (nd ? photo_d / nd : 0.0) << ','
         << sum_parts * inv << ','
         << max_gen << ',' << lineages.size() << ','
         << t.energy_in << ',' << t.energy_out << ','
         << t.out_upkeep << ',' << t.out_thermal << ',' << t.out_movement << ','
         << stored << ',' << audit << ','
         << (t.ticks ? t.total_ms / static_cast<double>(t.ticks) : 0.0) << '\n';
    out_.flush();
    return audit;
}

bool write_creature_snapshot(const Simulation& sim, const std::string& path) {
    std::ofstream f(path);
    if (!f) return false;
    f << "x,y,energy,age,lineage,generation,core_radius,photo,parts,pigment\n";
    const Creatures& c = sim.creatures();
    for (std::size_t i = 0; i < c.size(); ++i) {
        const Genome& g = c.genome[i];
        f << c.x[i] << ',' << c.y[i] << ',' << c.energy[i] << ',' << c.age[i] << ','
          << c.lineage[i] << ',' << c.generation[i] << ',' << g.core_radius << ','
          << total_part_size(g, PartType::Photo) << ',' << g.parts.size() << ','
          << g.pigment << '\n';
    }
    return true;
}

bool write_world_snapshot(const Simulation& sim, const std::string& path) {
    std::ofstream f(path);
    if (!f) return false;
    const World& w = sim.world();
    f << "# size=" << w.size() << " cell_size=" << w.cell_size()
      << " rows=y cols=x; fields: height\n";
    const auto& h = w.heights();
    for (int y = 0; y < w.size(); ++y) {
        for (int x = 0; x < w.size(); ++x) {
            if (x) f << ',';
            f << h[y * w.size() + x];
        }
        f << '\n';
    }
    return true;
}

}  // namespace luca
