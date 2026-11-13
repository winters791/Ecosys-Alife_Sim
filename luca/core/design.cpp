#include "design.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "energy.hpp"
#include "powers.hpp"
#include "sim.hpp"

namespace luca {

namespace {

constexpr float PI = 3.14159265f;

float upkeep_of(const Genome& g) { return upkeep_cost(compile_phenotype(g), 0); }

// Daylight over a day, as World::set_time has it (0 night .. 2 noon, mean 1).
float daylight_at(float phase) {
    return 2.0f * std::clamp(0.5f + 1.5f * std::sin(6.2831853f * phase), 0.0f, 1.0f);
}

std::string fmt(const char* f, double v) {
    char buf[96];
    std::snprintf(buf, sizeof buf, f, v);
    return buf;
}

}  // namespace

DesignReport assess_design(const Genome& genome, float sun, float air) {
    Genome g = genome;
    clamp_genome(g);
    DesignReport r;
    r.p = compile_phenotype(g);
    const Phenotype& p = r.p;
    r.upkeep = upkeep_cost(p, 0);
    r.upkeep_old = upkeep_cost(p, 10000);

    // --- The bill, line by line: what taking each thing away would save. ---
    auto without = [&](auto&& change) {
        Genome h = g;
        change(h);
        return std::max(0.0f, r.upkeep - upkeep_of(h));
    };
    float explained = 0.0f;
    auto add = [&](const char* kind, int index, float u) {
        if (u <= 1e-6f) return;
        r.costs.push_back({kind, index, u});
        explained += u;
    };
    for (std::size_t k = 0; k < g.parts.size(); ++k)
        add("part", static_cast<int>(k), without([&](Genome& h) { h.parts.erase(h.parts.begin() + static_cast<std::ptrdiff_t>(k)); }));
    for (std::size_t k = 0; k < g.enzymes.size(); ++k)
        add("enzyme", static_cast<int>(k), without([&](Genome& h) { h.enzymes.erase(h.enzymes.begin() + static_cast<std::ptrdiff_t>(k)); }));
    add("brain", -1, without([](Genome& h) { h.brain.clear(); }));
    add("tolerance", -1, without([](Genome& h) { h.thermal_tolerance = limits::MIN_TOLERANCE; }));
    add("bite", -1, without([](Genome& h) { h.bite = 0.0f; }));
    add("dormancy", -1, without([](Genome& h) { h.dormancy = 0.0f; }));
    add("throw", -1, without([](Genome& h) { h.seed_throw = 0.0f; }));
    add("shape", -1, without([](Genome& h) { h.elongation = h.squareness = 0.0f; }));
    std::sort(r.costs.begin(), r.costs.end(), [](const CostLine& a, const CostLine& b) { return a.upkeep > b.upkeep; });
    r.costs.insert(r.costs.begin(), CostLine{"body", -1, std::max(0.0f, r.upkeep - explained)});

    // --- What its enzymes can do in a tick. ---
    {
        Mix full;
        for (int c = 1; c < NCHEM; ++c) full.v[c] = p.can_burn(c) ? p.pool_capacity + 1.0f : 0.0f;
        r.burn_power = power(p, full, 1e6f);
        Mix empty;
        float soil = 1e6f;
        r.charge_power = charge(p, empty, soil, 1e9f);
    }

    // --- The sun over one day, sitting still and alone in a cell. ---
    constexpr int SAMPLES = 96;
    float light = 0.0f;
    for (int k = 0; k < SAMPLES; ++k) {
        LocalEnv env;
        env.light = sun * daylight_at((static_cast<float>(k) + 0.5f) / SAMPLES);
        env.temperature = air;
        light += std::min({light_demand(p, env), env.light, r.charge_power});
    }
    r.light_income = light / SAMPLES;
    {
        LocalEnv env;
        env.light = sun;
        env.temperature = air;
        r.thermal = thermal_cost(p, felt_temperature(p, env));
    }
    r.body_energy = p.mass * TOP_LEVEL;
    r.child_cost = p.mass * p.birth_size * TOP_LEVEL;
    r.mover = p.has_brain && p.max_speed > 0.0f;

    // --- What will go wrong. Plain words; the viewer shows them as they are. ---
    auto warn = [&](const std::string& s) { r.warnings.push_back(s); };
    const float needs = r.upkeep + r.thermal;
    if (p.burns == 0)
        warn("It has no enzyme that releases energy: it cannot pay its upkeep and dies at once. Give it one (A-D burns D).");
    else if (r.burn_power < needs)
        warn("Its enzymes release at most " + fmt("%.2f", r.burn_power) + " a tick but it spends " + fmt("%.2f", needs) +
             ": it starves on a full store. Raise an enzyme's rate.");
    if (!p.can_burn(CHEM_D) && p.burns != 0)
        warn("It cannot burn D, which is what plants and corpses are made of: it must live on what its enzymes can burn.");
    const bool motor = p.part_total[static_cast<int>(PartType::Flagellum)] + p.part_total[static_cast<int>(PartType::Legs)] > 0.0f;
    if (motor && g.brain.empty()) warn("It has a motor but no brain to drive it: it will never move. Wire a brain.");
    if (!motor && !g.brain.empty()) warn("It has a brain but no tail or legs: a brain only drives a motor, so it does nothing (and costs nothing).");
    if (r.mover && p.part_total[static_cast<int>(PartType::Photo)] > 0.0f)
        warn("It moves, and sun patches fold shut while it moves (they take a day to open again).");
    if (p.absorb <= 0.0f && p.light_capture <= 0.0f && p.burns == static_cast<uint8_t>(1u << CHEM_D))
        warn("No sun patch and no mouth: nothing feeds it but what it takes up through its skin.");
    if (p.repro_threshold < r.child_cost)
        warn("It divides at " + fmt("%.0f", p.repro_threshold) + " but a child's body costs " + fmt("%.0f", r.child_cost) +
             ": its young are born with an empty store.");
    if (p.repro_threshold >= 0.95f * p.energy_capacity - 1.0f && g.repro_threshold > p.repro_threshold + 1.0f)
        warn("It can't hold enough to divide where you asked: it divides when its store is 95% full instead. Add storage.");
    if (p.sense_cells == 0 && r.mover) warn("It is blind: without a sensor it can't lean towards anything but wander and its own reflexes.");
    return r;
}

std::vector<std::pair<std::string, Genome>> design_templates() {
    auto part = [](PartType t, float size, float theta, float stalk = 0.0f) { return Part{t, size, theta, 0.0f, stalk}; };
    auto wander = [](Genome& g, float throttle) {
        g.brain.connect(input_node(IN_BIAS), output_node(OUT_WANDER), 1.0f);
        g.brain.connect(input_node(IN_BIAS), output_node(OUT_INERTIA), 2.0f);
        g.brain.connect(input_node(IN_BIAS), output_node(OUT_THROTTLE), throttle);
    };
    std::vector<std::pair<std::string, Genome>> cast;
    cast.emplace_back("Luca", make_luca());                              // a plain sitter
    {   Genome g = make_luca();                                          // a long swimmer: tail behind, mouth and eyes in front
        g.elongation = 0.8f;
        g.parts.push_back(part(PartType::Flagellum, 1.2f, PI));
        g.parts.push_back(part(PartType::Absorber, 0.5f, 0.0f));
        g.parts.push_back(part(PartType::Sensor, 0.6f, 0.0f));
        g.hue = 0.55f;                                                   // teal
        wander(g, -0.3f);
        cast.emplace_back("Swimmer", g); }
    {   Genome g = make_luca();                                          // an armoured box: plates all round but the mouth and tail
        g.elongation = 0.4f;
        g.squareness = 1.0f;
        g.parts[0].size = 0.4f;
        g.parts.push_back(part(PartType::Armour, 0.75f, PI * 0.5f));
        g.parts.push_back(part(PartType::Armour, 0.75f, -PI * 0.5f));
        g.parts.push_back(part(PartType::Absorber, 0.9f, 0.0f));
        g.parts.push_back(part(PartType::Flagellum, 1.0f, PI));
        g.parts.push_back(part(PartType::Sensor, 0.3f, 0.3f));
        g.bite = 1.0f;
        g.hue = 0.25f;                                                   // olive
        wander(g, -0.35f);
        cast.emplace_back("Tank", g); }
    {   Genome g = make_luca();                                          // a tentacle monster: mouths out on stalks, armoured body
        g.core_radius = 1.3f;
        g.parts[0].size = 0.3f;
        for (int k = 0; k < 5; ++k) g.parts.push_back(part(PartType::Absorber, 0.4f, -PI * 0.8f + PI * 0.4f * static_cast<float>(k), 0.8f));
        g.parts.push_back(part(PartType::Armour, 2.0f, 0.0f));
        g.parts.push_back(part(PartType::Sensor, 0.8f, 0.0f));
        g.pigment = 0.6f;
        g.hue = 0.78f;                                                   // dusty mauve
        cast.emplace_back("Tentacles", g); }
    {   Genome g = make_luca();                                          // a frond: sun patches held up on stalks
        g.parts[0].stalk = 0.7f;
        g.parts.push_back(part(PartType::Photo, 0.8f, PI * 0.66f, 0.9f));
        g.parts.push_back(part(PartType::Photo, 0.8f, -PI * 0.66f, 0.5f));
        g.pigment = 0.15f;
        g.hue = 0.36f;                                                   // pale green
        cast.emplace_back("Frond", g); }
    {   Genome g = make_luca();                                          // a walker with legs along both sides
        g.elongation = 0.5f;
        g.parts.push_back(part(PartType::Legs, 1.0f, PI * 0.35f));
        g.parts.push_back(part(PartType::Legs, 1.0f, PI * 0.7f));
        g.parts.push_back(part(PartType::Sensor, 1.0f, 0.0f));
        g.parts.push_back(part(PartType::Absorber, 0.4f, 0.0f));
        g.hue = 0.66f;                                                   // blue
        wander(g, -0.42f);
        cast.emplace_back("Walker", g); }
    {   Genome g = make_luca();                                          // fat and poisonous on one side
        g.parts.push_back(part(PartType::Poison, 1.0f, PI));
        g.parts.push_back(part(PartType::Storage, 1.5f, 0.0f));
        g.pigment = 0.7f;
        g.squareness = 0.5f;
        g.hue = 0.95f;                                                   // muted rose
        cast.emplace_back("Puffball", g); }
    {   Genome g = make_luca();                                          // a hunter that feels its way: stops on prey, runs when bitten
        g.parts.clear();
        g.elongation = 0.6f;
        g.parts.push_back(part(PartType::Absorber, 0.8f, 0.0f));
        g.parts.push_back(part(PartType::Flagellum, 1.0f, PI));
        g.parts.push_back(part(PartType::Sensor, 0.8f, 0.0f));
        g.bite = 0.8f;
        g.pigment = 0.55f;
        g.hue = 0.02f;                                                   // rust
        g.brain.connect(input_node(IN_BIAS), output_node(OUT_BIOMASS), 1.5f);
        g.brain.connect(input_node(IN_BIAS), output_node(OUT_INERTIA), 1.0f);
        g.brain.connect(input_node(IN_BIAS), output_node(OUT_THROTTLE), 0.2f);
        g.brain.connect(input_node(IN_EATING), output_node(OUT_THROTTLE), -1.5f);
        g.brain.connect(input_node(IN_HURT), output_node(OUT_THROTTLE), 1.0f);
        g.brain.connect(input_node(IN_BIAS), output_node(OUT_BIGGER), -1.0f);
        cast.emplace_back("Hunter", g); }
    return cast;
}

void probe_brain(const Genome& g, const float* inputs, int ticks, float* outputs, float* hidden) {
    Genome h = g;
    clamp_genome(h);
    const BrainPlan plan = compile_brain(h);
    BrainState state;
    for (int o = 0; o < NUM_OUTPUTS; ++o) outputs[o] = 0.0f;
    for (int t = 0; t < std::max(ticks, 1); ++t) think(plan, state, inputs, outputs);
    for (int k = 0; k < MAX_HIDDEN; ++k)
        hidden[k] = k < plan.hidden ? std::tanh(state.hidden[k] + plan.bias[k]) : 0.0f;
}

CreatureFile bank_species(const Simulation& sim, uint32_t species, int max) {
    CreatureFile f;
    if (species >= sim.species().all().size()) return f;
    const Species& s = sim.species().get(species);
    f.name = s.name;
    f.origin = "banked from a world (seed " + std::to_string(sim.config().seed) + ") at tick " + with_commas(sim.tick());
    const Creatures& c = sim.creatures();
    std::vector<std::size_t> members;
    for (std::size_t i = 0; i < c.size(); ++i)
        if (c.species[i] == species) members.push_back(i);
    const std::size_t take = std::min(members.size(), static_cast<std::size_t>(std::max(max, 1)));
    for (std::size_t k = 0; k < take; ++k) f.bodies.push_back(c.genome[members[k * members.size() / take]]);
    if (f.bodies.empty()) {
        f.bodies.push_back(s.representative);
        f.origin = "the founder of an extinct species, from a world (seed " + std::to_string(sim.config().seed) + ")";
    }
    return f;
}

// --- The test tank ------------------------------------------------------------

Trial::Trial(const CreatureFile& cargo, const TrialOptions& o) {
    SimConfig cfg;
    cfg.seed = o.seed;
    cfg.world_size = std::clamp(o.size, 8, 128);
    cfg.max_height = 1.0f;          // flat: nothing to climb, no shade
    cfg.slope_shade = 0.0f;
    cfg.light_mode = LightMode::Uniform;
    cfg.sun_strength = std::max(0.0f, o.sun);
    cfg.temp_sea_level = o.temperature;
    cfg.temp_lapse = 0.0f;
    cfg.temp_latitude_drop = 0.0f;
    cfg.event_rate = 0.0f;
    cfg.initial_population = o.with_luca ? cfg.world_size * cfg.world_size / 8 : 0;
    sim_ = std::make_unique<Simulation>(cfg);
    if (o.with_luca) sim_->seed_population();
    if (cargo.bodies.empty()) return;
    PowerCommand meteor;
    meteor.kind = PowerKind::Seed;
    meteor.x = meteor.y = sim_->world().extent() * 0.5f;
    meteor.radius = std::max(1.5f, std::sqrt(static_cast<float>(o.count)) * 0.8f);
    meteor.amount = static_cast<float>(std::clamp(o.count, 1, 200));
    meteor.creature = creature_line(cargo);
    sim_->cast(meteor);
    sim_->flush_powers();
    const Creatures& c = sim_->creatures();
    if (c.size() > 0) lineage_ = c.lineage[c.size() - 1];
}

Trial::~Trial() = default;

void Trial::step(int ticks) {
    for (int t = 0; t < ticks; ++t) {
        sim_->step();
        const Creatures& c = sim_->creatures();
        const std::size_t born = sim_->last_stats().births;
        for (std::size_t i = c.size() - std::min(born, c.size()); i < c.size(); ++i)
            if (c.lineage[i] == lineage_) ++guest_births_;
    }
}

bool Trial::guest(std::size_t i) const { return sim_->creatures().lineage[i] == lineage_; }

int Trial::guests() const {
    int n = 0;
    for (uint32_t l : sim_->creatures().lineage) n += l == lineage_ ? 1 : 0;
    return n;
}

int Trial::natives() const { return static_cast<int>(sim_->creatures().size()) - guests(); }

uint32_t Trial::generations() const {
    const Creatures& c = sim_->creatures();
    uint32_t g = 0;
    for (std::size_t i = 0; i < c.size(); ++i)
        if (c.lineage[i] == lineage_) g = std::max(g, c.generation[i]);
    return g;
}

}  // namespace luca
