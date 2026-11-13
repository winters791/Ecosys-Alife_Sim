#include "metrics.hpp"

#include "body.hpp"

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
    dissolved_dead += s.dissolved_dead;
    moved_downhill += s.moved_downhill;
    in_windfall  += s.in_windfall;
    in_vent      += s.in_vent;
    in_meteor    += s.in_meteor;
    grown_mass   += s.grown_mass;
    two_flow     += s.two_flow;
    matter_in    += s.matter_in;
    p_in         += s.p_in;
    p_taken_up   += s.p_taken_up;
    p_given_off  += s.p_given_off;
    p_corpse_eaten += s.p_corpse_eaten;
    p_corpse_weathered += s.p_corpse_weathered;
    p_limited    += s.p_limited;
    energy_limited += s.energy_limited;
    windfalls    += s.windfalls;
    eaten_living += s.eaten_living;
    eaten_dead   += s.eaten_dead;
    births       += s.births;
    gene_transfers       += s.gene_transfers;
    gene_transfers_cross += s.gene_transfers_cross;
    gene_transfers_novel += s.gene_transfers_novel;
    deaths       += s.deaths;
    total_ms     += s.tick_ms;
    ticks        += 1;
    probe.add(s.probe);
}

bool ProbeWriter::open(const std::string& path) {
    out_.open(path);
    if (!out_) return false;
    out_ << "tick,population,meal_living,kept_living,spill_share,spill_room,spill_unburnable,meal_by_full,"
            "meal_dead,kept_dead,corpse_decay,deaths,deaths_drained,deaths_could_pay,deaths_poison,mean_store_at_death,"
            "mouth_upkeep,mouth_gear_share,mover_upkeep,mover_gear_share,meal_gaps,mean_meal_gap,mouth_deaths_fed,mean_fast_at_death\n";
    return true;
}

void ProbeWriter::write_row(const Simulation& sim, const IntervalTotals& t) {
    const TickStats::Probe& p = t.probe;
    out_ << sim.tick() << ',' << sim.creatures().size() << ',' << p.meal_living << ',' << p.kept_living << ','
         << p.spill_share << ',' << p.spill_room << ',' << p.spill_unburnable << ',' << p.meal_by_full << ','
         << p.meal_dead << ',' << p.kept_dead << ',' << t.out_decay << ',' << p.deaths << ',' << p.deaths_drained << ','
         << p.deaths_could_pay << ',' << p.deaths_poison << ',' << (p.deaths > 0 ? p.store_at_death / p.deaths : 0.0) << ','
         << p.mouth_upkeep << ',' << (p.mouth_upkeep > 0 ? p.mouth_gear / p.mouth_upkeep : 0.0) << ','
         << p.mover_upkeep << ',' << (p.mover_upkeep > 0 ? p.mover_gear / p.mover_upkeep : 0.0) << ','
         << p.meal_gaps << ',' << (p.meal_gaps > 0 ? p.meal_gap_ticks / p.meal_gaps : 0.0) << ','
         << p.mouth_deaths_fed << ',' << (p.mouth_deaths_fed > 0 ? p.fast_at_death / p.mouth_deaths_fed : 0.0) << '\n';
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
            "mean_speed,mean_absorber,mean_armour,soil_nutrient,corpse_matter,matter_total,daylight,species,species_established,mean_throw,mean_bite,brains,mean_neurons,mean_links,max_neurons,mean_dormancy,asleep,poisonous,out_poison,gene_transfers,gene_transfers_cross,gene_transfers_novel,"
            "waste_total,excreted,eaten_waste,out_fouling,out_waste_decay,cleaners,loose_B,loose_C,loose_D,"
            "in_windfall,windfalls,in_vent,in_meteor,chemotrophs,vent_loose,growing,mean_birth_size,grown_mass,"
            "neurons,slow_neurons,mean_tau,"
            "pool_B,pool_C,pool_D,tissue_B,tissue_C,tissue_D,mean_enzymes,rx_AB,rx_AC,rx_AD,rx_BC,rx_BD,rx_CD,rx_two,two_flow,matter_in,"
            "p_total,p_in,p_soil,p_corpse,p_bodies,p_taken_up,p_given_off,p_corpse_eaten,p_corpse_weathered,p_limited,energy_limited,eyed,steering,rooted_brains,gated,sleep_wired,swimmers,walkers,wet_bodies,wet_radius,dry_radius,dissolved_dead,moved_downhill\n";
    return true;
}

double MetricsWriter::write_row(const Simulation& sim, const IntervalTotals& t) {
    const Creatures& c = sim.creatures();
    const std::size_t n = c.size();
    const float half = sim.world().extent() * 0.5f;

    double sum_e = 0, sum_r = 0, sum_photo = 0, sum_parts = 0;
    double photo_b = 0, photo_d = 0, sum_speed = 0, sum_absorb = 0, sum_armour = 0, sum_throw = 0, sum_bite = 0, sum_neurons = 0, sum_links = 0;
    std::size_t brains = 0;
    // CTRNN neurons in running brains: how many, how many are slow (tau of
    // SLOW_TAU ticks or more), and their time constants' geometric mean.
    constexpr float SLOW_TAU = 30.0f;
    std::size_t neurons = 0, slow_neurons = 0;
    std::size_t eyed = 0, steering = 0, rooted_brains = 0, gated = 0, sleep_wired = 0;   // running brains with a sensor point; with a direct link side contrast -> turn (SENSOR_BRAIN)
    double sum_log_tau = 0.0;
    int max_neurons = 0;
    double sum_dormancy = 0, sum_birth = 0;
    std::size_t asleep = 0;
    std::size_t nb = 0, nd = 0;
    uint32_t max_gen = 0;
    std::unordered_set<uint32_t> lineages;
    // Chemistry: what stores and bodies are made of (mass of B, C, D), and how
    // many bodies can run each of the six reactions.
    double pool_m[NCHEM] = {}, tissue_m[NCHEM] = {}, sum_enzymes = 0;
    std::size_t rx[NCHEM][NCHEM] = {};   // one-to-one reactions
    std::size_t rx_two = 0;  // bodies carrying any two-input enzyme
    // Water stage 3: how many bodies live under water, and how big bodies are there and on land.
    std::size_t wet_bodies = 0;
    double wet_r = 0.0, dry_r = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const bool wet = sim.world().under_water(sim.world().cell_index(c.x[i], c.y[i]));
        (wet ? wet_r : dry_r) += c.genome[i].core_radius;
        wet_bodies += wet;
    }
    for (std::size_t i = 0; i < n; ++i) {
        for (int k = 0; k < NCHEM; ++k) { pool_m[k] += c.pool[i].v[k]; tissue_m[k] += c.tissue[i].v[k]; }
        const Phenotype& ph = c.pheno[i];
        sum_enzymes += ph.enzymes;
        bool has[NCHEM][NCHEM] = {};
        for (int e = 0; e < ph.enzymes; ++e)
            if (ph.enz_in[e][1] == NO_CHEM) has[ph.enz_out[e][0]][ph.enz_in[e][0]] = true;
        rx_two += ph.any_two;
        for (int x = 0; x < NCHEM; ++x)
            for (int y = x + 1; y < NCHEM; ++y) rx[x][y] += has[x][y];
    }
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
        sum_dormancy += c.genome[i].dormancy;
        sum_birth += c.genome[i].birth_size;
        asleep += c.sleep[i] >= DEEP_SLEEP ? 1 : 0;
        if (c.pheno[i].has_brain) {   // brains that are actually running
            ++brains;
            eyed += c.brain[i].sensors > 0;
            rooted_brains += c.pheno[i].max_speed <= 0.0f;
            for (int k = 0; k < c.brain[i].links; ++k)
                if (c.brain[i].link[k].to == output_node(OUT_SLEEP) && std::abs(c.brain[i].link[k].weight) > 0.05f) { ++sleep_wired; break; }
            for (int k = 0; k < c.brain[i].links; ++k)
                if (c.brain[i].link[k].to == output_node(OUT_MOUTH) && std::abs(c.brain[i].link[k].weight) > 0.05f) { ++gated; break; }
            for (int k = 0; k < c.brain[i].links; ++k) {
                const BrainLink& l = c.brain[i].link[k];
                if (l.to == output_node(OUT_TURN) && l.from >= IN_BUS && ((l.from - IN_BUS) & 1) && std::abs(l.weight) > 0.05f) { ++steering; break; }
            }
            sum_neurons += c.brain[i].hidden;
            sum_links += c.brain[i].links;
            max_neurons = std::max(max_neurons, static_cast<int>(c.brain[i].hidden));
            const Brain& b = c.genome[i].brain;
            for (int k = 0; k < b.hidden; ++k) {
                ++neurons;
                slow_neurons += b.tau[k] >= SLOW_TAU;
                sum_log_tau += std::log(b.tau[k]);
            }
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
    for (const Mix& v : sim.soil().corpse) corpse += v.mass();
    double waste = 0, vent_loose = 0;   // vent_loose: units of loose VENT_CHEM a vent made
    for (float v : sim.soil().vent_made) vent_loose += v;
    for (float v : sim.soil().waste) waste += v;
    double loose[NCHEM] = {};
    for (const Mix& m : sim.soil().dissolved)
        for (int ch = 0; ch < NCHEM; ++ch) loose[ch] += m.v[ch];

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
         << sum_bite * inv << ','
         << brains << ',' << (brains ? sum_neurons / static_cast<double>(brains) : 0.0) << ','
         << (brains ? sum_links / static_cast<double>(brains) : 0.0) << ',' << max_neurons << ','
         << sum_dormancy * inv << ',' << asleep << ',' << k.poisonous << ',' << t.out_poison << ','
         << t.gene_transfers << ',' << t.gene_transfers_cross << ',' << t.gene_transfers_novel << ','
         << waste << ',' << t.excreted << ',' << t.eaten_waste << ',' << t.out_fouling << ',' << t.out_waste_decay << ','
         << k.cleaners << ',' << loose[1] << ',' << loose[2] << ',' << loose[3] << ','
         << t.in_windfall << ',' << t.windfalls << ',' << t.in_vent << ',' << t.in_meteor << ',' << k.chemotrophs << ',' << vent_loose
         << ',' << k.growing << ',' << sum_birth * inv << ',' << t.grown_mass << ','
         << neurons << ',' << slow_neurons << ',' << (neurons ? std::exp(sum_log_tau / static_cast<double>(neurons)) : 0.0) << ','
         << pool_m[1] << ',' << pool_m[2] << ',' << pool_m[3] << ',' << tissue_m[1] << ',' << tissue_m[2] << ',' << tissue_m[3] << ','
         << sum_enzymes * inv << ',' << rx[0][1] << ',' << rx[0][2] << ',' << rx[0][3] << ',' << rx[1][2] << ',' << rx[1][3] << ',' << rx[2][3] << ','
         << rx_two << ',' << t.two_flow << ',' << t.matter_in << ',';
    {   // stoichiometry: the P book and its flows over the interval
        double soil_p = 0.0, corpse_p = 0.0, bodies_p = 0.0;
        for (float v : sim.soil().mineral) soil_p += v;
        for (float v : sim.soil().corpse_p) corpse_p += v;
        for (std::size_t i = 0; i < sim.creatures().size(); ++i) bodies_p += sim.creatures().tissue_p[i] + sim.creatures().pool_p[i];
        out_ << soil_p + corpse_p + bodies_p << ',' << t.p_in << ',' << soil_p << ',' << corpse_p << ',' << bodies_p << ','
             << t.p_taken_up << ',' << t.p_given_off << ',' << t.p_corpse_eaten << ',' << t.p_corpse_weathered << ','
             << t.p_limited << ',' << t.energy_limited << ',' << eyed << ',' << steering << ',' << rooted_brains << ',' << gated << ',' << sleep_wired << ','
         << k.swimmers << ',' << k.walkers << ',' << wet_bodies << ','
         << (wet_bodies ? wet_r / wet_bodies : 0.0) << ',' << (n > wet_bodies ? dry_r / (n - wet_bodies) : 0.0) << ','
         << t.dissolved_dead << ',' << t.moved_downhill << '\n';
    }
    out_.flush();
    return audit;
}

namespace {
// Body plan columns for the snapshot: summed mouth stalk, and how well its
// tails sit (thrust_facing, size-weighted; 0 with no tail).
float mouth_stalk(const Genome& g) {
    float s = 0.0f;
    for (const Part& p : g.parts) if (p.type == PartType::Absorber) s += p.stalk;
    return s;
}
float tail_facing(const Genome& g) {
    float w = 0.0f, s = 0.0f;
    for (const Part& p : g.parts)
        if (p.type == PartType::Flagellum) { w += p.size; s += p.size * thrust_facing(p.type, p.theta); }
    return w > 0.0f ? s / w : 0.0f;
}
}  // namespace

bool write_creature_snapshot(const Simulation& sim, const std::string& path) {
    std::ofstream f(path);
    if (!f) return false;
    f << "x,y,energy,age,lineage,generation,core_radius,photo,parts,pigment,"
         "absorber,flagellum,legs,sensor,armour,storage,poison,max_speed,fed_light,fed_living,fed_dead,species,seed_throw,bite,neurons,links,dormancy,burns,fed_waste,"
         "elongation,squareness,mouth_stalk,canopy_lift,tail_facing,fed_vent,birth_size,grown,hue,enzymes\n";
    const Creatures& c = sim.creatures();
    for (std::size_t i = 0; i < c.size(); ++i) {
        const Genome& g = c.genome[i];
        f << c.x[i] << ',' << c.y[i] << ',' << c.energy[i] << ',' << c.age[i] << ','
          << c.lineage[i] << ',' << c.generation[i] << ',' << g.core_radius << ','
          << total_part_size(g, PartType::Photo) << ',' << g.parts.size() << ','
          << g.pigment;
        for (int t = 1; t < static_cast<int>(PartType::Count); ++t) f << ',' << c.pheno[i].part_total[t];
        f << ',' << c.pheno[i].max_speed << ',' << c.fed_light[i] << ',' << c.fed_living[i] << ',' << c.fed_dead[i] << ',' << c.species[i] << ',' << g.seed_throw << ',' << g.bite << ','
          << static_cast<int>(g.brain.hidden) << ',' << g.brain.links.size() << ',' << g.dormancy << ',' << static_cast<int>(c.pheno[i].burns) << ',' << c.fed_waste[i] << ','
          << g.elongation << ',' << g.squareness << ',' << mouth_stalk(g) << ',' << c.pheno[i].canopy - c.pheno[i].radius << ',' << tail_facing(g) << ',' << c.fed_vent[i] << ',' << g.birth_size << ',' << c.pheno[i].grown << ',' << g.hue << ',';
        for (std::size_t k = 0; k < g.enzymes.size(); ++k) f << (k ? " " : "") << enzyme_words(g.enzymes[k]);
        f << '\n';
    }
    return true;
}

bool write_species_table(const Simulation& sim, const std::string& path) {
    std::ofstream f(path);
    if (!f) return false;
    f << "id,name,parent,born_tick,extinct_tick,population,peak,established,"
         "core_radius,photo,absorber,flagellum,legs,sensor,armour,storage,poison,brain,split\n";
    for (const Species& s : sim.species().all()) {
        f << s.id << ',' << s.name << ',';
        if (s.parent != NO_SPECIES) f << s.parent;
        f << ',' << s.born_tick << ',';
        if (!s.alive()) f << s.extinct_tick;
        f << ',' << s.population << ',' << s.peak << ',' << (s.established ? 1 : 0) << ','
          << s.representative.core_radius;
        for (int t = 0; t < static_cast<int>(PartType::Count); ++t)
            f << ',' << total_part_size(s.representative, static_cast<PartType>(t));
        f << ',' << (s.representative.brain.empty() ? 0 : 1) << ',' << (s.by_split ? 1 : 0) << '\n';
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
