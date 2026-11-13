// LucaWorld's creature-designer half (see luca_world.h). Everything here is
// conversion: the rules live in core/design.cpp, core/creature_file.cpp and
// the phenotype compiler.

#include "luca_world.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

#include "creature_file.hpp"
#include "energy.hpp"
#include "laws.hpp"
#include "powers.hpp"
#include "sprite_data.hpp"

using namespace godot;

namespace {

constexpr int STRIDE = 20;   // floats per sprite in the instance buffer
constexpr float GUEST_HUE = 0.08f, NATIVE_HUE = 0.42f;

float num(const Dictionary& d, const char* key, float fallback) {
    return d.has(key) ? static_cast<float>(static_cast<double>(d[key])) : fallback;
}

Dictionary to_dict(const luca::Genome& g) {
    Dictionary d;
    d["radius"] = g.core_radius;
    d["pigment"] = g.pigment;
    d["hue"] = g.hue;
    d["thermal_optimum"] = g.thermal_optimum;
    d["thermal_tolerance"] = g.thermal_tolerance;
    d["repro_threshold"] = g.repro_threshold;
    d["offspring_share"] = g.offspring_share;
    d["birth_size"] = g.birth_size;
    d["elongation"] = g.elongation;
    d["squareness"] = g.squareness;
    d["bite"] = g.bite;
    d["dormancy"] = g.dormancy;
    d["seed_throw"] = g.seed_throw;
    Array shape;
    for (float s : g.shape) shape.push_back(s);
    d["shape"] = shape;
    Array parts;
    for (const luca::Part& p : g.parts) {
        Dictionary q;
        q["type"] = static_cast<int>(p.type);
        q["size"] = p.size;
        q["theta"] = p.theta;
        q["stalk"] = p.stalk;
        q["channel"] = static_cast<int>(p.channel);
        parts.push_back(q);
    }
    d["parts"] = parts;
    Array enzymes;
    for (const luca::Enzyme& e : g.enzymes) {
        Dictionary q;
        q["x"] = static_cast<int>(e.x);
        q["y"] = static_cast<int>(e.y);
        q["x2"] = e.two() ? static_cast<int>(e.x2) : -1;
        q["y2"] = e.two() ? static_cast<int>(e.y2) : -1;
        q["rate"] = e.rate;
        q["words"] = String(luca::enzyme_words(e).c_str());
        enzymes.push_back(q);
    }
    d["enzymes"] = enzymes;
    d["hidden"] = static_cast<int>(g.brain.hidden);
    Array tau, bias;
    for (int k = 0; k < g.brain.hidden; ++k) {
        tau.push_back(g.brain.tau[k]);
        bias.push_back(g.brain.bias[k]);
    }
    d["tau"] = tau;
    d["bias"] = bias;
    Array links;
    for (const luca::BrainLink& l : g.brain.links) {
        Dictionary q;
        q["from"] = static_cast<int>(l.from);
        q["to"] = static_cast<int>(l.to);
        q["weight"] = l.weight;
        links.push_back(q);
    }
    d["links"] = links;
    return d;
}

luca::Genome from_dict(const Dictionary& d) {
    luca::Genome g = luca::make_luca();
    g.core_radius = num(d, "radius", g.core_radius);
    g.pigment = num(d, "pigment", g.pigment);
    g.hue = num(d, "hue", g.hue);
    g.thermal_optimum = num(d, "thermal_optimum", g.thermal_optimum);
    g.thermal_tolerance = num(d, "thermal_tolerance", g.thermal_tolerance);
    g.repro_threshold = num(d, "repro_threshold", g.repro_threshold);
    g.offspring_share = num(d, "offspring_share", g.offspring_share);
    g.birth_size = num(d, "birth_size", g.birth_size);
    g.elongation = num(d, "elongation", 0.0f);
    g.squareness = num(d, "squareness", 0.0f);
    g.bite = num(d, "bite", 0.0f);
    g.dormancy = num(d, "dormancy", 0.0f);
    g.seed_throw = num(d, "seed_throw", 0.0f);
    if (d.has("shape")) {
        const Array shape = d["shape"];
        for (int k = 0; k < 8 && k < shape.size(); ++k) g.shape[k] = static_cast<float>(static_cast<double>(shape[k]));
    }
    if (d.has("parts")) {
        g.parts.clear();
        const Array parts = d["parts"];
        for (int64_t k = 0; k < parts.size(); ++k) {
            const Dictionary q = parts[k];
            luca::Part p;
            p.type = static_cast<luca::PartType>(std::clamp(static_cast<int>(q.get("type", 0)), 0, static_cast<int>(luca::PartType::Count) - 1));
            p.size = num(q, "size", 1.0f);
            p.theta = num(q, "theta", 0.0f);
            p.stalk = num(q, "stalk", 0.0f);
            p.channel = static_cast<uint8_t>(std::clamp(static_cast<int>(num(q, "channel", 0.0f)), 0, static_cast<int>(luca::CH_COUNT) - 1));
            g.parts.push_back(p);
        }
    }
    if (d.has("enzymes")) {
        g.enzymes.clear();
        const Array enzymes = d["enzymes"];
        for (int64_t k = 0; k < enzymes.size(); ++k) {
            const Dictionary q = enzymes[k];
            luca::Enzyme e;
            auto chem = [&](const char* key, uint8_t fallback) {
                const int v = q.get(key, -1);
                return v < 0 ? fallback : static_cast<uint8_t>(std::min(v, luca::NCHEM - 1));
            };
            e.x = chem("x", luca::CHEM_A);
            e.y = chem("y", luca::CHEM_D);
            e.x2 = chem("x2", luca::NO_CHEM);
            e.y2 = chem("y2", luca::NO_CHEM);
            e.rate = num(q, "rate", 0.01f);
            g.enzymes.push_back(e);
        }
    }
    g.brain.clear();
    g.brain.hidden = static_cast<uint8_t>(std::clamp(static_cast<int>(d.get("hidden", 0)), 0, luca::MAX_HIDDEN));
    const Array tau = d.get("tau", Array()), bias = d.get("bias", Array());
    for (int k = 0; k < g.brain.hidden; ++k) {
        g.brain.tau[k] = k < tau.size() ? static_cast<float>(static_cast<double>(tau[k])) : 1.0f;
        g.brain.bias[k] = k < bias.size() ? static_cast<float>(static_cast<double>(bias[k])) : 0.0f;
    }
    const Array links = d.get("links", Array());
    for (int64_t k = 0; k < links.size(); ++k) {
        const Dictionary q = links[k];
        const int from = q.get("from", -1), to = q.get("to", -1);
        if (from < 0 || to < 0 || from > 255 || to > 255) continue;
        g.brain.connect(from, to, num(q, "weight", 0.0f));
    }
    luca::clamp_genome(g);
    return g;
}

PackedFloat32Array floats(const float* v, int n) {
    PackedFloat32Array out;
    out.resize(n);
    std::memcpy(out.ptrw(), v, static_cast<std::size_t>(n) * sizeof(float));
    return out;
}

}  // namespace

Dictionary LucaWorld::get_brain_layout() const {
    Dictionary d;
    PackedStringArray inputs, outputs;
    // With SENSOR_BRAIN off the brain has the nine old senses and eleven old urges; on, every sense
    // and only thrust and turn (the lean outputs are not read).
    const bool sensor_brain = luca::g_laws.SENSOR_BRAIN != 0.0f;
    const int n_in = sensor_brain ? luca::NUM_INPUTS : luca::LEGACY_INPUTS;
    const int n_out = sensor_brain ? luca::NUM_OUTPUTS : luca::LEGACY_OUTPUTS;
    for (int i = 0; i < n_in; ++i) inputs.push_back(luca::brain_input_name(i));
    for (int o = 0; o < n_out; ++o) outputs.push_back(luca::brain_output_name(o));
    d["inputs"] = inputs;
    d["outputs"] = outputs;
    d["out_start"] = sensor_brain ? static_cast<int>(luca::OUT_THRUST) : 0;   // outputs before this are not shown
    d["sensor_brain"] = sensor_brain;
    PackedStringArray channels;
    for (int ch = 0; ch < luca::CH_COUNT; ++ch) channels.push_back(luca::channel_name(ch));
    d["channels"] = channels;
    d["first_output"] = luca::FIRST_OUTPUT;
    d["first_hidden"] = luca::FIRST_HIDDEN;
    d["max_hidden"] = luca::MAX_HIDDEN;
    d["max_links"] = luca::MAX_LINKS;
    d["max_weight"] = luca::limits::MAX_WEIGHT;
    d["max_tau"] = luca::limits::MAX_TAU;
    d["max_parts"] = luca::limits::MAX_PARTS;
    d["max_enzymes"] = luca::limits::MAX_ENZYMES;
    d["min_part"] = luca::limits::MIN_PART;
    d["max_part"] = luca::limits::MAX_PART;
    d["max_stalk"] = luca::limits::MAX_STALK;
    d["min_radius"] = luca::limits::MIN_RADIUS;
    d["max_radius"] = luca::limits::MAX_RADIUS;
    d["min_rate"] = luca::limits::MIN_ENZYME_RATE;
    d["max_rate"] = luca::limits::MAX_ENZYME_RATE;
    d["max_throw"] = luca::limits::MAX_THROW;
    d["max_bite"] = luca::limits::MAX_BITE;
    d["min_threshold"] = luca::limits::MIN_THRESHOLD;
    d["max_threshold"] = luca::limits::MAX_THRESHOLD;
    d["min_birth"] = luca::limits::MIN_BIRTH;
    d["vault_sample"] = luca::VAULT_SAMPLE;
    d["day_length"] = sim_ ? sim_->world().day_length() : 600;
    PackedFloat32Array levels;
    for (int c = 0; c < luca::NCHEM; ++c) levels.push_back(luca::LEVEL[c]);
    d["levels"] = levels;
    return d;
}

Array LucaWorld::get_design_templates() const {
    Array out;
    for (const auto& [name, g] : luca::design_templates()) {
        Dictionary t;
        t["name"] = String(name.c_str());
        t["genome"] = to_dict(g);
        out.push_back(t);
    }
    return out;
}

Dictionary LucaWorld::clamp_design(const Dictionary& genome) const { return to_dict(from_dict(genome)); }

Dictionary LucaWorld::assess_design(const Dictionary& genome, double sun, double air) const {
    const luca::Genome g = from_dict(genome);
    const luca::DesignReport r = luca::assess_design(g, static_cast<float>(sun), static_cast<float>(air));
    const luca::Phenotype& p = r.p;
    Dictionary d;
    d["upkeep"] = r.upkeep;
    d["upkeep_old"] = r.upkeep_old;
    d["light_income"] = r.light_income;
    d["thermal"] = r.thermal;
    d["burn_power"] = r.burn_power;
    d["charge_power"] = r.charge_power;
    d["body_energy"] = r.body_energy;
    d["child_cost"] = r.child_cost;
    d["mover"] = r.mover;
    Array costs;
    for (const luca::CostLine& c : r.costs) {
        Dictionary q;
        q["kind"] = String(c.kind.c_str());
        q["index"] = c.index;
        q["upkeep"] = c.upkeep;
        costs.push_back(q);
    }
    d["costs"] = costs;
    PackedStringArray warnings;
    for (const std::string& w : r.warnings) warnings.push_back(String::utf8(w.c_str()));
    d["warnings"] = warnings;
    // The stats a player reads.
    d["mass"] = p.mass;
    d["radius"] = p.radius;
    d["skin"] = p.surface_area;
    d["capacity"] = p.energy_capacity;
    d["repro_threshold"] = p.repro_threshold;
    d["max_speed"] = p.has_brain ? p.max_speed : 0.0f;
    d["could_speed"] = p.max_speed;
    d["has_brain"] = p.has_brain;
    d["sense_cells"] = p.sense_cells;
    d["pull"] = p.absorb * p.bite;
    d["membrane"] = p.membrane;
    d["toxicity"] = p.toxicity;
    d["light_capture"] = p.light_capture;
    d["canopy"] = p.canopy;
    d["reach"] = p.reach;
    d["heat_follow"] = p.heat_follow;
    String burns;
    for (int c = 1; c < luca::NCHEM; ++c)
        if (p.can_burn(c)) burns += String(burns.is_empty() ? "" : " ") + luca::chem_name(c);
    d["burns"] = burns;
    return d;
}

Dictionary LucaWorld::design_sprite(const Dictionary& genome, double fullness) const {
    const luca::Genome g = from_dict(genome);
    const luca::Phenotype p = luca::compile_phenotype(g);
    PackedFloat32Array inst, parts;
    inst.resize(STRIDE);
    parts.resize(PARTS_PER_CREATURE * 4);   // zero-filled
    sprite::write_sprite(inst.ptrw(), g, p, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                         static_cast<float>(std::clamp(fullness, 0.0, 1.0)) * p.repro_threshold, GUEST_HUE, 0.37f);
    sprite::write_parts(parts.ptrw(), g, PARTS_PER_CREATURE);
    Dictionary d;
    d["instances"] = inst;
    d["parts"] = parts;
    return d;
}

Dictionary LucaWorld::probe_brain(const Dictionary& genome, const PackedFloat32Array& inputs, int ticks) const {
    const luca::Genome g = from_dict(genome);
    float in[luca::NUM_INPUTS] = {};
    for (int i = 0; i < luca::NUM_INPUTS && i < inputs.size(); ++i) in[i] = inputs[i];
    in[luca::IN_BIAS] = 1.0f;
    float out[luca::NUM_OUTPUTS], hidden[luca::MAX_HIDDEN];
    luca::probe_brain(g, in, std::clamp(ticks, 1, 5000), out, hidden);
    Dictionary d;
    d["outputs"] = floats(out, luca::NUM_OUTPUTS);
    d["hidden"] = floats(hidden, luca::MAX_HIDDEN);
    return d;
}

String LucaWorld::write_creature_file(const Array& genomes, const String& name, const String& origin) const {
    luca::CreatureFile f;
    f.name = name.utf8().get_data();
    f.origin = origin.utf8().get_data();
    for (int64_t k = 0; k < genomes.size(); ++k) f.bodies.push_back(from_dict(genomes[k]));
    return String::utf8(luca::write_creature_file(f).c_str());
}

Dictionary LucaWorld::read_creature_file(const String& text) const {
    luca::CreatureFile f;
    std::string error;
    Dictionary d;
    d["ok"] = luca::read_creature_file(text.utf8().get_data(), f, error);
    d["error"] = String::utf8(error.c_str());
    d["name"] = String::utf8(f.name.c_str());
    d["origin"] = String::utf8(f.origin.c_str());
    Array bodies;
    for (const luca::Genome& g : f.bodies) bodies.push_back(to_dict(g));
    d["bodies"] = bodies;
    return d;
}

String LucaWorld::bank_species(int64_t species_id) const {
    if (!sim_ || species_id < 0 || species_id >= static_cast<int64_t>(sim_->species().all().size())) return String();
    return String::utf8(luca::write_creature_file(luca::bank_species(*sim_, static_cast<uint32_t>(species_id))).c_str());
}

Dictionary LucaWorld::genome_of(int64_t id) const {
    if (!sim_) return Dictionary();
    const luca::Creatures& c = sim_->creatures();
    for (std::size_t i = 0; i < c.size(); ++i)
        if (c.id[i] == static_cast<uint64_t>(id)) return to_dict(c.genome[i]);
    return Dictionary();
}

bool LucaWorld::release(const String& text, double x, double z, double radius, int count) {
    if (!sim_ || !std::isfinite(x) || !std::isfinite(z) || !std::isfinite(radius)) return false;
    if (!sim_->scenario().info().meteors) return false;   // this scenario is won by evolution alone
    luca::CreatureFile f;
    std::string error;
    if (!luca::read_creature_file(text.utf8().get_data(), f, error)) return false;
    luca::PowerCommand cmd;
    cmd.tick = sim_->tick();
    cmd.kind = luca::PowerKind::Seed;
    cmd.x = static_cast<float>(x);
    cmd.y = static_cast<float>(z);
    cmd.radius = static_cast<float>(std::max(radius, 0.0));
    cmd.amount = static_cast<float>(std::clamp(count, 1, 500));
    cmd.creature = luca::creature_line(f);
    sim_->cast(cmd);
    sim_->flush_powers();   // lands at once, even while paused
    return true;
}

// --- The test tank -----------------------------------------------------------------

bool LucaWorld::trial_start(const String& text, int count, bool with_luca, double sun, double air, int64_t seed) {
    luca::CreatureFile f;
    std::string error;
    if (!luca::read_creature_file(text.utf8().get_data(), f, error)) return false;
    luca::TrialOptions o;
    o.count = std::clamp(count, 1, 200);
    o.with_luca = with_luca;
    o.sun = static_cast<float>(sun);
    o.temperature = static_cast<float>(air);
    o.seed = static_cast<uint64_t>(seed);
    trial_ = std::make_unique<luca::Trial>(f, o);
    return true;
}

void LucaWorld::trial_stop() { trial_.reset(); }

int LucaWorld::trial_step(int max_ticks, double budget_ms) {
    if (!trial_) return 0;
    const auto start = std::chrono::steady_clock::now();
    int ran = 0;
    while (ran < max_ticks) {
        trial_->step(1);
        ++ran;
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        if (ms >= budget_ms) break;
    }
    return ran;
}

Dictionary LucaWorld::trial_info() const {
    Dictionary d;
    if (!trial_) return d;
    const luca::Simulation& s = trial_->sim();
    const luca::Creatures& c = s.creatures();
    d["tick"] = static_cast<int64_t>(s.tick());
    d["guests"] = trial_->guests();
    d["natives"] = trial_->natives();
    d["generations"] = static_cast<int64_t>(trial_->generations());
    d["births"] = static_cast<int64_t>(trial_->guest_births());
    d["extent"] = s.world().extent();
    d["daylight"] = s.world().daylight() * 0.5;
    double light = 0, living = 0, dead = 0, loose = 0, energy = 0;
    int n = 0;
    for (std::size_t i = 0; i < c.size(); ++i) {
        if (!trial_->guest(i)) continue;
        light += c.fed_light[i];
        living += c.fed_living[i];
        dead += c.fed_dead[i];
        loose += c.fed_waste[i] + c.fed_vent[i];
        energy += c.pheno[i].repro_threshold > 0.0f ? std::min(1.0f, c.energy[i] / c.pheno[i].repro_threshold) : 0.0f;
        ++n;
    }
    d["fed_light"] = light;
    d["fed_living"] = living;
    d["fed_dead"] = dead;
    d["fed_loose"] = loose;
    d["mean_energy"] = n > 0 ? energy / n : 0.0;
    return d;
}

PackedFloat32Array LucaWorld::trial_instance_buffer(int capacity) const {
    PackedFloat32Array out;
    capacity = std::max(capacity, 0);
    out.resize(static_cast<int64_t>(capacity) * STRIDE);
    if (!trial_ || capacity == 0) return out;
    const luca::Creatures& c = trial_->sim().creatures();
    const std::size_t n = std::min(c.size(), static_cast<std::size_t>(capacity));
    float* f = out.ptrw();
    for (std::size_t i = 0; i < n; ++i, f += STRIDE)
        sprite::write_sprite(f, c.genome[i], c.pheno[i], c.x[i], c.y[i], c.hx[i], c.hy[i], c.sleep[i], c.energy[i],
                             trial_->guest(i) ? GUEST_HUE : NATIVE_HUE, sprite::hash01(c.id[i]));
    return out;
}

PackedFloat32Array LucaWorld::trial_part_buffer(int capacity) const {
    PackedFloat32Array out;
    capacity = std::max(capacity, 0);
    const int64_t texels = static_cast<int64_t>(capacity) * PARTS_PER_CREATURE;
    const int64_t rows = std::max<int64_t>(1, (texels + PART_TEX_WIDTH - 1) / PART_TEX_WIDTH);
    out.resize(rows * PART_TEX_WIDTH * 4);
    if (!trial_) return out;
    const luca::Creatures& c = trial_->sim().creatures();
    const std::size_t n = std::min(c.size(), static_cast<std::size_t>(capacity));
    for (std::size_t i = 0; i < n; ++i)
        sprite::write_parts(out.ptrw() + static_cast<int64_t>(i) * PARTS_PER_CREATURE * 4, c.genome[i], PARTS_PER_CREATURE);
    return out;
}
