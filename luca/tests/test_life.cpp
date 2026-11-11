// Tests for the rules of life: day and night, mutation, movement, the contact
// primitive, corpses, species, and the two books that must always balance (energy and
// matter). No test framework; a failed CHECK prints the line and the run
// exits non-zero.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "energy.hpp"
#include "mutation.hpp"
#include "sim.hpp"

namespace {

int failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

luca::SimConfig small_config() {
    luca::SimConfig cfg;
    cfg.world_size = 48;
    cfg.initial_population = 0;  // each test places its own creatures
    return cfg;
}

std::size_t find(const luca::Simulation& sim, uint64_t id) {
    const auto& ids = sim.creatures().id;
    for (std::size_t i = 0; i < ids.size(); ++i)
        if (ids[i] == id) return i;
    return ids.size();
}

// A body with one part of the given type added to a LUCA without its patch.
luca::Genome with_part(luca::PartType type, float size) {
    luca::Genome g = luca::make_luca();
    g.parts.clear();
    g.parts.push_back({type, size, 0.0f, 0.0f});
    return g;
}

void test_day_and_night() {
    luca::SimConfig cfg = small_config();
    luca::World w;
    w.generate(cfg);
    const int cell = w.cell_index(5.0f, 1.0f);
    const float noon = w.noon_light(cell);

    double sum = 0.0;
    for (int t = 0; t < cfg.day_length; ++t) {
        w.set_time(static_cast<uint64_t>(t));
        sum += w.daylight();
    }
    CHECK(std::abs(sum / cfg.day_length - 1.0) < 0.02);  // a day averages to the plain sun

    w.set_time(static_cast<uint64_t>(cfg.day_length / 4));      // noon
    CHECK(std::abs(w.light(cell) - 2.0f * noon) < 1e-4f);
    w.set_time(static_cast<uint64_t>(cfg.day_length * 3 / 4));  // midnight
    CHECK(w.light(cell) == 0.0f);
    w.set_time(static_cast<uint64_t>(cfg.day_length * 5 / 4));  // noon again, a day later
    CHECK(std::abs(w.light(cell) - 2.0f * noon) < 1e-4f);

    cfg.day_length = 0;  // no cycle: a constant, average sun
    luca::World flat;
    flat.generate(cfg);
    flat.set_time(12345);
    CHECK(flat.daylight() == 1.0f);
}

void test_mutation() {
    using namespace luca;
    Rng rng(7);
    Genome g = make_luca();
    bool changed = false, saw_brain = false, saw_new_part = false;
    for (int k = 0; k < 20000; ++k) {
        const Genome child = mutate(g, rng);
        changed = changed || child.core_radius != g.core_radius;
        g = child;

        // Every gene stays inside its range, however long the lineage runs.
        CHECK(g.core_radius >= limits::MIN_RADIUS && g.core_radius <= limits::MAX_RADIUS);
        CHECK(g.pigment >= 0.0f && g.pigment <= 1.0f);
        CHECK(g.offspring_share >= limits::MIN_SHARE && g.offspring_share <= limits::MAX_SHARE);
        CHECK(g.parts.size() <= static_cast<std::size_t>(limits::MAX_PARTS));
        CHECK(g.brain.hidden <= MAX_HIDDEN && g.brain.links.size() <= static_cast<std::size_t>(MAX_LINKS));
        for (const Part& p : g.parts) {
            CHECK(p.size >= limits::MIN_PART && p.size <= limits::MAX_PART);
            saw_new_part = saw_new_part || p.type != PartType::Photo;
        }
        saw_brain = saw_brain || !g.brain.empty();

        // And the body it compiles to always has a real cost.
        const Phenotype p = compile_phenotype(g);
        CHECK(std::isfinite(p.upkeep) && p.upkeep > 0.0f && p.mass > 0.0f);
        if (failures) return;
    }
    CHECK(changed);
    CHECK(saw_new_part);  // duplication / addition really does invent structure
    CHECK(saw_brain);     // and a motor part brings a nervous system with it

    // Same seed, same child.
    Rng a(99), b(99);
    const Genome ca = mutate(make_luca(), a), cb = mutate(make_luca(), b);
    CHECK(ca.core_radius == cb.core_radius && ca.parts.size() == cb.parts.size());
}

void test_costs() {
    using namespace luca;
    const Phenotype p = compile_phenotype(make_luca());
    // Wear: an old body costs more than a young one.
    CHECK(upkeep_cost(p, 10000) > upkeep_cost(p, 0) * 1.9f);
    // Comfortable inside the range, paying outside it, more the further out.
    CHECK(thermal_cost(p, {0.0f, p.thermal_optimum, 0.0f}) == 0.0f);
    const float cold = thermal_cost(p, {0.0f, p.thermal_optimum - p.thermal_tolerance - 8.0f, 0.0f});
    const float colder = thermal_cost(p, {0.0f, p.thermal_optimum - p.thermal_tolerance - 16.0f, 0.0f});
    CHECK(cold > 0.0f && colder > cold * 3.0f);
    // Moving costs with the square of speed; climbing costs extra; downhill is not a refund.
    CHECK(movement_cost(p, 0.2f, 0.0f) > movement_cost(p, 0.1f, 0.0f) * 3.9f);
    CHECK(movement_cost(p, 0.1f, 1.0f) > movement_cost(p, 0.1f, 0.0f));
    CHECK(movement_cost(p, 0.1f, -1.0f) == movement_cost(p, 0.1f, 0.0f));

    // No free lunches: every part type adds upkeep and mass.
    for (int t = 0; t < static_cast<int>(PartType::Count); ++t) {
        Genome g = make_luca();
        g.parts.push_back({static_cast<PartType>(t), 1.0f, 0.0f, 0.0f});
        const Phenotype q = compile_phenotype(g);
        CHECK(q.upkeep > p.upkeep && q.mass > p.mass);
    }
    // Armour resists a pull; a body with no absorber pulls nothing.
    Genome armoured = make_luca();
    armoured.parts.push_back({PartType::Armour, 1.0f, 0.0f, 0.0f});
    const Phenotype hunter = compile_phenotype(with_part(PartType::Absorber, 1.0f));
    CHECK(contact_flow(hunter, compile_phenotype(armoured)) < contact_flow(hunter, p) * 0.5f);
    CHECK(contact_flow(p, hunter) == 0.0f);

    // Size at the mouth: the same mouth pulls harder on a smaller body and
    // can only sip from a bigger one, and the edge of being big has a ceiling.
    Genome mouth_g = with_part(PartType::Absorber, 1.0f);
    Genome small_g = mouth_g, big_g = mouth_g, huge_g = mouth_g;
    small_g.core_radius = 0.5f;
    big_g.core_radius = 2.0f;
    huge_g.core_radius = 4.0f;
    const Phenotype mid = compile_phenotype(mouth_g), tiny = compile_phenotype(small_g);
    const Phenotype big = compile_phenotype(big_g), huge = compile_phenotype(huge_g);
    CHECK(contact_flow(tiny, mid) < contact_flow(mid, mid) * 0.3f);
    CHECK(contact_flow(big, mid) > contact_flow(mid, mid) * 1.5f);
    CHECK(contact_flow(huge, mid) == contact_flow(big, mid));
    CHECK(contact_flow(tiny, mid) > 0.0f);            // a parasite's sip, never nothing

    // A body is worth eating: a LUCA's corpse holds about 244.
    CHECK(std::abs(compile_phenotype(make_luca()).body_energy - 244.0f) < 2.0f);
}

void test_movement() {
    using namespace luca;
    Simulation sim(small_config());

    Genome swimmer = make_luca();
    swimmer.parts.push_back({PartType::Flagellum, 1.0f, 0.0f, 0.0f});
    swimmer.brain.connect(input_node(IN_BIAS), output_node(OUT_WANDER), 1.0f);
    swimmer.brain.connect(input_node(IN_BIAS), output_node(OUT_THROTTLE), 0.5f);   // always on the move

    Genome paralysed = swimmer;          // the same body with no brain
    paralysed.brain.clear();

    const uint64_t a = sim.add_creature(24.0f, 6.0f, 300.0f, swimmer);
    const uint64_t b = sim.add_creature(30.0f, 6.0f, 300.0f, paralysed);
    const uint64_t c = sim.add_creature(36.0f, 6.0f, 300.0f, make_luca());

    double moved_cost = 0.0, path = 0.0;
    float lx = 24.0f, ly = 6.0f;
    for (int t = 0; t < 100; ++t) {
        sim.step();
        moved_cost += sim.last_stats().out_movement;
        const std::size_t i = find(sim, a);
        if (i == sim.creatures().size()) break;
        path += std::hypot(sim.creatures().x[i] - lx, sim.creatures().y[i] - ly);
        lx = sim.creatures().x[i];
        ly = sim.creatures().y[i];
    }
    CHECK(path > 3.0);          // it went somewhere
    CHECK(moved_cost > 0.0);    // and paid for it

    const std::size_t ib = find(sim, b), ic = find(sim, c);
    CHECK(ib < sim.creatures().size() && sim.creatures().x[ib] == 30.0f);  // no brain, no movement
    CHECK(ic < sim.creatures().size() && sim.creatures().x[ic] == 36.0f);  // LUCA sits still
}

// One tick-by-tick audit helper: returns the worst relative error seen.
double run_audited(luca::Simulation& sim, int ticks) {
    double start = sim.stored_energy(), in = 0, out = 0, worst = 0;
    for (int t = 0; t < ticks; ++t) {
        sim.step();
        in += sim.last_stats().energy_in;
        out += sim.last_stats().energy_out;
        if ((t + 1) % 50 == 0) {
            const double stored = sim.stored_energy();
            worst = std::max(worst, std::abs(stored - (start + in - out)) / std::max(1.0, std::abs(stored)));
            start = stored;
            in = out = 0;
        }
    }
    return worst;
}

void test_contact_and_corpses() {
    using namespace luca;
    SimConfig cfg = small_config();
    cfg.nutrient_mean = 0.0f;   // nobody can breed: the cast stays as written
    Simulation sim(cfg);

    // A mouth with no patch, touching a LUCA. Far away, a second LUCA alone.
    const uint64_t hunter = sim.add_creature(24.2f, 6.0f, 100.0f, with_part(PartType::Absorber, 1.5f));
    const uint64_t prey   = sim.add_creature(24.6f, 6.0f, 300.0f, make_luca());
    const uint64_t loner  = sim.add_creature(40.5f, 6.0f, 300.0f, make_luca());
    const double matter0 = sim.total_matter();

    double eaten = 0.0, lost = 0.0;
    for (int t = 0; t < 40; ++t) {
        sim.step();
        eaten += sim.last_stats().eaten_living;
        lost += sim.last_stats().out_transfer;
    }
    const std::size_t ih = find(sim, hunter), ip = find(sim, prey), il = find(sim, loner);
    CHECK(ih < sim.creatures().size() && ip < sim.creatures().size() && il < sim.creatures().size());
    if (failures) return;
    const Creatures& c = sim.creatures();
    CHECK(eaten > 0.0);
    CHECK(c.fed_living[ih] > 0.0f && c.fed_light[ih] == 0.0f);  // it lives off its neighbour
    CHECK(c.fed_living[ip] == 0.0f && c.fed_living[il] == 0.0f);
    CHECK(c.energy[ip] < c.energy[il]);                         // and the neighbour is poorer for it
    // What arrives is TRANSFER_EFFICIENCY of what was taken; the rest is heat.
    CHECK(std::abs(lost / (eaten + lost) - (1.0 - TRANSFER_EFFICIENCY)) < 1e-3);

    // Run on until bodies start to fall. The books must balance throughout.
    const double worst = run_audited(sim, 3000);
    CHECK(worst < 1e-4);
    CHECK(std::abs(sim.total_matter() - matter0) < 1e-3 * matter0);

    // Something died, so its matter is in the soil or still in its corpse.
    CHECK(sim.creatures().size() < 3);
    double soil = 0.0;
    for (float v : sim.soil().nutrient) soil += v;
    for (float v : sim.soil().corpse_matter) soil += v;
    CHECK(soil > 0.0);
}

// A corpse feeds a body with a mouth, and being eaten frees its matter.
void test_scavenging() {
    using namespace luca;
    SimConfig cfg = small_config();
    cfg.nutrient_mean = 0.0f;
    Simulation sim(cfg);

    // A body with no way to feed and no energy dies on the first tick and
    // leaves a corpse.
    sim.add_creature(10.5f, 10.5f, 0.0f, with_part(PartType::Storage, 1.0f));
    sim.add_creature(30.5f, 10.5f, 0.0f, with_part(PartType::Storage, 1.0f));  // a second corpse, with nobody near it
    sim.step();
    const int here = sim.world().cell_index(10.5f, 10.5f), there = sim.world().cell_index(30.5f, 10.5f);
    CHECK(sim.creatures().size() == 0);
    CHECK(sim.soil().corpse_energy[here] > 0.0f && sim.soil().corpse_matter[here] > 0.0f);

    const uint64_t eater = sim.add_creature(10.4f, 10.6f, 50.0f, with_part(PartType::Absorber, 0.5f));
    double eaten = 0.0;
    for (int t = 0; t < 250; ++t) {   // a corpse is a big meal now: give it time
        sim.step();
        eaten += sim.last_stats().eaten_dead;
    }
    CHECK(eaten > 0.0);
    const std::size_t i = find(sim, eater);
    CHECK(i < sim.creatures().size() && sim.creatures().fed_dead[i] > 0.0f);
    // The eaten corpse has given up far more of its matter than the one left to weather.
    CHECK(sim.soil().corpse_matter[here] < sim.soil().corpse_matter[there] * 0.5f);
    CHECK(sim.soil().nutrient[here] > sim.soil().nutrient[there]);
}

void test_whole_world() {
    using namespace luca;
    SimConfig cfg;
    cfg.world_size = 64;
    cfg.initial_population = 150;

    Simulation a(cfg), b(cfg);
    a.seed_population();
    b.seed_population();
    const double matter0 = a.total_matter();
    const double worst = run_audited(a, 6000);
    run_audited(b, 6000);

    CHECK(a.creatures().size() > 0);                      // life gets through its nights
    CHECK(worst < 1e-4);                                  // energy: nothing created or destroyed
    CHECK(std::abs(a.total_matter() - matter0) < 1e-3 * matter0);  // matter: the same
    // Determinism: same seed, same world, bit for bit.
    CHECK(a.creatures().size() == b.creatures().size());
    CHECK(a.stored_energy() == b.stored_energy());
    CHECK(a.total_matter() == b.total_matter());
}

// The canopy: where bodies ask for more light than a cell gets, the biggest
// is served first.
void test_canopy() {
    using namespace luca;
    SimConfig cfg = small_config();
    cfg.day_length = 0;  // a steady sun
    Simulation sim(cfg);

    Genome tall = make_luca();
    tall.core_radius = 1.3f;
    Genome small = make_luca();
    small.core_radius = 0.8f;
    Genome sparse = tall;                 // tall, but with a small patch: it lets light through
    sparse.parts[0].size = 0.3f;

    // Three cells, far enough apart that nothing touches or shares light.
    const uint64_t big1 = sim.add_creature(10.5f, 4.5f, 50.0f, tall);
    const uint64_t low1 = sim.add_creature(10.5f, 4.5f, 50.0f, small);
    const uint64_t twin_a = sim.add_creature(20.5f, 4.5f, 50.0f, make_luca());
    const uint64_t twin_b = sim.add_creature(20.5f, 4.5f, 50.0f, make_luca());
    const uint64_t big3 = sim.add_creature(30.5f, 4.5f, 50.0f, sparse);
    const uint64_t low3 = sim.add_creature(30.5f, 4.5f, 50.0f, small);
    sim.step();

    const Creatures& c = sim.creatures();
    auto fed = [&](uint64_t id) { return c.fed_light[find(sim, id)]; };
    auto light_at = [&](float x) { return sim.world().light(sim.world().cell_index(x, 4.5f)); };

    // A full patch on the taller body takes the whole cell; the one beneath starves.
    CHECK(std::abs(fed(big1) - light_at(10.5f)) < 1e-5f);
    CHECK(fed(low1) == 0.0f);
    // Bodies of the same size share alike.
    CHECK(fed(twin_a) > 0.0f && fed(twin_a) == fed(twin_b));
    CHECK(std::abs(fed(twin_a) + fed(twin_b) - light_at(20.5f)) < 1e-5f);
    // A sparse canopy leaves the rest for whatever lives under it.
    CHECK(std::abs(fed(big3) - 0.3f * light_at(30.5f)) < 1e-5f);
    CHECK(std::abs(fed(low3) - compile_phenotype(small).light_capture * light_at(30.5f)) < 1e-5f);
    CHECK(fed(big3) + fed(low3) <= light_at(30.5f) + 1e-5f);
}

// Dispersal is paid for: children land touching the parent unless it can
// fling them, which costs energy, and nothing lands across a cliff.
void test_dispersal() {
    using namespace luca;
    SimConfig cfg = small_config();
    cfg.day_length = 0;
    cfg.nutrient_patchiness = 0.0f;       // plenty of matter everywhere, so births aren't
    cfg.nutrient_mean = 20.0f;            // held up: a child's body comes out of its parent's cell
    cfg.slope_shade = 0.0f;               // and the spike below must not starve its neighbours of light

    Genome thrower = make_luca();
    thrower.seed_throw = 5.0f;
    CHECK(compile_phenotype(thrower).upkeep > compile_phenotype(make_luca()).upkeep);  // the gene has a price
    CHECK(throw_cost(1.0f, 2.0f) > throw_cost(1.0f, 1.0f));

    // Distances from a sessile founder to its own children (generation 1).
    // (The founder itself may be dead by the time we look: a taller child shades it out.)
    auto child_distances = [](const Simulation& sim, uint32_t lineage, float px, float py) {
        std::vector<float> out;
        const Creatures& c = sim.creatures();
        for (std::size_t i = 0; i < c.size(); ++i)
            if (c.generation[i] == 1 && c.lineage[i] == lineage)
                out.push_back(std::hypot(c.x[i] - px, c.y[i] - py));
        return out;
    };
    auto lineage_of = [](const Simulation& sim, uint64_t id) { return sim.creatures().lineage[find(sim, id)]; };

    Simulation sim(cfg);
    const uint32_t stay = lineage_of(sim, sim.add_creature(12.5f, 6.5f, 300.0f, make_luca()));
    const uint32_t fling = lineage_of(sim, sim.add_creature(34.5f, 6.5f, 300.0f, thrower));
    const double matter0 = sim.total_matter();
    double thrown = 0.0;
    double start = sim.stored_energy(), in = 0, out = 0;
    for (int t = 0; t < 2500; ++t) {
        sim.step();
        thrown += sim.last_stats().out_movement;
        in += sim.last_stats().energy_in;
        out += sim.last_stats().energy_out;
    }
    const std::vector<float> near = child_distances(sim, stay, 12.5f, 6.5f);
    const std::vector<float> far = child_distances(sim, fling, 34.5f, 6.5f);
    CHECK(!near.empty() && !far.empty());
    for (float d : near) CHECK(d < 1.3f);                              // touching: about one body away
    CHECK(!far.empty() && *std::max_element(far.begin(), far.end()) > 2.0f);
    CHECK(thrown > 0.0);                                               // and the throws were paid for
    CHECK(std::abs(sim.stored_energy() - (start + in - out)) < 1e-4 * sim.stored_energy());
    CHECK(std::abs(sim.total_matter() - matter0) < 1e-3 * matter0);

    // A cliff: a tall spike beside a thrower. No child comes down on it.
    Simulation walled(cfg);
    // Four throwers around where the spike will stand (bodies are dear, so each has few children).
    const uint32_t line = lineage_of(walled, walled.add_creature(24.5f, 6.5f, 500.0f, thrower));
    walled.add_creature(31.5f, 6.5f, 500.0f, thrower);
    walled.add_creature(28.0f, 3.0f, 500.0f, thrower);
    walled.add_creature(28.0f, 10.0f, 500.0f, thrower);
    PowerCommand spike;
    spike.kind = PowerKind::Terrain;
    spike.x = 28.0f;
    spike.y = 6.5f;
    spike.radius = 3.0f;
    spike.amount = 55.0f;
    walled.cast(spike);
    for (int t = 0; t < 4000; ++t) walled.step();  // a child takes about 1,000 ticks to afford
    const World& w = walled.world();
    const float ground = w.height(w.cell_index(24.5f, 6.5f));
    CHECK(w.height(w.cell_index(28.0f, 6.5f)) > ground + 30.0f);       // the spike is really there
    const Creatures& c = walled.creatures();
    std::size_t children = 0;
    bool on_spike = false;
    for (std::size_t i = 0; i < c.size(); ++i) {
        if (c.generation[i] != 1 || c.lineage[i] < line || c.lineage[i] > line + 3) continue;
        ++children;
        on_spike = on_spike || w.height(w.cell_index(c.x[i], c.y[i])) > ground + 3.0f * cfg.birth_max_step;
    }
    CHECK(children >= 3);
    CHECK(!on_spike);
}

// The shape of the land decides where light falls and where matter gathers.
void test_terrain_shade_and_wash() {
    using namespace luca;
    SimConfig cfg = small_config();
    cfg.max_height = 0.0f;                // a flat plain to build on
    cfg.light_mode = LightMode::Uniform;
    cfg.nutrient_patchiness = 0.0f;
    cfg.wash_rate = 0.05f;                // heavy rain, so a short run shows it

    World w;
    w.generate(cfg);
    const int sunny = w.cell_index(24.5f, 20.5f), top = w.cell_index(24.5f, 24.5f), lee = w.cell_index(24.5f, 28.5f);
    const int plain = w.cell_index(6.5f, 6.5f);
    CHECK(w.shade(sunny) == 1.0f && w.shade(lee) == 1.0f && w.downhill(top) == -1);   // flat: no shade, no flow

    // A hill. The sun stands over y = 0, so the far side is in shadow.
    w.brush(BrushField::Height, 24.5f, 24.5f, 8.0f, 30.0f);
    CHECK(w.shade(lee) < 0.9f);
    CHECK(w.shade(sunny) == 1.0f);
    CHECK(w.noon_light(lee) < w.noon_light(sunny));
    CHECK(w.shade(plain) == 1.0f && w.downhill(plain) == -1);                         // far away: untouched
    CHECK(w.downhill(sunny) >= 0 && w.height(w.downhill(sunny)) < w.height(sunny));
    CHECK(w.steepness(sunny) > 0.0f && w.steepness(sunny) <= 1.0f);
    // A second hill makes a valley between them; its floor is darker than open ground.
    w.brush(BrushField::Height, 36.5f, 24.5f, 5.0f, 30.0f);
    const int floor_cell = w.cell_index(31.5f, 24.5f);
    CHECK(w.shade(floor_cell) < 1.0f);
    // Taking the hills away again gives the plain back.
    w.brush(BrushField::Height, 36.5f, 24.5f, 5.0f, -30.0f);
    w.brush(BrushField::Height, 24.5f, 24.5f, 8.0f, -30.0f);
    CHECK(w.shade(lee) > 0.9999f && w.shade(floor_cell) > 0.9999f);   // (to within float rounding)
    // And the rule can be switched off.
    SimConfig no_shade = cfg;
    no_shade.slope_shade = 0.0f;
    World open;
    open.generate(no_shade);
    open.brush(BrushField::Height, 24.5f, 24.5f, 8.0f, 30.0f);
    CHECK(open.shade(lee) == 1.0f);

    // Rain: matter leaves the slopes and gathers at the foot; none is lost.
    Simulation sim(cfg);
    PowerCommand hill;
    hill.kind = PowerKind::Terrain;
    hill.x = 24.5f;
    hill.y = 24.5f;
    hill.radius = 8.0f;
    hill.amount = 30.0f;
    sim.cast(hill);
    const double matter0 = sim.total_matter();
    const float before = sim.soil().nutrient[static_cast<std::size_t>(sunny)];
    for (int t = 0; t < 3200; ++t) sim.step();
    const auto& soil = sim.soil().nutrient;
    CHECK(soil[static_cast<std::size_t>(sunny)] < 0.5f * before);                     // the slope is washed thin
    CHECK(soil[static_cast<std::size_t>(plain)] == before);                           // the flat keeps what it had
    float richest = 0.0f;
    for (float v : soil) richest = std::max(richest, v);
    CHECK(richest > 2.0f * before);                                                   // somewhere at the foot is rich
    CHECK(std::abs(sim.total_matter() - matter0) < 1e-5 * matter0);
}

// Two feeding specialisations and the danger / kin sense.
void test_feeding_genes_and_kin_sense() {
    using namespace luca;
    const Phenotype plain = compile_phenotype(with_part(PartType::Absorber, 1.0f));
    const Phenotype prey = compile_phenotype(make_luca());

    // Bite: drains the living faster, and costs upkeep even with nothing to bite.
    Genome biter_g = with_part(PartType::Absorber, 1.0f);
    biter_g.bite = 1.0f;
    const Phenotype biter = compile_phenotype(biter_g);
    CHECK(std::abs(contact_flow(biter, prey) - 2.0f * contact_flow(plain, prey)) < 1e-5f);
    CHECK(corpse_demand(biter) == corpse_demand(plain));          // it is no help with the dead
    CHECK(biter.upkeep > plain.upkeep);
    Genome idle = make_luca();
    idle.bite = 1.0f;
    idle.digestion = 1.0f;
    CHECK(compile_phenotype(idle).upkeep > prey.upkeep + 0.1f);   // pure cost to a plant

    // Digestion: keeps more of a meal. Measured from the books of a real meal.
    Genome gut_g = with_part(PartType::Absorber, 1.5f);
    gut_g.digestion = 1.0f;
    CHECK(std::abs(compile_phenotype(gut_g).efficiency - 0.95f) < 1e-5f);
    CHECK(std::abs(plain.efficiency - TRANSFER_EFFICIENCY) < 1e-6f);
    SimConfig cfg = small_config();
    cfg.nutrient_mean = 0.0f;   // nobody breeds
    {
        Simulation sim(cfg);
        sim.add_creature(24.2f, 6.0f, 100.0f, gut_g);
        sim.add_creature(24.6f, 6.0f, 300.0f, make_luca());
        double eaten = 0.0, lost = 0.0;
        for (int t = 0; t < 40; ++t) {
            sim.step();
            eaten += sim.last_stats().eaten_living;
            lost += sim.last_stats().out_transfer;
        }
        CHECK(eaten > 0.0 && std::abs(lost / (eaten + lost) - 0.05) < 1e-3);
        CHECK(run_audited(sim, 500) < 1e-4);
    }

    // The kin sense. A sighted swimmer whose only urge is set by one weight,
    // with one body a cell and a half to its east. Returns how far east it went.
    auto drift = [&](BrainOutput urge, float value, const Genome& neighbour, bool& same_species) {
        Genome g = make_luca();
        g.parts.push_back({PartType::Flagellum, 1.0f, 0.0f, 0.0f});
        g.parts.push_back({PartType::Sensor, 1.0f, 0.0f, 0.0f});
        g.brain.connect(input_node(IN_BIAS), output_node(OUT_THROTTLE), 0.5f);
        g.brain.connect(input_node(IN_BIAS), output_node(urge), value);
        Simulation sim(cfg);
        const uint64_t me = sim.add_creature(20.5f, 6.5f, 300.0f, g);
        Genome other = neighbour;
        if (other.parts.empty()) {          // "one of my own": the same body, unable to move
            other = g;
            other.brain.clear();
        }
        const uint64_t it = sim.add_creature(22.0f, 6.5f, 300.0f, other);
        same_species = sim.creatures().species[find(sim, me)] == sim.creatures().species[find(sim, it)];
        for (int t = 0; t < 10; ++t) sim.step();
        return sim.creatures().x[find(sim, me)] - 20.5f;
    };
    Genome giant = make_luca();
    giant.core_radius = 3.0f;
    bool same = false;
    CHECK(drift(OUT_KIN, 2.0f, Genome{}, same) > 0.5f);      // drawn to its own kind
    CHECK(same);
    CHECK(drift(OUT_KIN, -2.0f, Genome{}, same) < -0.5f);    // or keeping its distance from them
    CHECK(drift(OUT_KIN, 2.0f, giant, same) == 0.0f);        // a stranger is not kin: no pull at all
    CHECK(!same);
    CHECK(drift(OUT_BIGGER, -2.0f, giant, same) < -0.5f);    // fleeing something bigger
    CHECK(drift(OUT_BIGGER, 2.0f, giant, same) > 0.5f);      // or closing in on it
    CHECK(drift(OUT_MOVING, 2.0f, giant, same) == 0.0f);     // it is not moving: nothing to react to
}

// The brain is evolved like the body: its size and wiring change, and cost.
void test_evolving_brain() {
    using namespace luca;
    Genome body = make_luca();
    body.parts.push_back({PartType::Flagellum, 1.0f, 0.0f, 0.0f});

    // Thinking is never free: a link, a stronger link and a neuron each add upkeep.
    Genome reflex = body;
    reflex.brain.connect(input_node(IN_BIAS), output_node(OUT_WANDER), 1.0f);
    Genome stronger = reflex;
    stronger.brain.connect(input_node(IN_BIAS), output_node(OUT_WANDER), 2.0f);
    Genome wired = reflex;
    wired.brain.connect(input_node(IN_HUNGER), output_node(OUT_THROTTLE), 0.0f);
    Genome neuron = reflex;
    neuron.brain.hidden = 1;
    neuron.brain.connect(input_node(IN_HUNGER), hidden_node(0), 1.0f);
    neuron.brain.connect(hidden_node(0), output_node(OUT_THROTTLE), 1.0f);
    const float base = compile_phenotype(reflex).upkeep;
    CHECK(base > compile_phenotype(body).upkeep);
    CHECK(compile_phenotype(stronger).upkeep > base);
    CHECK(compile_phenotype(wired).upkeep > base);
    CHECK(compile_phenotype(neuron).upkeep > compile_phenotype(wired).upkeep);
    // A brain on a body that cannot move does nothing and costs nothing.
    Genome idle = make_luca();
    idle.brain = neuron.brain;
    CHECK(!compile_phenotype(idle).has_brain && compile_phenotype(idle).upkeep == compile_phenotype(make_luca()).upkeep);

    // Thinking. A reflex: output = weight x input.
    float in[NUM_INPUTS] = {}, out[NUM_OUTPUTS] = {};
    in[IN_BIAS] = 1.0f;
    in[IN_HUNGER] = 0.5f;
    BrainState state;
    think(compile_brain(stronger), state, in, out);
    CHECK(out[OUT_WANDER] == 2.0f && out[OUT_THROTTLE] == 0.0f);
    // Through a hidden neuron: tanh of what it hears.
    think(compile_brain(neuron), state, in, out);
    CHECK(std::abs(out[OUT_THROTTLE] - std::tanh(0.5f)) < 1e-6f);
    // Memory: a neuron that listens to itself keeps a signal after the input has gone.
    Genome memory = body;
    memory.brain.hidden = 1;
    memory.brain.connect(input_node(IN_LIGHT), hidden_node(0), 3.0f);
    memory.brain.connect(hidden_node(0), hidden_node(0), 3.0f);
    memory.brain.connect(hidden_node(0), output_node(OUT_THROTTLE), 1.0f);
    const BrainPlan plan = compile_brain(memory);
    BrainState remembered;
    in[IN_LIGHT] = 1.0f;
    think(plan, remembered, in, out);
    in[IN_LIGHT] = 0.0f;                      // the light goes out...
    for (int t = 0; t < 20; ++t) think(plan, remembered, in, out);
    CHECK(out[OUT_THROTTLE] > 0.9f);          // ...and it still knows there was light
    BrainState blank;
    think(plan, blank, in, out);
    CHECK(out[OUT_THROTTLE] == 0.0f);         // one that never saw it does not

    // Evolution changes the structure both ways and never breaks it.
    Rng rng(11);
    Genome g = reflex;
    int most_hidden = 0;
    std::size_t most_links = 0, fewest_links = 99;
    bool shrank = false;
    for (int k = 0; k < 6000; ++k) {
        const int before = g.brain.hidden;
        g = mutate(g, rng);
        if (g.brain.empty()) g = reflex;      // (it lost its tail and then its brain: start again)
        shrank = shrank || g.brain.hidden < before;
        most_hidden = std::max(most_hidden, static_cast<int>(g.brain.hidden));
        most_links = std::max(most_links, g.brain.links.size());
        fewest_links = std::min(fewest_links, g.brain.links.size());
        bool sound = g.brain.hidden <= MAX_HIDDEN && g.brain.links.size() <= static_cast<std::size_t>(MAX_LINKS);
        for (const BrainLink& l : g.brain.links) {
            sound = sound && (l.from < NUM_INPUTS || (l.from >= FIRST_HIDDEN && l.from < FIRST_HIDDEN + g.brain.hidden));
            sound = sound && l.to >= FIRST_OUTPUT && l.to < FIRST_HIDDEN + g.brain.hidden;
            sound = sound && std::abs(l.weight) <= limits::MAX_WEIGHT;
        }
        CHECK(sound);
        if (!sound) break;
    }
    CHECK(most_hidden >= 2 && shrank);        // neurons come and go
    CHECK(most_links > 6 && fewest_links < most_links);

    // Removing a neuron renumbers the ones after it and keeps their wiring.
    Brain b;
    b.hidden = 2;
    b.connect(input_node(IN_BIAS), hidden_node(0), 1.0f);
    b.connect(input_node(IN_BIAS), hidden_node(1), 2.0f);
    b.connect(hidden_node(1), output_node(OUT_WANDER), 3.0f);
    b.remove_hidden(0);
    CHECK(b.hidden == 1 && b.links.size() == 2);
    CHECK(b.links[0].to == hidden_node(0) && b.links[0].weight == 2.0f);
    CHECK(b.links[1].from == hidden_node(0) && b.links[1].weight == 3.0f);
}

// Sunbathing and moving conflict: a body on the move catches less light.
void test_moving_costs_sunlight() {
    using namespace luca;
    CHECK(light_while_moving(0.0f) == 1.0f);
    CHECK(light_while_moving(0.005f) < 1.0f && light_while_moving(0.005f) > light_while_moving(0.01f));
    CHECK(light_while_moving(10.0f) > 0.0f && light_while_moving(10.0f) == light_while_moving(1.0f));   // a floor, not zero

    // The same green swimmer twice, alone under a steady sun: one always on
    // the move, one that never sets off. The sitter takes in more light.
    SimConfig cfg = small_config();
    cfg.day_length = 0;
    cfg.light_mode = LightMode::Uniform;
    cfg.slope_shade = 0.0f;
    cfg.nutrient_mean = 0.0f;   // nobody breeds
    Genome roamer = make_luca();
    roamer.parts.push_back({PartType::Flagellum, 1.0f, 0.0f, 0.0f});
    roamer.brain.connect(input_node(IN_BIAS), output_node(OUT_WANDER), 1.0f);
    roamer.brain.connect(input_node(IN_BIAS), output_node(OUT_THROTTLE), 0.5f);
    Genome sitter = roamer;
    sitter.brain.connect(input_node(IN_BIAS), output_node(OUT_THROTTLE), -1.0f);
    Simulation sim(cfg);
    const uint64_t a = sim.add_creature(12.5f, 24.5f, 100.0f, roamer);
    const uint64_t b = sim.add_creature(36.5f, 24.5f, 100.0f, sitter);
    const double worst = run_audited(sim, 100);
    const std::size_t ia = find(sim, a), ib = find(sim, b);
    CHECK(ia < sim.creatures().size() && ib < sim.creatures().size());
    if (ia < sim.creatures().size() && ib < sim.creatures().size()) {
        CHECK(sim.creatures().x[ib] == 36.5f);                                    // it really sat still
        CHECK(sim.creatures().fed_light[ia] < 0.5f * sim.creatures().fed_light[ib]);
        CHECK(sim.creatures().fed_light[ia] > 0.0f);
    }
    CHECK(worst < 1e-4);
}

// Dormancy: sleeping through the dark saves upkeep, at the price of being
// helpless and slow to wake.
void test_dormancy() {
    using namespace luca;
    Genome sleeper_g = make_luca();
    sleeper_g.dormancy = 1.0f;
    const Phenotype plain = compile_phenotype(make_luca()), sleeper = compile_phenotype(sleeper_g);
    CHECK(sleeper.upkeep > plain.upkeep);                       // the gene has a price even awake

    LocalEnv night{0.0f, 20.0f, 0.0f}, day{1.0f, 20.0f, 0.0f}, frost{1.0f, -5.0f, 0.0f};
    CHECK(next_sleep(plain, night, 0.0f) == 0.0f);              // without the gene, never
    float s = 0.0f;
    for (int t = 0; t < 30; ++t) s = next_sleep(sleeper, night, s);
    CHECK(s == 1.0f);                                           // asleep within half an hour of dusk
    int waking = 0;
    for (; s > 0.0f && waking < 1000; ++waking) s = next_sleep(sleeper, day, s);
    CHECK(waking > 60);                                         // and slow to wake
    CHECK(next_sleep(sleeper, frost, 0.0f) > 0.0f);             // cold sends it under too, even by day
    Genome dozer_g = make_luca();
    dozer_g.dormancy = 0.4f;
    s = 0.0f;
    for (int t = 0; t < 100; ++t) s = next_sleep(compile_phenotype(dozer_g), night, s);
    CHECK(std::abs(s - 0.4f) < 1e-6f);                          // never deeper than its gene allows
    CHECK(cost_while_asleep(0.0f) == 1.0f && cost_while_asleep(1.0f) < 0.2f);

    // Two bodies through one night and morning. Nobody breeds (no soil matter).
    SimConfig cfg = small_config();
    cfg.nutrient_mean = 0.0f;
    cfg.light_mode = LightMode::Uniform;
    cfg.slope_shade = 0.0f;
    Simulation sim(cfg);
    const uint64_t a = sim.add_creature(12.5f, 24.5f, 300.0f, sleeper_g);
    const uint64_t b = sim.add_creature(36.5f, 24.5f, 300.0f, make_luca());
    const int dusk = cfg.day_length / 2, dawn = cfg.day_length;
    double worst = 0.0;
    for (int t = 0; t < dusk + 60; ++t) sim.step();             // into the night
    std::size_t ia = find(sim, a), ib = find(sim, b);
    CHECK(sim.creatures().sleep[ia] == 1.0f && sim.creatures().sleep[ib] == 0.0f);
    const float ea = sim.creatures().energy[ia], eb = sim.creatures().energy[ib];
    const uint32_t age_a = sim.creatures().age[ia], age_b = sim.creatures().age[ib];
    worst = run_audited(sim, 150);                              // deep night
    ia = find(sim, a); ib = find(sim, b);
    const float spent_a = ea - sim.creatures().energy[ia], spent_b = eb - sim.creatures().energy[ib];
    CHECK(spent_a > 0.0f && spent_a < 0.25f * spent_b);         // the sleeper burned far less
    CHECK(sim.creatures().age[ia] - age_a < (sim.creatures().age[ib] - age_b) / 4);   // and barely aged
    CHECK(worst < 1e-4);
    // The first stretch of morning: the sleeper is still coming round and catches less light.
    while (sim.tick() < static_cast<uint64_t>(dawn)) sim.step();
    ia = find(sim, a); ib = find(sim, b);
    const float la = sim.creatures().fed_light[ia], lb = sim.creatures().fed_light[ib];
    for (int t = 0; t < 70; ++t) sim.step();
    ia = find(sim, a); ib = find(sim, b);
    CHECK(sim.creatures().fed_light[ib] - lb > 0.0f);
    CHECK(sim.creatures().fed_light[ia] - la < 0.6f * (sim.creatures().fed_light[ib] - lb));

    // Helpless: a sleeping mouth pulls nothing from the body it touches.
    Genome mouth = with_part(PartType::Absorber, 1.5f);
    mouth.dormancy = 1.0f;
    Simulation dark(cfg);
    dark.add_creature(24.2f, 6.0f, 200.0f, mouth);
    dark.add_creature(24.6f, 6.0f, 300.0f, make_luca());
    for (int t = 0; t < dusk + 60; ++t) dark.step();
    double eaten = 0.0;
    for (int t = 0; t < 100; ++t) {
        dark.step();
        eaten += dark.last_stats().eaten_living;
    }
    CHECK(eaten == 0.0);
}

// Poison: a body that costs its eater energy for every unit drained.
void test_poison() {
    using namespace luca;
    Genome toxic_g = make_luca();
    toxic_g.parts.push_back({PartType::Poison, 1.0f, 0.0f, 0.0f});
    const Phenotype plain = compile_phenotype(make_luca()), toxic = compile_phenotype(toxic_g);
    CHECK(plain.toxicity == 0.0f && toxic.toxicity > 0.7f);     // dearer to eat than a meal is worth
    CHECK(toxic.upkeep > plain.upkeep + 0.1f);                  // and dear to carry
    CHECK(toxic.membrane == plain.membrane);                    // it does not stop the bite, unlike armour
    Genome reeking = toxic_g;
    reeking.parts[1].size = 4.0f;
    CHECK(compile_phenotype(reeking).toxicity == 2.0f);         // capped

    // The same hunter beside a plain LUCA and beside a poisonous one.
    SimConfig cfg = small_config();
    cfg.nutrient_mean = 0.0f;   // nobody breeds
    cfg.day_length = 0;
    const Genome hunter_g = with_part(PartType::Absorber, 1.5f);
    auto feed = [&](const Genome& prey, double& poison_paid, double& worst) {
        Simulation sim(cfg);
        const uint64_t h = sim.add_creature(24.2f, 6.0f, 200.0f, hunter_g);
        sim.add_creature(24.6f, 6.0f, 300.0f, prey);
        double start = sim.stored_energy(), in = 0, out = 0;
        poison_paid = 0.0;
        for (int t = 0; t < 40; ++t) {
            sim.step();
            poison_paid += sim.last_stats().out_poison;
            in += sim.last_stats().energy_in;
            out += sim.last_stats().energy_out;
        }
        worst = std::abs(sim.stored_energy() - (start + in - out)) / sim.stored_energy();
        const std::size_t i = find(sim, h);
        return i < sim.creatures().size() ? sim.creatures().energy[i] : 0.0f;
    };
    double paid_plain = 0, paid_toxic = 0, audit_plain = 0, audit_toxic = 0;
    const float fed = feed(make_luca(), paid_plain, audit_plain);
    const float sick = feed(toxic_g, paid_toxic, audit_toxic);
    CHECK(paid_plain == 0.0 && paid_toxic > 0.0);
    CHECK(fed > 200.0f);                 // an ordinary meal leaves the hunter richer
    CHECK(sick < 200.0f && sick < fed);  // a poisonous one leaves it poorer than not eating at all
    CHECK(audit_plain < 1e-5 && audit_toxic < 1e-5);   // and the burned energy is on the books
}

// Gene transfer: a body that feeds on another can take up one of its parts'
// blueprints. Its own body does not change; its genome does.
void test_gene_transfer() {
    using namespace luca;
    Rng rng(5);
    Genome donor = make_luca();
    donor.parts[0].size = 2.0f;
    Genome taker = with_part(PartType::Absorber, 1.0f);
    PartType copied = PartType::Absorber;
    CHECK(transfer_part(donor, taker, rng, copied));
    CHECK(copied == PartType::Photo && taker.parts.size() == 2);
    CHECK(taker.parts[1].type == PartType::Photo && taker.parts[1].size == 1.0f);   // arrives as a bud, half size
    Genome full = taker;
    while (full.parts.size() < static_cast<std::size_t>(limits::MAX_PARTS)) full.parts.push_back(full.parts[0]);
    CHECK(!transfer_part(donor, full, rng, copied));                                 // no room: nothing happens
    Genome bare = make_luca();
    bare.parts.clear();
    CHECK(!transfer_part(bare, taker, rng, copied));                                 // nothing to copy

    // In the world: a mouth feeding on a LUCA, with transfer made common.
    auto feed = [](float chance, std::size_t& transfers, std::size_t& novel, bool& body_changed, double& matter_drift) {
        SimConfig cfg = small_config();
        cfg.nutrient_mean = 0.0f;       // nobody breeds
        cfg.day_length = 0;
        cfg.gene_transfer_chance = chance;
        Simulation sim(cfg);
        const uint64_t h = sim.add_creature(24.2f, 6.0f, 200.0f, with_part(PartType::Absorber, 1.5f));
        sim.add_creature(24.6f, 6.0f, 300.0f, make_luca());
        const float mass0 = sim.creatures().pheno[find(sim, h)].mass;
        const double matter0 = sim.total_matter();
        transfers = novel = 0;
        for (int t = 0; t < 60; ++t) {
            sim.step();
            transfers += sim.last_stats().gene_transfers;
            novel += sim.last_stats().gene_transfers_novel;
        }
        const std::size_t i = find(sim, h);
        if (i == sim.creatures().size()) return std::size_t{0};
        body_changed = sim.creatures().pheno[i].mass != mass0 || sim.creatures().pheno[i].light_capture != 0.0f;
        matter_drift = std::abs(sim.total_matter() - matter0);
        return static_cast<std::size_t>(total_part_size(sim.creatures().genome[i], PartType::Photo) > 0.0f ? 1 : 0);
    };
    std::size_t transfers = 0, novel = 0;
    bool body_changed = true;
    double drift = 1.0;
    CHECK(feed(0.2f, transfers, novel, body_changed, drift) == 1);   // its genome now holds a sun patch
    CHECK(transfers >= 1 && novel == 1);                             // only the first was new to it
    CHECK(!body_changed);                                            // but the body it already has is as it was
    CHECK(drift < 1e-4);                                             // and no matter appeared
    CHECK(feed(0.0f, transfers, novel, body_changed, drift) == 0);   // switched off: never
    CHECK(transfers == 0);
}

void test_species() {
    using namespace luca;

    // Distance: zero to itself, the same both ways, and a new kind of part counts.
    const Genome luca_g = make_luca();
    Genome mouthed = luca_g;
    mouthed.parts.push_back({PartType::Absorber, 1.0f, 0.0f, 0.0f});
    Genome nudged = luca_g;
    nudged.core_radius *= 1.03f;
    CHECK(genome_distance(luca_g, luca_g) == 0.0f);
    CHECK(genome_distance(luca_g, mouthed) == genome_distance(mouthed, luca_g));
    CHECK(genome_distance(luca_g, nudged) < 0.2f * SPECIES_THRESHOLD);
    CHECK(genome_distance(luca_g, mouthed) > genome_distance(luca_g, nudged));

    // A giant is a different creature from a LUCA; its twin joins it, not a third species.
    Genome giant = luca_g;
    giant.core_radius = 3.0f;
    CHECK(genome_distance(luca_g, giant) > SPECIES_THRESHOLD);
    SpeciesRegistry reg;
    const uint32_t root = reg.found_root(luca_g, 0, "Luca");
    reg.born(root);
    CHECK(reg.classify(root, nudged, 1) == root);
    const uint32_t branch = reg.classify(root, giant, 1);
    CHECK(branch != root);
    CHECK(reg.get(branch).parent == root);
    CHECK(reg.classify(root, giant, 1) == branch);   // founded this tick, founder not yet added
    reg.born(branch);
    CHECK(reg.classify(root, giant, 2) == branch);
    CHECK(reg.living() == 2);
    // Extinction: the last one dies, and a later giant founds a new species.
    CHECK(reg.died(branch, 5));
    CHECK(reg.living() == 1 && reg.get(branch).extinct_tick == 5);
    CHECK(reg.classify(root, giant, 6) != branch);
    // Established only once enough are alive at once.
    bool became = false;
    for (uint32_t k = 1; k < SPECIES_ESTABLISHED; ++k) became = reg.born(root);
    CHECK(became && reg.get(root).established);
    CHECK(!reg.born(root));

    // Names: no two alike among the first few thousand.
    std::vector<std::string> names;
    for (uint32_t id = 0; id < 5000; ++id) names.push_back(species_name(id));
    std::sort(names.begin(), names.end());
    CHECK(std::adjacent_find(names.begin(), names.end()) == names.end());

    CHECK(describe_difference(mouthed, luca_g) == "has grown a mouth");
    CHECK(describe_difference(luca_g, mouthed) == "has lost its mouth");
    CHECK(describe_difference(giant, luca_g) == "is larger");

    // In a living world the species books match the creatures exactly, and
    // new species do appear.
    SimConfig cfg;
    cfg.world_size = 64;
    cfg.initial_population = 150;
    Simulation sim(cfg);
    sim.seed_population();
    CHECK(sim.species().living() == 1);               // identical LUCAs: one species
    for (int t = 0; t < 40000; ++t) sim.step();
    const auto& all = sim.species().all();
    std::vector<uint32_t> count(all.size(), 0);
    for (uint32_t sp : sim.creatures().species) ++count[sp];
    std::size_t living = 0;
    bool books_match = true;
    for (const Species& sp : all) {
        books_match = books_match && sp.population == count[sp.id] && sp.peak >= sp.population;
        living += sp.alive() ? 1 : 0;
    }
    CHECK(books_match);
    CHECK(living == sim.species().living());
    CHECK(all.size() > 1);                            // something has branched off
    CHECK(sim.species().living_established() >= 1);

    // A newcomer like something already alive joins that species.
    const uint32_t before = static_cast<uint32_t>(all.size());
    const Genome resident = sim.species().get(sim.creatures().species[0]).representative;
    sim.add_creature(5.0f, 5.0f, 10.0f, resident);
    CHECK(sim.species().all().size() == before);
}

}  // namespace

int main() {
    test_day_and_night();
    test_mutation();
    test_costs();
    test_movement();
    test_contact_and_corpses();
    test_scavenging();
    test_whole_world();
    test_canopy();
    test_dispersal();
    test_terrain_shade_and_wash();
    test_feeding_genes_and_kin_sense();
    test_evolving_brain();
    test_moving_costs_sunlight();
    test_dormancy();
    test_poison();
    test_gene_transfer();
    test_species();
    if (failures == 0) std::puts("all life tests passed");
    return failures == 0 ? 0 : 1;
}
