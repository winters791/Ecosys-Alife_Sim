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
    out_transfer += s.out_transfer;
    out_decay    += s.out_decay;
    out_waste    += s.out_waste;
    out_poison   += s.out_poison;
    out_fouling  += s.out_fouling;
    out_waste_decay += s.out_waste_decay;
    excreted     += s.excreted;
    eaten_waste  += s.eaten_waste;
    eaten_living += s.eaten_living;
    eaten_dead   += s.eaten_dead;
    births       += s.births;
    gene_transfers       += s.gene_transfers;
    gene_transfers_cross += s.gene_transfers_cross;
    gene_transfers_novel += s.gene_transfers_novel;
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
            "ms_per_tick,"
            "out_transfer,out_decay,out_waste,eaten_living,eaten_dead,"
            "producers,hunters,scavengers,mixed,movers,sighted,armoured,"
            "mean_speed,mean_absorber,mean_armour,soil_nutrient,corpse_matter,matter_total,daylight,species,species_established,mean_throw,mean_bite,mean_digestion,brains,mean_neurons,mean_links,max_neurons,mean_dormancy,asleep,poisonous,out_poison,gene_transfers,gene_transfers_cross,gene_transfers_novel,"
            "waste_total,excreted,eaten_waste,out_fouling,out_waste_decay,cleaners,mean_detox\n";
    return true;
}

double MetricsWriter::write_row(const Simulation& sim, const IntervalTotals& t) {
    const Creatures& c = sim.creatures();
    const std::size_t n = c.size();
    const float half = sim.world().extent() * 0.5f;

    double sum_e = 0, sum_r = 0, sum_photo = 0, sum_parts = 0;
    double photo_b = 0, photo_d = 0, sum_speed = 0, sum_absorb = 0, sum_armour = 0, sum_throw = 0, sum_bite = 0, sum_digest = 0, sum_neurons = 0, sum_links = 0, sum_detox = 0;
    std::size_t brains = 0;
    int max_neurons = 0;
    double sum_dormancy = 0;
    std::size_t asleep = 0;
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
        sum_speed += c.pheno[i].max_speed;
        sum_throw += c.pheno[i].throw_range;
        sum_bite += c.genome[i].bite;
        sum_digest += c.genome[i].digestion;
        sum_detox += c.genome[i].detox;
        sum_dormancy += c.genome[i].dormancy;
        asleep += c.sleep[i] >= DEEP_SLEEP ? 1 : 0;
        if (c.pheno[i].has_brain) {   // brains that are actually running
            ++brains;
            sum_neurons += c.brain[i].hidden;
            sum_links += c.brain[i].links;
            max_neurons = std::max(max_neurons, static_cast<int>(c.brain[i].hidden));
        }
        sum_absorb += c.pheno[i].part_total[static_cast<int>(PartType::Absorber)];
        sum_armour += c.pheno[i].part_total[static_cast<int>(PartType::Armour)];
        max_gen = std::max(max_gen, c.generation[i]);
        lineages.insert(c.lineage[i]);
    }
    const double inv = n ? 1.0 / static_cast<double>(n) : 0.0;

    const Census& k = sim.census();
    double nutrient = 0, corpse = 0;
    for (float v : sim.soil().nutrient) nutrient += v;
    for (float v : sim.soil().corpse_matter) corpse += v;
    double waste = 0;
    for (float v : sim.soil().waste) waste += v;

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
         << (t.ticks ? t.total_ms / static_cast<double>(t.ticks) : 0.0) << ','
         << t.out_transfer << ',' << t.out_decay << ',' << t.out_waste << ','
         << t.eaten_living << ',' << t.eaten_dead << ','
         << k.producers << ',' << k.hunters << ',' << k.scavengers << ',' << k.mixed << ','
         << k.movers << ',' << k.sighted << ',' << k.armoured << ','
         << sum_speed * inv << ',' << sum_absorb * inv << ',' << sum_armour * inv << ','
         << nutrient << ',' << corpse << ',' << sim.total_matter() << ',' << sim.world().daylight() << ','
         << sim.species().living() << ',' << sim.species().living_established() << ',' << sum_throw * inv << ','
         << sum_bite * inv << ',' << sum_digest * inv << ','
         << brains << ',' << (brains ? sum_neurons / static_cast<double>(brains) : 0.0) << ','
         << (brains ? sum_links / static_cast<double>(brains) : 0.0) << ',' << max_neurons << ','
         << sum_dormancy * inv << ',' << asleep << ',' << k.poisonous << ',' << t.out_poison << ','
         << t.gene_transfers << ',' << t.gene_transfers_cross << ',' << t.gene_transfers_novel << ','
         << waste << ',' << t.excreted << ',' << t.eaten_waste << ',' << t.out_fouling << ',' << t.out_waste_decay << ','
         << k.cleaners << ',' << sum_detox * inv << '\n';
    out_.flush();
    return audit;
}

bool write_creature_snapshot(const Simulation& sim, const std::string& path) {
    std::ofstream f(path);
    if (!f) return false;
    f << "x,y,energy,age,lineage,generation,core_radius,photo,parts,pigment,"
         "absorber,flagellum,legs,sensor,armour,storage,poison,max_speed,fed_light,fed_living,fed_dead,species,seed_throw,bite,digestion,neurons,links,dormancy,detox,fed_waste\n";
    const Creatures& c = sim.creatures();
    for (std::size_t i = 0; i < c.size(); ++i) {
        const Genome& g = c.genome[i];
        f << c.x[i] << ',' << c.y[i] << ',' << c.energy[i] << ',' << c.age[i] << ','
          << c.lineage[i] << ',' << c.generation[i] << ',' << g.core_radius << ','
          << total_part_size(g, PartType::Photo) << ',' << g.parts.size() << ','
          << g.pigment;
        for (int t = 1; t < static_cast<int>(PartType::Count); ++t) f << ',' << c.pheno[i].part_total[t];
        f << ',' << c.pheno[i].max_speed << ',' << c.fed_light[i] << ',' << c.fed_living[i] << ',' << c.fed_dead[i] << ',' << c.species[i] << ',' << g.seed_throw << ',' << g.bite << ',' << g.digestion << ','
          << static_cast<int>(g.brain.hidden) << ',' << g.brain.links.size() << ',' << g.dormancy << ',' << g.detox << ',' << c.fed_waste[i] << '\n';
    }
    return true;
}

bool write_species_table(const Simulation& sim, const std::string& path) {
    std::ofstream f(path);
    if (!f) return false;
    f << "id,name,parent,born_tick,extinct_tick,population,peak,established,"
         "core_radius,photo,absorber,flagellum,legs,sensor,armour,storage,poison,brain\n";
    for (const Species& s : sim.species().all()) {
        f << s.id << ',' << s.name << ',';
        if (s.parent != NO_SPECIES) f << s.parent;
        f << ',' << s.born_tick << ',';
        if (!s.alive()) f << s.extinct_tick;
        f << ',' << s.population << ',' << s.peak << ',' << (s.established ? 1 : 0) << ','
          << s.representative.core_radius;
        for (int t = 0; t < static_cast<int>(PartType::Count); ++t)
            f << ',' << total_part_size(s.representative, static_cast<PartType>(t));
        f << ',' << (s.representative.brain.empty() ? 0 : 1) << '\n';
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
