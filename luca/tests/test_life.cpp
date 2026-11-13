// Tests for the rules of life: day and night, mutation, movement, the contact
// primitive, corpses, species, and the two books that must always balance (energy and
// matter). No test framework; a failed CHECK prints the line and the run
// exits non-zero.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include "body.hpp"
#include "creature_file.hpp"
#include "design.hpp"
#include "powers.hpp"
#include "energy.hpp"
#include "footprint.hpp"
#include "foodweb.hpp"
#include "laws.hpp"
#include "mutation.hpp"
#include "sim.hpp"
#include "snapshot.hpp"

namespace {

int failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

// The default laws, but running the old brain (see main).
luca::Laws legacy_laws() {
    luca::Laws l;
    l.SENSOR_BRAIN = 0.0f;
    return l;
}

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
    g.parts.push_back({type, size, type == luca::PartType::Flagellum ? 3.14159265f : 0.0f, 0.0f});   // a tail at the back
    return g;
}

// Turn a genome's mouths so that, born next in `sim`, they face east (+x),
// where the tests put the prey. A body faces a direction set by its id.
luca::Genome aim_east(const luca::Simulation& sim, luca::Genome g) {
    const float facing = luca::Creatures::initial_wander(sim.next_id());
    for (luca::Part& p : g.parts)
        if (p.type == luca::PartType::Absorber) p.theta += -facing;
    return g;
}

// Contact between two bodies using their whole-body totals.
float flow(const luca::Phenotype& taker, const luca::Phenotype& giver) {
    return luca::contact_flow(taker, taker.absorb, giver, giver.membrane);
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

// Cold nights: the air swings with the day, and a body's own temperature lags
// it, the more so the bigger it is. Nothing tells anything to be big.
void test_cold_nights() {
    using namespace luca;
    SimConfig cfg = small_config();
    World w;
    w.generate(cfg);
    const int cell = w.cell_index(5.0f, 5.0f);
    double sum = 0.0;
    float warmest = -1e9f, coldest = 1e9f;
    int warm_t = 0, cold_t = 0;
    for (int t = 0; t < cfg.day_length; ++t) {
        w.set_time(static_cast<uint64_t>(t));
        const float temp = w.temperature(cell);
        sum += temp - w.mean_temperature(cell);
        if (temp > warmest) { warmest = temp; warm_t = t; }
        if (temp < coldest) { coldest = temp; cold_t = t; }
    }
    CHECK(std::abs(sum / cfg.day_length) < 0.01);                          // the daily mean is unchanged
    CHECK(std::abs((warmest - coldest) - cfg.temp_day_swing) < 0.01f);
    CHECK(warm_t > cfg.day_length / 4 && warm_t < cfg.day_length / 2);     // warmest in the afternoon
    CHECK(cold_t > cfg.day_length / 2);                                    // coldest in the night

    // Three bodies, small, plain and big, through a few days on the same ground.
    Genome small_g = make_luca(), big_g = make_luca();
    small_g.core_radius = 0.5f;
    big_g.core_radius = 2.0f;
    const Phenotype ps = compile_phenotype(small_g), pl = compile_phenotype(make_luca()), pb = compile_phenotype(big_g);
    CHECK(ps.heat_follow > pl.heat_follow && pl.heat_follow > pb.heat_follow);
    float ts = 20.0f, tl = 20.0f, tb = 20.0f;
    float lo_s = 1e9f, hi_s = -1e9f, lo_b = 1e9f, hi_b = -1e9f;
    for (int t = 0; t < 4 * cfg.day_length; ++t) {
        w.set_time(static_cast<uint64_t>(t));
        LocalEnv env = w.env(cell);
        env.light = 0.0f;  // leave pigment out of it
        ts = next_body_temp(ps, env, ts);
        tl = next_body_temp(pl, env, tl);
        tb = next_body_temp(pb, env, tb);
        if (t >= 2 * cfg.day_length) {
            lo_s = std::min(lo_s, ts); hi_s = std::max(hi_s, ts);
            lo_b = std::min(lo_b, tb); hi_b = std::max(hi_b, tb);
        }
    }
    CHECK(hi_s - lo_s < cfg.temp_day_swing);           // even a small body smooths the swing a little
    CHECK(hi_b - lo_b < 0.6f * (hi_s - lo_s));         // a big one far more
    CHECK(lo_b > lo_s);                                // so its nights are warmer

    // No day cycle, no swing.
    cfg.day_length = 0;
    World flat;
    flat.generate(cfg);
    flat.set_time(123);
    CHECK(flat.temperature(cell) == flat.mean_temperature(cell));
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
    CHECK(thermal_cost(p, p.thermal_optimum) == 0.0f);
    const float cold = thermal_cost(p, p.thermal_optimum - p.thermal_tolerance - 8.0f);
    const float colder = thermal_cost(p, p.thermal_optimum - p.thermal_tolerance - 16.0f);
    CHECK(cold > 0.0f && colder > cold * 3.0f);
    // Moving costs more the faster it goes, per tick and per distance; but
    // creeping a distance is not much cheaper than walking it (100 ticks at
    // 0.01 against 10 at 0.1). Climbing costs extra; downhill is not a refund.
    CHECK(movement_cost(p, 0.2f, 0.0f) > movement_cost(p, 0.1f, 0.0f) * 2.0f);
    CHECK(movement_cost(p, 0.2f, 0.0f) > movement_cost(p, 0.1f, 0.0f) * 2.0f * 1.01f);
    CHECK(100.0f * movement_cost(p, 0.01f, 0.0f) > 0.6f * 10.0f * movement_cost(p, 0.1f, 0.0f));
    CHECK(movement_cost(p, 0.1f, 1.0f) > movement_cost(p, 0.1f, 0.0f));
    CHECK(movement_cost(p, 0.1f, -1.0f) == movement_cost(p, 0.1f, 0.0f));

    // No free lunches: every part type adds upkeep and mass.
    for (int t = 0; t < static_cast<int>(PartType::Count); ++t) {
        Genome g = make_luca();
        g.parts.push_back({static_cast<PartType>(t), 1.0f, 0.0f, 0.0f});
        const Phenotype q = compile_phenotype(g);
        CHECK(q.upkeep > p.upkeep && q.mass > p.mass);
    }
    // Armour all round resists a pull; a body with no absorber pulls nothing.
    Genome armoured = make_luca();
    armoured.parts.push_back({PartType::Armour, 2.0f, 0.0f, 0.0f});
    const Phenotype hunter = compile_phenotype(with_part(PartType::Absorber, 1.0f));
    CHECK(flow(hunter, compile_phenotype(armoured)) < flow(hunter, p) * 0.5f);
    CHECK(flow(p, hunter) == 0.0f);

    // Size at the mouth: the same mouth pulls harder on a smaller body and
    // can only sip from a bigger one, and the edge of being big has a ceiling.
    Genome mouth_g = with_part(PartType::Absorber, 1.0f);
    Genome small_g = mouth_g, big_g = mouth_g, huge_g = mouth_g;
    small_g.core_radius = 0.5f;
    big_g.core_radius = 2.0f;
    huge_g.core_radius = 4.0f;
    const Phenotype mid = compile_phenotype(mouth_g), tiny = compile_phenotype(small_g);
    const Phenotype big = compile_phenotype(big_g), huge = compile_phenotype(huge_g);
    CHECK(flow(tiny, mid) < flow(mid, mid) * 0.3f);
    CHECK(flow(big, mid) > flow(mid, mid) * 1.5f);
    CHECK(flow(huge, mid) == flow(big, mid));
    CHECK(flow(tiny, mid) > 0.0f);            // a parasite's sip, never nothing

    // A body is worth eating: a LUCA's body, built of D, holds about 244.
    CHECK(std::abs(compile_phenotype(make_luca()).mass * TOP_LEVEL - 244.0f) < 2.0f);
}

void test_movement() {
    using namespace luca;
    Simulation sim(small_config());

    Genome swimmer = make_luca();
    swimmer.parts.push_back({PartType::Flagellum, 1.0f, 3.14159265f, 0.0f});
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

// 14.9 stage 1: sensors read points, the bus gives level and side contrast,
// and the brain steers with thrust and turn; turning costs energy.
void test_sensor_brain() {
    using namespace luca;
    g_laws.SENSOR_BRAIN = 1.0f;
    SimConfig cfg = small_config();
    cfg.light_mode = LightMode::Gradient;
    cfg.day_length = 0;
    Simulation sim(cfg);
    const auto& lights = sim.world().lights();
    const float bright_top = lights[sim.world().cell_index(24.0f, 4.0f)], bright_bottom = lights[sim.world().cell_index(24.0f, 44.0f)];
    CHECK(bright_top != bright_bottom);

    Genome g = make_luca();
    g.parts.push_back({PartType::Flagellum, 1.0f, 3.14159265f, 0.0f});
    Part eye{PartType::Sensor, 1.0f, 1.5707963f, 0.0f};   // a mirrored pair at the sides
    eye.channel = CH_LIGHT;
    g.parts.push_back(eye);
    g.brain.connect(bus_contrast(CH_LIGHT), output_node(OUT_TURN), 3.0f);   // turn towards the brighter side
    const uint64_t id = sim.add_creature(24.0f, 24.0f, 300.0f, g);

    double cost = 0.0;
    for (int t = 0; t < 300; ++t) { sim.step(); cost += sim.last_stats().out_movement; }
    const std::size_t i = find(sim, id);
    CHECK(i < sim.creatures().size());
    if (i < sim.creatures().size()) {
        const float hy = sim.creatures().hy[i];
        CHECK((bright_top > bright_bottom) == (hy < 0.0f));    // it faces the brighter end
        CHECK(sim.creatures().x[i] == 24.0f && sim.creatures().y[i] == 24.0f);   // thrust 0: it only turned
    }
    CHECK(cost > 0.0);   // and turning was paid for

    // No sensor, no contrast: the same brain on a blind body does not turn.
    Genome blind = g;
    blind.parts.pop_back();
    const uint64_t b = sim.add_creature(10.0f, 24.0f, 300.0f, blind);
    const std::size_t ib0 = find(sim, b);
    const float h0 = sim.creatures().hy[ib0];
    for (int t = 0; t < 50; ++t) sim.step();
    const std::size_t ib = find(sim, b);
    CHECK(ib < sim.creatures().size() && sim.creatures().hy[ib] == h0);
    g_laws.SENSOR_BRAIN = 0.0f;
}

// 14.9 stage 2: a brain on a body that cannot move can shut its mouth.
void test_mouth_gate() {
    using namespace luca;
    g_laws.SENSOR_BRAIN = 1.0f;
    auto drained = [](bool shut_it) {
        SimConfig cfg = small_config();
        cfg.nutrient_mean = 0.0f;
        Simulation sim(cfg);
        Genome hunter = with_part(PartType::Absorber, 1.5f);
        hunter.brain.connect(input_node(IN_BIAS), output_node(OUT_MOUTH), shut_it ? -3.0f : 0.5f);   // shut, or just open (the default)
        const Phenotype p = compile_phenotype(hunter);
        CHECK(p.has_brain && p.max_speed <= 0.0f);   // a brain with no motor, driving a mouth
        sim.add_creature(24.2f, 6.5f, 100.0f, aim_east(sim, hunter));
        sim.add_creature(24.6f, 6.0f, 300.0f, make_luca());
        double eaten = 0.0;
        for (int t = 0; t < 40; ++t) { sim.step(); eaten += sim.last_stats().eaten_living; }
        return eaten;
    };
    const double open_mouth = drained(false), shut_mouth = drained(true);
    CHECK(open_mouth > 0.0);
    CHECK(shut_mouth < 0.1 * open_mouth);   // it shuts from the second tick: the brain's word is read one tick late
    g_laws.SENSOR_BRAIN = 0.0f;
}

// 14.9 stage 3: with the sensor brain, sleep is the brain's word, not the dark-or-cold trigger.
void test_sleep_drive() {
    using namespace luca;
    g_laws.SENSOR_BRAIN = 1.0f;
    SimConfig cfg = small_config();
    cfg.nutrient_mean = 0.0f;
    Simulation sim(cfg);
    Genome base = make_luca();
    base.dormancy = 1.0f;
    Genome reflex = base;                       // brainless: the old trigger, asleep at night
    Genome drowsy = base;                       // a brain that always wants sleep
    drowsy.brain.connect(input_node(IN_BIAS), output_node(OUT_SLEEP), 2.0f);
    Genome insomniac = base;                    // a brain that never does
    insomniac.brain.connect(input_node(IN_BIAS), output_node(OUT_SLEEP), -2.0f);
    CHECK(compile_phenotype(drowsy).has_brain);   // a sleeper has something to drive
    const uint64_t a = sim.add_creature(10.0f, 10.0f, 300.0f, reflex);
    const uint64_t b = sim.add_creature(20.0f, 10.0f, 300.0f, drowsy);
    const uint64_t c = sim.add_creature(30.0f, 10.0f, 300.0f, insomniac);
    float most_a = 0.0f, most_b = 0.0f, most_c = 0.0f;
    for (int t = 0; t < 700; ++t) {   // a whole day, night included
        sim.step();
        most_a = std::max(most_a, sim.creatures().sleep[find(sim, a)]);
        most_b = std::max(most_b, sim.creatures().sleep[find(sim, b)]);
        most_c = std::max(most_c, sim.creatures().sleep[find(sim, c)]);
    }
    CHECK(most_a > 0.5f);    // the reflex sleeps through the night
    CHECK(most_b > 0.9f);    // the drowsy brain sleeps in daylight too
    CHECK(most_c == 0.0f);   // the insomniac never does, night or not
    g_laws.SENSOR_BRAIN = 0.0f;
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
    const uint64_t hunter = sim.add_creature(24.2f, 6.5f, 100.0f, aim_east(sim, with_part(PartType::Absorber, 1.5f)));
    const uint64_t prey   = sim.add_creature(24.6f, 6.0f, 300.0f, make_luca());
    const uint64_t loner  = sim.add_creature(40.5f, 6.0f, 300.0f, make_luca());
    const double matter0 = sim.total_matter();

    double eaten = 0.0, lost = 0.0;
    for (int t = 0; t < 40; ++t) {
        sim.step();
        eaten += sim.last_stats().eaten_living;
        lost += sim.last_stats().excreted;
    }
    const std::size_t ih = find(sim, hunter), ip = find(sim, prey), il = find(sim, loner);
    CHECK(ih < sim.creatures().size() && ip < sim.creatures().size() && il < sim.creatures().size());
    if (failures) return;
    const Creatures& c = sim.creatures();
    CHECK(eaten > 0.0);
    CHECK(c.fed_living[ih] > 0.0f && c.fed_light[ih] == 0.0f);  // it lives off its neighbour
    CHECK(c.fed_living[ip] == 0.0f && c.fed_living[il] == 0.0f);
    CHECK(c.energy[ip] < c.energy[il]);                         // and the neighbour is poorer for it
    // What arrives is TRANSFER_EFFICIENCY of what was taken; the rest spills into the cell.
    CHECK(std::abs(lost / (eaten + lost) - (1.0 - TRANSFER_EFFICIENCY)) < 1e-3);

    // Run on until bodies start to fall. The books must balance throughout.
    const double worst = run_audited(sim, 3000);
    CHECK(worst < 1e-4);
    CHECK(std::abs(sim.total_matter() - matter0) < 1e-3 * matter0);

    // Something died, so its matter is in the soil or still in its corpse.
    CHECK(sim.creatures().size() < 3);
    double soil = 0.0;
    for (float v : sim.soil().nutrient) soil += v;
    for (const Mix& v : sim.soil().corpse) soil += v.mass();
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
    CHECK(sim.soil().corpse_energy[here] > 0.0f && sim.soil().corpse[here].mass() > 0.0f);

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
    CHECK(sim.soil().corpse[here].mass() < sim.soil().corpse[there].mass() * 0.5f);
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
// is served first. Checked under the one-cell rule (LIGHT_FOOTPRINT 0), where
// each body asks in its own cell only; test_geometry_laws checks the footprint.
void test_canopy() {
    using namespace luca;
    SimConfig cfg = small_config();
    cfg.day_length = 0;  // a steady sun
    g_laws = legacy_laws();
    g_laws.LIGHT_FOOTPRINT = 0.0f;
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
    g_laws = legacy_laws();
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
    // Several throwers, far apart: each has only a few children alive at the
    // end and each child is flung a random distance, so one thrower's two
    // children can both land short by chance (about 1 run in 25).
    const float fling_at[4][2] = {{34.5f, 6.5f}, {34.5f, 30.5f}, {12.5f, 30.5f}, {23.5f, 41.5f}};
    uint32_t fling[4];
    for (int k = 0; k < 4; ++k) fling[k] = lineage_of(sim, sim.add_creature(fling_at[k][0], fling_at[k][1], 300.0f, thrower));
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
    std::vector<float> far;
    for (int k = 0; k < 4; ++k)
        for (float d : child_distances(sim, fling[k], fling_at[k][0], fling_at[k][1])) far.push_back(d);
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
        // On the spike itself: within its brush and high up. (Natural ground
        // elsewhere on the small map can stand as high.)
        const float sx = c.x[i] - 28.0f, sy = c.y[i] - 6.5f;
        on_spike = on_spike || (sx * sx + sy * sy < 9.0f &&
                                w.height(w.cell_index(c.x[i], c.y[i])) > ground + 3.0f * cfg.birth_max_step);
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
    CHECK(std::abs(flow(biter, prey) - 2.0f * flow(plain, prey)) < 1e-5f);
    CHECK(corpse_demand(biter) == corpse_demand(plain));          // it is no help with the dead
    CHECK(biter.upkeep > plain.upkeep);
    Genome idle = make_luca();
    idle.bite = 1.0f;
    CHECK(compile_phenotype(idle).upkeep > prey.upkeep + 0.05f);   // pure cost to a plant

    // Every eater keeps the same share of a meal (chemistry stage 3: no
    // digestion gene). Measured from the books of a real meal: the rest spills.
    Genome gut_g = with_part(PartType::Absorber, 1.5f);
    CHECK(std::abs(compile_phenotype(gut_g).efficiency - TRANSFER_EFFICIENCY) < 1e-6f);
    CHECK(std::abs(plain.efficiency - TRANSFER_EFFICIENCY) < 1e-6f);
    SimConfig cfg = small_config();
    cfg.nutrient_mean = 0.0f;   // nobody breeds
    {
        Simulation sim(cfg);
        sim.add_creature(24.2f, 6.5f, 100.0f, aim_east(sim, gut_g));
        sim.add_creature(24.6f, 6.0f, 300.0f, make_luca());
        double eaten = 0.0, lost = 0.0;
        for (int t = 0; t < 40; ++t) {
            sim.step();
            eaten += sim.last_stats().eaten_living;
            lost += sim.last_stats().excreted;
        }
        CHECK(eaten > 0.0 && std::abs(lost / (eaten + lost) - (1.0 - TRANSFER_EFFICIENCY)) < 1e-3);
        CHECK(run_audited(sim, 500) < 1e-4);
    }

    // The kin sense. A sighted swimmer whose only urge is set by one weight,
    // with one body a cell and a half to its east. Returns how far east it went.
    auto drift = [&](BrainOutput urge, float value, const Genome& neighbour, bool& same_species) {
        Genome g = make_luca();
        g.parts.push_back({PartType::Flagellum, 1.0f, 3.14159265f, 0.0f});
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
    body.parts.push_back({PartType::Flagellum, 1.0f, 3.14159265f, 0.0f});

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
    CHECK(light_while_moving(0.002f) < 1.0f && light_while_moving(0.002f) > light_while_moving(0.004f));
    CHECK(light_while_moving(10.0f) > 0.0f && light_while_moving(10.0f) == light_while_moving(1.0f));   // a floor, not zero

    // The same green swimmer twice, alone under a steady sun: one always on
    // the move, one that never sets off. The sitter takes in more light.
    SimConfig cfg = small_config();
    cfg.day_length = 0;
    cfg.light_mode = LightMode::Uniform;
    cfg.slope_shade = 0.0f;
    cfg.nutrient_mean = 0.0f;   // nobody breeds
    Genome roamer = make_luca();
    roamer.parts.push_back({PartType::Flagellum, 1.0f, 3.14159265f, 0.0f});
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
    CHECK(next_sleep(plain, night, night.temperature, 0.0f) == 0.0f);              // without the gene, never
    float s = 0.0f;
    for (int t = 0; t < 30; ++t) s = next_sleep(sleeper, night, night.temperature, s);
    CHECK(s == 1.0f);                                           // asleep within half an hour of dusk
    int waking = 0;
    for (; s > 0.0f && waking < 1000; ++waking) s = next_sleep(sleeper, day, day.temperature, s);
    CHECK(waking > 60);                                         // and slow to wake
    CHECK(next_sleep(sleeper, frost, frost.temperature, 0.0f) > 0.0f);             // cold sends it under too, even by day
    Genome dozer_g = make_luca();
    dozer_g.dormancy = 0.4f;
    s = 0.0f;
    for (int t = 0; t < 100; ++t) s = next_sleep(compile_phenotype(dozer_g), night, night.temperature, s);
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
    dark.add_creature(24.2f, 6.0f, 200.0f, aim_east(dark, mouth));
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
        const uint64_t h = sim.add_creature(24.2f, 6.0f, 200.0f, aim_east(sim, hunter_g));
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
        const uint64_t h = sim.add_creature(24.2f, 6.0f, 200.0f, aim_east(sim, with_part(PartType::Absorber, 1.5f)));
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

// Creature files, the seed power (a life-bearing meteor), the designer's
// budget and the test tank (2026-11-12).
void test_creature_files_and_meteors() {
    using namespace luca;
    // A file round-trips every gene exactly, brain and two-input enzymes included.
    const auto templates = design_templates();
    CHECK(templates.size() >= 8);
    Genome g = templates.back().second;   // the hunter: a wired brain
    g.brain.hidden = 2;
    g.brain.tau[0] = 37.5f; g.brain.bias[0] = -0.25f; g.brain.tau[1] = 3.0f;
    g.brain.connect(input_node(IN_HUNGER), hidden_node(0), 1.25f);
    g.brain.connect(hidden_node(0), hidden_node(1), -0.5f);
    g.brain.connect(hidden_node(1), output_node(OUT_LIGHT), 0.75f);
    g.enzymes.push_back(canonical_enzyme(Enzyme{CHEM_A, CHEM_D, 0.01f, CHEM_A, CHEM_D}));
    g.shape[0] = 0.3f; g.seed_throw = 1.0f;
    clamp_genome(g);
    CreatureFile f;
    f.name = "Spiky;#\nthing";   // separators and comments are cleaned out
    f.origin = "designed";
    f.bodies = {g, make_luca()};
    for (const std::string& text : {write_creature_file(f), creature_line(f)}) {
        CreatureFile back;
        std::string err;
        CHECK(read_creature_file(text, back, err));
        CHECK(back.name == "Spikything" && back.origin == "designed" && back.bodies.size() == 2);
        const Genome& b = back.bodies[0];
        CHECK(b.core_radius == g.core_radius && b.elongation == g.elongation && b.pigment == g.pigment);
        CHECK(b.parts.size() == g.parts.size() && b.enzymes.size() == g.enzymes.size());
        CHECK(b.brain.hidden == 2 && b.brain.tau[0] == 37.5f && b.brain.bias[0] == -0.25f);
        CHECK(b.brain.links.size() == g.brain.links.size());
        for (std::size_t k = 0; k < b.brain.links.size() && k < g.brain.links.size(); ++k)
            CHECK(b.brain.links[k].from == g.brain.links[k].from && b.brain.links[k].to == g.brain.links[k].to &&
                  b.brain.links[k].weight == g.brain.links[k].weight);
        CHECK(enzyme_words(b.enzymes.back()) == "A+A-D+D");
        CHECK(b.shape[0] == 0.3f && b.seed_throw == 1.0f);
        CHECK(compile_phenotype(b).upkeep == compile_phenotype(g).upkeep);
    }
    {   // Bad files are refused with a reason, not half read.
        CreatureFile back;
        std::string err;
        CHECK(!read_creature_file("hello", back, err) && !err.empty());
        CHECK(!read_creature_file("luca-creature 1 ; name x ; body ; wings 3", back, err));
        CHECK(!read_creature_file("luca-creature 1 ; name x", back, err));
        CHECK(!read_creature_file("luca-creature 1 ; body ; link hunger nowhere 1", back, err));
    }

    // The seed power: a script line carries its cargo and round-trips.
    {
        PowerCommand m;
        m.tick = 3; m.kind = PowerKind::Seed; m.x = 10; m.y = 12; m.radius = 3; m.amount = 7;
        m.creature = creature_line(f);
        std::ostringstream out;
        write_power_script(out, {m});
        std::istringstream in(out.str());
        std::vector<PowerCommand> back;
        std::string err;
        CHECK(parse_power_script(in, back, err));
        CHECK(back.size() == 1 && back[0].kind == PowerKind::Seed && back[0].amount == 7.0f && back[0].creature == m.creature);
        std::istringstream bad("0 seed 1 1 1 5 luca-creature 1 ; body ; wings 3\n");
        back.clear();
        CHECK(!parse_power_script(bad, back, err));
    }

    // A meteor lands every body, books its energy and matter as arriving from
    // outside, names the species after the cargo, and replays exactly.
    for (bool between : {false, true}) {
        SimConfig cfg = small_config();
        cfg.windfall_rate = 0.0f;
        Simulation sim(cfg);
        sim.step();
        const double e0 = sim.stored_energy(), m0 = sim.total_matter() - sim.matter_brought_up();
        PowerCommand m;
        m.kind = PowerKind::Seed; m.x = 24; m.y = 24; m.radius = 4; m.amount = 9;
        CreatureFile one;
        one.name = "Spiky";
        one.bodies = {templates[1].second};
        m.tick = sim.tick();
        m.creature = creature_line(one);
        sim.cast(m);
        if (between) sim.flush_powers();
        CHECK(sim.creatures().size() == (between ? 9u : 0u));
        sim.step();
        CHECK(sim.creatures().size() == 9);
        const auto& sp = sim.species().get(sim.creatures().species[0]);
        CHECK(sp.name == "Spiky");
        for (std::size_t i = 0; i < sim.creatures().size(); ++i) {
            CHECK(sim.creatures().species[i] == sim.creatures().species[0]);
            const float dx = sim.creatures().x[i] - 24.0f, dy = sim.creatures().y[i] - 24.0f;
            CHECK(dx * dx + dy * dy <= 4.0f * 4.0f + 1.0f);
        }
        const TickStats& t = sim.last_stats();
        CHECK(t.in_meteor > 0.0 && t.matter_in > 0.0);
        const double audit = sim.stored_energy() - (e0 + t.energy_in - t.energy_out);
        CHECK(std::abs(audit) < 1e-3 * e0 + 1e-2);
        CHECK(std::abs(sim.total_matter() - sim.matter_brought_up() - m0) < 1e-4 * m0);
    }

    // The budget: the bill adds up to the upkeep, and the warnings catch the obvious.
    {
        const DesignReport r = assess_design(make_luca(), 1.0f, 20.0f);
        float sum = 0.0f;
        for (const CostLine& c : r.costs) sum += c.upkeep;
        CHECK(std::abs(sum - r.upkeep) < 1e-4f);
        CHECK(r.costs.front().kind == "body");
        CHECK(r.light_income > r.upkeep);        // a LUCA in full sun pays its way
        CHECK(r.burn_power > r.upkeep && r.warnings.empty());
        Genome no_enzyme = make_luca();
        no_enzyme.enzymes.clear();
        CHECK(!assess_design(no_enzyme, 1.0f, 20.0f).warnings.empty());
        CHECK(assess_design(make_luca(), 0.1f, 20.0f).light_income < r.light_income);
        Genome motor_only = with_part(PartType::Flagellum, 1.0f);
        CHECK(!assess_design(motor_only, 1.0f, 20.0f).warnings.empty());
    }

    // The brain probe answers like think() does.
    {
        float in[NUM_INPUTS] = {1.0f}, out[NUM_OUTPUTS], hid[MAX_HIDDEN];
        probe_brain(templates.back().second, in, 1, out, hid);
        CHECK(std::abs(out[OUT_BIOMASS] - 1.5f) < 1e-5f && std::abs(out[OUT_THROTTLE] - 0.2f) < 1e-5f);
    }

    // The tank: LUCAs alone in a tank thrive; a body that can't burn anything doesn't.
    {
        CreatureFile luca_file;
        luca_file.name = "Luca2";
        luca_file.bodies = {make_luca()};
        TrialOptions o;
        o.with_luca = false;
        o.count = 6;
        Trial a(luca_file, o);
        CHECK(a.guests() == 6 && a.natives() == 0);
        a.step(3000);
        CHECK(a.guests() > 6 && a.guest_births() > 0 && a.generations() >= 1);
        Genome dud = make_luca();
        dud.enzymes.clear();
        CreatureFile dud_file;
        dud_file.bodies = {dud};
        o.with_luca = true;
        Trial b(dud_file, o);
        CHECK(b.natives() > 0);
        b.step(50);
        CHECK(b.guests() == 0 && b.natives() > 0);
    }

    // The seed vault: a sample of a living species, or its founder once gone.
    {
        SimConfig cfg = small_config();
        cfg.initial_population = 30;
        Simulation sim(cfg);
        sim.seed_population();
        const CreatureFile v = bank_species(sim, sim.creatures().species[0], 20);
        CHECK(v.bodies.size() == 20 && v.name == sim.species().get(sim.creatures().species[0]).name);
        CreatureFile back;
        std::string err;
        CHECK(read_creature_file(write_creature_file(v), back, err) && back.bodies.size() == 20);
    }
}

// Snapshots (design doc 14.6.1): a world saved and loaded goes on exactly as
// if it had never stopped, ramps, events, followed species, gene transfer and all.
// Laws (14.6.2): the tunable numbers can be changed without a rebuild, take
// effect where the constants were used, and travel with a snapshot.
// Counter-based draws (14.10.4): a key depends on its inputs only, every
// input matters, and the numbers drawn from keys are evenly spread.
void test_keyed_draws() {
    using namespace luca;
    const uint64_t k = draw_key(42, Draw::Birth, 1000, 7);
    CHECK(k == draw_key(42, Draw::Birth, 1000, 7));
    CHECK(k != draw_key(43, Draw::Birth, 1000, 7));
    CHECK(k != draw_key(42, Draw::Wander, 1000, 7));
    CHECK(k != draw_key(42, Draw::Birth, 1001, 7));
    CHECK(k != draw_key(42, Draw::Birth, 1000, 8));
    CHECK(k != draw_key(42, Draw::Birth, 1000, 7, 1));
    CHECK(draw_key(42, Draw::Transfer, 5, 1, 2) != draw_key(42, Draw::Transfer, 5, 2, 1));   // taker and giver are not the same pair
    // Neighbouring ids and ticks give unrelated numbers.
    double sum = 0.0, sq = 0.0, lag = 0.0;
    float prev = 0.5f;
    const int N = 200000;
    for (int i = 0; i < N; ++i) {
        const float u = key_uniform(draw_key(7, Draw::Wander, static_cast<uint64_t>(i / 100), static_cast<uint64_t>(i % 100)));
        CHECK(u >= 0.0f && u < 1.0f);
        sum += u;
        sq += u * u;
        lag += (u - 0.5) * (prev - 0.5);
        prev = u;
    }
    CHECK(std::abs(sum / N - 0.5) < 0.005);
    CHECK(std::abs(sq / N - 1.0 / 3.0) < 0.005);
    CHECK(std::abs(lag / N) < 0.002);
    double nsum = 0.0, nsq = 0.0;
    for (int i = 0; i < 20000; ++i) {
        Rng r(draw_key(9, Draw::Wander, 3, static_cast<uint64_t>(i)));
        const float x = r.normal();
        nsum += x;
        nsq += x * x;
    }
    CHECK(std::abs(nsum / 20000) < 0.03 && std::abs(nsq / 20000 - 1.0) < 0.05);
}

void test_laws() {
    using namespace luca;
    const Laws defaults{};
    // The tests below run the old brain (main); here the laws are the defaults.
    struct Restore { float v = g_laws.SENSOR_BRAIN; ~Restore() { g_laws.SENSOR_BRAIN = v; } } restore;
    g_laws.SENSOR_BRAIN = defaults.SENSOR_BRAIN;
    CHECK(law_table().size() > 100);
    CHECK(changed_laws(g_laws).empty());

    // Parsing: names in any case, numbers only.
    LawChanges ch;
    std::string err;
    CHECK(parse_law("transfer_efficiency = 0.5", ch, err));
    CHECK(ch.size() == 1 && ch[0].first == "TRANSFER_EFFICIENCY" && ch[0].second == 0.5f);
    CHECK(!parse_law("NO_SUCH_LAW=1", ch, err));
    CHECK(!parse_law("UPTAKE_RATE=fast", ch, err));
    CHECK(!parse_law("UPTAKE_RATE", ch, err));
    CHECK(ch.size() == 1);

    // A law reaches the rules that read it: dearer bodies, a smaller share of a meal.
    const Genome g = make_luca();
    const float upkeep = compile_phenotype(g).upkeep;
    CHECK(std::abs(compile_phenotype(g).efficiency - 0.7f) < 1e-6f);
    g_laws.BASE_UPKEEP = 2.0f * defaults.BASE_UPKEEP;
    apply_laws(g_laws, ch);
    CHECK(compile_phenotype(g).upkeep > upkeep * 1.5f);
    CHECK(compile_phenotype(g).efficiency == 0.5f);
    CHECK(changed_laws(g_laws).size() == 2);
    g_laws = defaults;
    CHECK(compile_phenotype(g).upkeep == upkeep);

    // A world saved under other laws loads under them, whatever the laws of the
    // process loading it, and continues exactly.
    SimConfig cfg;
    cfg.world_size = 48;
    cfg.initial_population = 150;
    g_laws.METABOLIC_RATE = 0.7f;
    Simulation a(cfg);
    a.seed_population();
    for (int t = 0; t < 1500; ++t) a.step();
    const std::string bytes = snapshot_bytes(a);
    CHECK(bytes.find("\nlaw METABOLIC_RATE 0.699999988\n") != std::string::npos);
    g_laws = defaults;
    auto b = snapshot_from_bytes(bytes, err);
    CHECK(b != nullptr);
    if (!b) return;
    CHECK(g_laws.METABOLIC_RATE == 0.7f && changed_laws(g_laws).size() == 1);
    CHECK(snapshot_bytes(*b) == bytes);
    std::vector<float> loaded_upkeep;
    for (std::size_t i = 0; i < b->creatures().size(); ++i) loaded_upkeep.push_back(b->creatures().pheno[i].upkeep);
    for (int t = 0; t < 500; ++t) {
        a.step();
        b->step();
    }
    CHECK(snapshot_bytes(*b) == snapshot_bytes(a));

    // Loaded with a change: the change wins, bodies are recompiled under it,
    // and the chronicle says so.
    g_laws = defaults;
    auto c = snapshot_from_bytes(bytes, err, {{"METABOLIC_RATE", 0.75f}, {"BASE_UPKEEP", 0.4f}});
    CHECK(c != nullptr);
    if (!c) return;
    CHECK(g_laws.METABOLIC_RATE == 0.75f && g_laws.BASE_UPKEEP == 0.4f);
    const std::string& note = c->chronicle().entries().back().text;
    CHECK(note.find("METABOLIC_RATE 0.7 to 0.75") != std::string::npos && note.find("BASE_UPKEEP 0.3 to 0.4") != std::string::npos);
    bool dearer = c->creatures().size() > 0;
    for (std::size_t i = 0; i < c->creatures().size(); ++i)
        dearer = dearer && c->creatures().pheno[i].upkeep > loaded_upkeep[i];
    CHECK(dearer);
    g_laws = defaults;
}

void test_snapshot_round_trip() {
    using namespace luca;
    SimConfig cfg;
    cfg.world_size = 64;
    cfg.initial_population = 300;
    cfg.event_rate = 4.0f;              // storms and meteors part-way through
    cfg.gene_transfer_chance = 0.02f;   // bodies carrying borrowed blueprints
    Simulation a(cfg);
    a.seed_population();
    // Rooted mouths among the plants, so blueprints are taken up early.
    Genome biter = make_luca();
    biter.parts.push_back({PartType::Absorber, 1.5f, 0.0f, 0.0f, 0.0f});
    for (int k = 0; k < 40; ++k) a.add_creature(4.0f + 1.4f * static_cast<float>(k), 20.0f, 400.0f, biter);
    a.cast(PowerCommand{0, PowerKind::Vent, 32.0f, 32.0f, 5.0f, 2.0f});
    a.cast(PowerCommand{3000, PowerKind::Climate, 0.0f, 0.0f, 0.0f, -3.0f, 4000});   // still ramping at the save
    a.cast(PowerCommand{3500, PowerKind::Sun, 0.0f, 0.0f, 0.0f, 0.2f, 0});           // still pending at the save
    a.follow(0);
    for (int t = 0; t < 3300; ++t) a.step();
    CHECK(a.creatures().size() > 100);
    CHECK(a.borrowed_count() > 0);

    const std::string bytes = snapshot_bytes(a);
    std::string err;
    auto b = snapshot_from_bytes(bytes, err);
    CHECK(b != nullptr);
    if (!b) { std::printf("  %s\n", err.c_str()); return; }

    // Loaded, every body is rebuilt exactly as it was.
    CHECK(b->tick() == a.tick() && b->creatures().size() == a.creatures().size());
    CHECK(b->stored_energy() == a.stored_energy() && b->total_matter() == a.total_matter());
    CHECK(b->borrowed_count() == a.borrowed_count());
    const Creatures& ca = a.creatures();
    const Creatures& cb = b->creatures();
    bool same = true;
    for (std::size_t i = 0; i < ca.size(); ++i) {
        const Phenotype &pa = ca.pheno[i], &pb = cb.pheno[i];
        same = same && pa.mass == pb.mass && pa.upkeep == pb.upkeep && pa.grown == pb.grown && pa.max_speed == pb.max_speed &&
               pa.absorb == pb.absorb && pa.light_capture == pb.light_capture && pa.burns == pb.burns && pa.has_brain == pb.has_brain;
        same = same && ca.brain[i].links == cb.brain[i].links && ca.brain[i].hidden == cb.brain[i].hidden;
        same = same && ca.body[i].mouth[0] == cb.body[i].mouth[0] && ca.body[i].armour[3] == cb.body[i].armour[3];
        same = same && ca.energy[i] == cb.energy[i];
    }
    CHECK(same);
    CHECK(b->world().light(100) == a.world().light(100) && b->world().temperature(100) == a.world().temperature(100));
    CHECK(snapshot_bytes(*b) == bytes);   // saving what was loaded gives the same file

    // And it goes on tick for tick.
    for (int t = 0; t < 2500; ++t) {
        a.step();
        b->step();
    }
    CHECK(a.creatures().size() == b->creatures().size());
    CHECK(a.stored_energy() == b->stored_energy() && a.total_matter() == b->total_matter());
    CHECK(a.last_stats().energy_in == b->last_stats().energy_in && a.last_stats().births == b->last_stats().births);
    CHECK(a.chronicle().entries().size() == b->chronicle().entries().size());
    CHECK(snapshot_bytes(a) == snapshot_bytes(*b));

    // A damaged or foreign file is refused, not half-read.
    CHECK(snapshot_from_bytes(bytes.substr(0, bytes.size() - 1000), err) == nullptr);
    CHECK(snapshot_from_bytes("hello\n\n", err) == nullptr);
    std::string newer = bytes;
    newer.replace(newer.find(' ') + 1, 1, "9");
    CHECK(snapshot_from_bytes(newer, err) == nullptr);
}

}  // namespace

// Loose chemicals (chemistry stage 2): a body gives off what it can't burn;
// a neighbour that can burn it takes it up through its skin (cross-feeding,
// with nothing coded for it); it fouls whoever can't burn it; it spreads to
// the next cell and breaks down. Both books balance throughout.
void test_loose_chemicals() {
    using namespace luca;
    Genome maker_g = make_luca(), eater_g = make_luca();
    maker_g.enzymes = {Enzyme{CHEM_B, CHEM_D, 0.015f}};                               // burns D to B, and can't burn B
    eater_g.enzymes = {Enzyme{CHEM_A, CHEM_B, 0.015f}, Enzyme{CHEM_A, CHEM_D, 0.015f}};  // burns B (first) and D
    const Phenotype plain = compile_phenotype(make_luca()), maker = compile_phenotype(maker_g), eater = compile_phenotype(eater_g);
    CHECK(plain.can_burn(CHEM_D) && !plain.can_burn(CHEM_B));
    CHECK(maker.can_burn(CHEM_D) && !maker.can_burn(CHEM_B));
    CHECK(eater.can_burn(CHEM_B) && eater.can_burn(CHEM_D));
    CHECK(eater.upkeep > plain.upkeep);   // a second enzyme has a price
    CHECK(uptake_rate(plain) > 0.0f && fouling_cost(plain, 0.0f) == 0.0f && fouling_cost(plain, 2.0f) > fouling_cost(plain, 1.0f));

    SimConfig cfg = small_config();
    cfg.nutrient_mean = 0.0f;  // nobody breeds
    cfg.light_mode = LightMode::Uniform;
    struct Result { double fouled = 0, given = 0, taken = 0, worst = 0, drift = 0; float loose_b = 0; float eater_fed = -1, plain_fed = -1; };
    auto run = [&](bool with_eater) {
        Result r;
        Simulation sim(cfg);
        sim.add_creature(20.3f, 20.5f, 400.0f, maker_g);
        const uint64_t lid = sim.add_creature(20.5f, 20.2f, 400.0f, make_luca());
        const uint64_t eid = with_eater ? sim.add_creature(20.7f, 20.5f, 400.0f, eater_g) : 0;
        const double matter0 = sim.total_matter();
        for (int t = 0; t < 300; ++t) {
            const double before = sim.stored_energy();
            sim.step();
            const TickStats& st = sim.last_stats();
            r.fouled += st.out_fouling;
            r.given += st.excreted;
            r.taken += st.eaten_waste;
            const double after = sim.stored_energy();
            r.worst = std::max(r.worst, std::abs(after - (before + st.energy_in - st.energy_out)) / std::max(1.0, after));
        }
        r.drift = std::abs(sim.total_matter() - matter0) / matter0;
        r.loose_b = sim.soil().dissolved[sim.world().cell_index(20.5f, 20.5f)].v[CHEM_B];
        if (with_eater) CHECK(!sim.leavings_eaten().empty());   // the leavings tag saw it (observation only)
        const std::size_t il = find(sim, lid);
        if (il < sim.creatures().size()) r.plain_fed = sim.creatures().fed_waste[il];
        if (with_eater) {
            const std::size_t ie = find(sim, eid);
            if (ie < sim.creatures().size()) r.eater_fed = sim.creatures().fed_waste[ie];
        }
        return r;
    };
    const Result alone = run(false), shared = run(true);
    CHECK(alone.given > 0.0 && alone.loose_b > 0.0f);     // the maker gives off B into its cell
    CHECK(alone.fouled > 0.0);                            // which fouls bodies that can't burn it
    CHECK(alone.taken == 0.0 && alone.plain_fed == 0.0f); // and nobody there can eat it
    CHECK(shared.taken > 0.0 && shared.eater_fed > 0.0f); // a neighbour that burns B lives partly on it
    CHECK(shared.plain_fed == 0.0f);
    CHECK(shared.loose_b < alone.loose_b && shared.fouled < alone.fouled);   // and the cell is cleaner for it
    CHECK(alone.worst < 1e-5 && shared.worst < 1e-5);    // the energy books balance
    CHECK(alone.drift < 1e-5 && shared.drift < 1e-5);    // and so does matter

    // Loose chemicals outlive their maker, spread to the next cell, and fade.
    Simulation sim(cfg);
    sim.add_creature(30.5f, 30.5f, 200.0f, maker_g);
    for (int t = 0; t < 3000 && sim.creatures().size() > 0; ++t) sim.step();
    CHECK(sim.creatures().size() == 0);
    const int here = sim.world().cell_index(30.5f, 30.5f), next = sim.world().cell_index(31.5f, 30.5f);
    const float w0 = sim.soil().waste[here];
    CHECK(w0 > 0.0f && sim.soil().dissolved[next].v[CHEM_B] > 0.0f);
    const float a0 = sim.soil().nutrient[here];
    for (int t = 0; t < 800; ++t) sim.step();
    CHECK(sim.soil().waste[here] < 0.5f * w0);
    CHECK(sim.soil().nutrient[here] > a0);   // broken down to A
}

// Wandering bends by distance, not by time: a slow wanderer turns a little
// each tick and a fast one more, so neither spins on the spot and both trace
// paths of about the same shape.
void test_wander_turns_by_distance() {
    using namespace luca;
    SimConfig cfg = small_config();
    cfg.day_length = 0;
    cfg.nutrient_mean = 0.0f;  // nobody breeds
    auto wanderer = [](float motor) {
        Genome g = with_part(PartType::Flagellum, motor);
        g.brain.connect(input_node(IN_BIAS), output_node(OUT_WANDER), 1.0f);
        return g;
    };
    Simulation sim(cfg);
    const uint64_t slow = sim.add_creature(12.5f, 24.5f, 500.0f, wanderer(0.1f));
    const uint64_t fast = sim.add_creature(36.5f, 24.5f, 500.0f, wanderer(2.0f));
    auto heading = [&](uint64_t id) { const std::size_t i = find(sim, id); return std::atan2(sim.creatures().hy[i], sim.creatures().hx[i]); };
    double turn_slow = 0.0, turn_fast = 0.0, path_slow = 0.0;
    float hs = 0.0f, hf = 0.0f;
    const int T = 300;
    for (int t = 0; t < T; ++t) {
        sim.step();
        CHECK(find(sim, slow) < sim.creatures().size() && find(sim, fast) < sim.creatures().size());
        const float ns = heading(slow), nf = heading(fast);
        if (t > 0) {
            turn_slow += std::abs(std::remainder(ns - hs, 6.2831853f));
            turn_fast += std::abs(std::remainder(nf - hf, 6.2831853f));
        }
        path_slow += sim.creatures().moved[find(sim, slow)];
        hs = ns;
        hf = nf;
    }
    CHECK(path_slow > 0.0);                           // it does move
    CHECK(turn_slow / T < 0.15);                      // under ~9 degrees a tick: no spinning
    CHECK(turn_fast > 2.0 * turn_slow);               // the faster one turns more per tick
}

// Where a part sits matters: a mouth only drains what lies in its arc, armour
// only guards the side it covers and clogs what lies under it, and a tail
// pushes best from behind (body.hpp).
void test_part_placement() {
    using namespace luca;
    constexpr float PI = 3.14159265f;
    CHECK(body_dir(1.0f, 0.0f) == 0 && body_dir(-1.0f, 0.0f) == BODY_DIRS / 2 && body_dir(0.0f, 1.0f) == BODY_DIRS / 4);

    Genome g = with_part(PartType::Absorber, 1.0f);              // a mouth at the front
    g.parts.push_back({PartType::Armour, 1.0f, PI * 0.5f, 0.0f});  // a plate over the left side
    BodyPlan b = compile_body(g);
    CHECK(std::abs(b.mouth[0] - 1.0f) < 1e-5f && b.mouth[BODY_DIRS / 2] == 0.0f && !b.even_mouth);
    CHECK(b.any_armour && b.armour[BODY_DIRS / 4] == 1.0f && b.armour[3 * BODY_DIRS / 4] == 0.0f);
    CHECK(clogged(g, g.parts[0]) == 1.0f);                       // the plate does not reach the mouth
    g.parts[0].theta = PI * 0.5f;                                 // move the mouth under the plate
    CHECK(clogged(g, g.parts[0]) < 0.7f);
    CHECK(compile_phenotype(g).absorb < 0.7f);
    // A new mouth buds near the front; other new parts land anywhere.
    Rng rng(11);
    int mouths = 0, front = 0, others = 0, behind = 0;
    for (int k = 0; k < 20000; ++k) {
        const Genome child = mutate(make_luca(), rng);
        for (std::size_t j = 1; j < child.parts.size(); ++j) {
            const bool fwd = std::abs(child.parts[j].theta) <= 0.35f;
            if (child.parts[j].type == PartType::Absorber) { ++mouths; front += fwd; }
            else if (child.parts[j].type != PartType::Photo) { ++others; behind += std::abs(child.parts[j].theta) > 1.5f; }   // copies of the front patch left out
        }
    }
    CHECK(mouths > 20 && front >= mouths * 0.9);
    CHECK(others > 100 && behind > others / 3);
    Genome round = make_luca();
    round.parts.push_back({PartType::Absorber, 4.0f, 0.0f, 0.0f});   // a big enough mouth reaches all round
    CHECK(compile_body(round).even_mouth);

    // A tail behind is four times a tail in front; legs push best from the sides.
    auto speed = [](PartType t, float theta) {
        Genome m = make_luca();
        m.parts.push_back({t, 1.0f, theta, 0.0f});
        return compile_phenotype(m).max_speed;
    };
    CHECK(std::abs(speed(PartType::Flagellum, PI) - 4.0f * speed(PartType::Flagellum, 0.0f)) < 1e-5f);
    CHECK(speed(PartType::Legs, PI * 0.5f) > 1.9f * speed(PartType::Legs, 0.0f));

    // In the world: the same hunter, its mouth towards the prey or away from it.
    SimConfig cfg = small_config();
    cfg.nutrient_mean = 0.0f;   // nobody breeds
    cfg.day_length = 0;
    auto drained = [&](float turn, float prey_plate) {
        Simulation sim(cfg);
        Genome h = aim_east(sim, with_part(PartType::Absorber, 1.0f));
        for (Part& p : h.parts) p.theta += turn;
        sim.add_creature(24.2f, 6.0f, 200.0f, h);
        Genome prey = make_luca();
        if (prey_plate >= 0.0f)   // a plate facing west (towards the hunter) or east
            prey.parts.push_back({PartType::Armour, 0.6f, prey_plate - Creatures::initial_wander(sim.next_id()), 0.0f});
        sim.add_creature(24.6f, 6.0f, 300.0f, prey);
        double eaten = 0.0;
        for (int t = 0; t < 20; ++t) {
            sim.step();
            eaten += sim.last_stats().eaten_living;
        }
        return eaten;
    };
    const double facing = drained(0.0f, -1.0f), away = drained(PI, -1.0f);
    CHECK(facing > 0.0 && away < 0.01 * facing);
    const double plate_towards = drained(0.0f, PI), plate_away = drained(0.0f, 0.0f);
    CHECK(plate_towards < 0.6 * plate_away);   // membrane 1.5 against 0.3: about half
    CHECK(std::abs(plate_away - facing) < 0.02 * facing);       // a plate on the far side is no help
}

// Shape matters: a long or boxy body has more skin than a round one of the
// same volume (room for sun patches, faster heat loss, more fouling, upkeep),
// a long one moves more cheaply and a boxy one drags (body.hpp).
void test_body_shape() {
    using namespace luca;
    CHECK(skin_factor(0.0f, 0.0f) == 1.0f && drag_factor(0.0f, 0.0f) == 1.0f);
    CHECK(std::abs(skin_factor(1.0f, 0.0f) - 1.182f) < 0.005f);      // a 3:1 spheroid
    CHECK(std::abs(skin_factor(0.0f, 1.0f) - 1.24f) < 0.005f);       // about a cube
    CHECK(drag_factor(1.0f, 0.0f) < 0.75f && drag_factor(0.0f, 1.0f) > 1.25f);

    Genome round = make_luca();
    round.parts[0].size = 1.5f;                                       // more patch than a round body has room for
    round.parts.push_back({PartType::Flagellum, 1.0f, 3.14159265f, 0.0f});
    Genome long_g = round, box_g = round;
    long_g.elongation = 1.0f;
    box_g.squareness = 1.0f;
    const Phenotype r = compile_phenotype(round), l = compile_phenotype(long_g), b = compile_phenotype(box_g);
    CHECK(std::abs(l.mass - r.mass) < 1e-6f);                         // same volume, same mass
    CHECK(l.light_capture > r.light_capture * 1.15f);                 // more room for sun
    CHECK(l.heat_follow > r.heat_follow);                             // feels the night more
    CHECK(fouling_cost(l, 10.0f) > fouling_cost(r, 10.0f));           // and its waste
    CHECK(l.upkeep > r.upkeep && b.upkeep > l.upkeep);                // skin is kept up
    CHECK(l.max_speed > 1.3f * r.max_speed && b.max_speed < r.max_speed);
    CHECK(movement_cost(l, 0.05f, 0.0f) < 0.75f * movement_cost(r, 0.05f, 0.0f));
    CHECK(movement_cost(b, 0.05f, 0.0f) > movement_cost(r, 0.05f, 0.0f));
    CHECK(movement_cost(l, 0.0f, 1.0f) == movement_cost(r, 0.0f, 1.0f));   // climbing is the same
}

// Stalks: a mouth on a tentacle reaches bodies its skin does not touch and
// clears its own armour; sun patches on stalks stand over their neighbours.
// Each costs mass and upkeep by length; parts that gain nothing grow none.
void test_stalks() {
    using namespace luca;
    Genome g = with_part(PartType::Absorber, 1.0f);
    g.parts.push_back({PartType::Armour, 2.0f, 0.0f, 0.0f});         // plates all round, over the mouth
    Genome t = g;
    t.parts[0].stalk = 0.6f;
    CHECK(clogged(g, g.parts[0]) < 0.6f && clogged(t, t.parts[0]) == 1.0f);
    const Phenotype pg = compile_phenotype(g), pt = compile_phenotype(t);
    CHECK(pt.absorb > pg.absorb && pt.mass > pg.mass && pt.upkeep > pg.upkeep);
    const BodyPlan bt = compile_body(t);
    CHECK(bt.longest_stalk == 0.6f && bt.stalk[0] == 0.6f && bt.stalk[BODY_DIRS / 2] == 0.0f && !bt.even_mouth);

    Genome odd = make_luca();
    odd.parts.push_back({PartType::Armour, 1.0f, 0.0f, 0.0f, 0.5f});
    clamp_genome(odd);
    CHECK(odd.parts[1].stalk == 0.0f);                              // armour can't grow a stalk

    Genome frond = make_luca();
    frond.parts[0].stalk = 0.5f;
    CHECK(compile_phenotype(frond).canopy > compile_phenotype(make_luca()).canopy + 0.4f);

    // In the world: prey 1.5 apart is out of a bare mouth's touch, but within a tentacle's.
    SimConfig cfg = small_config();
    cfg.nutrient_mean = 0.0f;
    cfg.day_length = 0;
    auto drained = [&](float stalk) {
        Simulation sim(cfg);
        Genome h = with_part(PartType::Absorber, 1.0f);
        h.parts[0].stalk = stalk;
        sim.add_creature(23.6f, 6.0f, 200.0f, aim_east(sim, h));
        sim.add_creature(24.85f, 6.0f, 300.0f, make_luca());
        double eaten = 0.0;
        for (int k = 0; k < 20; ++k) {
            sim.step();
            eaten += sim.last_stats().eaten_living;
        }
        return eaten;
    };
    CHECK(drained(0.0f) == 0.0 && drained(0.5f) > 0.0);

    // In the canopy: a frond overtops a plain LUCA of the same size in the same
    // cell (one-cell rule; under the footprint both also catch light next door).
    g_laws = legacy_laws();
    g_laws.LIGHT_FOOTPRINT = 0.0f;
    Simulation sim(cfg);
    const uint64_t plain = sim.add_creature(20.3f, 20.5f, 300.0f, make_luca());
    const uint64_t tall = sim.add_creature(20.7f, 20.5f, 300.0f, frond);
    for (int k = 0; k < 30; ++k) sim.step();
    CHECK(sim.creatures().fed_light[find(sim, tall)] > 2.0f * sim.creatures().fed_light[find(sim, plain)]);
    g_laws = legacy_laws();
}

// Windfalls: food falls on random cells at the configured mean rate, as
// energy only. The energy books balance with it counted in; the matter books
// don't move; the same seed drops it in the same places; rate 0 drops nothing.
void test_windfalls() {
    using namespace luca;
    SimConfig cfg = small_config();
    cfg.windfall_rate = 0.01f;
    cfg.windfall_size = 50.0f;   // 48 x 48 cells: about 0.46 lumps a tick
    cfg.nutrient_patchiness = 0.0f;   // every cell has the A a lump is made from
    Simulation a(cfg), b(cfg);
    const double matter0 = a.total_matter();
    double start = a.stored_energy(), in = 0, out = 0, fallen = 0;
    std::size_t lumps = 0;
    const int ticks = 2000;
    for (int t = 0; t < ticks; ++t) {
        a.step();
        b.step();
        in += a.last_stats().energy_in;
        out += a.last_stats().energy_out;
        fallen += a.last_stats().in_windfall;
        lumps += a.last_stats().windfalls;
    }
    const double expected = static_cast<double>(ticks) * 48 * 48 * cfg.windfall_rate;
    CHECK(std::abs(fallen - expected) < 0.15 * expected);
    CHECK(std::abs(fallen - static_cast<double>(lumps) * cfg.windfall_size) < 1e-3);
    CHECK(out > 0.0);                                                  // uneaten, it weathers away
    CHECK(std::abs(a.stored_energy() - (start + in - out)) < 1e-4 * a.stored_energy());
    CHECK(std::abs(a.total_matter() - matter0) < 1e-6 * matter0);
    // A lump is D made from the cell's own A: the matter was already there.
    CHECK(a.soil().corpse_energy == b.soil().corpse_energy);           // deterministic

    cfg.windfall_rate = 0.0f;
    Simulation none(cfg);
    for (int t = 0; t < 200; ++t) none.step();
    double left = 0.0;
    for (float e : none.soil().corpse_energy) left += e;
    CHECK(left == 0.0);
}

// Vents: a brush opens one, it warms the ground and gives its cell a steady
// flow of energy, day and night, that only bodies with the chemo gene take up.
// Folding: a body that moves folds its sun patches and they take
// UNFOLD_TICKS to reopen; a body that sits still keeps them open. Being able
// to move costs no light of itself.
void test_folding() {
    using namespace luca;
    Genome mover = make_luca();
    mover.parts.push_back({PartType::Flagellum, 1.0f, 3.14159265f, 0.0f});
    mover.brain.connect(input_node(IN_BIAS), output_node(OUT_WANDER), 1.0f);
    Genome still = mover;
    still.brain.clear();
    CHECK(compile_phenotype(mover).light_capture == compile_phenotype(still).light_capture);
    CHECK(next_unfold(1.0f, 0.0f) == 1.0f);                      // sitting still: open
    CHECK(next_unfold(1.0f, 2.0f * FOLD_STRIDE) == 0.0f);        // one stride: shut
    float u = 0.0f;
    for (int t = 0; t < static_cast<int>(UNFOLD_TICKS / 2); ++t) u = next_unfold(u, 0.0f);
    CHECK(std::abs(u - 0.5f) < 1e-3f);                           // half open after half the time

    // In a world: a wanderer catches far less light than the same body still.
    Simulation sim(small_config());
    mover.brain.connect(input_node(IN_BIAS), output_node(OUT_THROTTLE), 0.5f);
    const uint64_t a = sim.add_creature(10.0f, 6.0f, 600.0f, mover);
    const uint64_t b = sim.add_creature(30.0f, 6.0f, 600.0f, still);
    for (int t = 0; t < 300; ++t) sim.step();
    const Creatures& c = sim.creatures();
    const std::size_t ia = find(sim, a), ib = find(sim, b);
    CHECK(ia < c.size() && ib < c.size());
    if (ia < c.size() && ib < c.size()) CHECK(c.fed_light[ia] < 0.2f * c.fed_light[ib]);
}

// A species holding two separate clusters of genomes is split in two at the
// next check; a species of one kind is not. Following it reports the split.
void test_species_split() {
    using namespace luca;
    // The first body founds the species; then two groups that each differ from
    // it in one gene (distance 0.6 and 0.7) and from each other in both (0.92).
    auto run = [](bool apart, bool watch, Simulation& sim) {
        sim.add_creature(24.0f, 24.0f, 300.0f, make_luca());
        Genome a = make_luca(), b = a;
        if (apart) {
            a.pigment += 0.3f;
            b.thermal_tolerance += 14.0f;
        }
        for (int k = 0; k < 20; ++k) sim.add_creature(4.0f + (k % 5) * 2.0f, 4.0f + (k / 5) * 2.0f, 300.0f, a);
        // Both groups in the same band of light, so neither dwindles below a
        // quarter of the species by chance (in the dim corner it could).
        for (int k = 0; k < 20; ++k) sim.add_creature(34.0f + (k % 5) * 2.0f, 4.0f + (k / 5) * 2.0f, 300.0f, b);
        if (watch) sim.follow(0);
        for (int t = 0; t < 2000; ++t) sim.step();
    };

    Simulation two(small_config());
    run(true, true, two);
    const auto& all = two.species().all();
    CHECK(genome_distance(make_luca(), all[0].representative) == 0.0f);
    const Species* split = nullptr;
    for (const Species& s : all) if (s.by_split) split = &s;
    CHECK(split != nullptr);
    if (split) {
        CHECK(split->parent == 0 && split->established && split->alive());
        const float tol = make_luca().thermal_tolerance + 7.0f;
        CHECK(split->representative.thermal_tolerance > tol);   // the side further from the founder leaves
        const Creatures& c = two.creatures();
        for (std::size_t i = 0; i < c.size(); ++i)
            if (c.species[i] == 0 || c.species[i] == split->id)
                CHECK((c.genome[i].thermal_tolerance > tol) == (c.species[i] == split->id));
        std::size_t on = 0;
        for (const Species& s : all) on += s.population;
        CHECK(on == c.size());                           // the books still add up
    }
    bool logged = false, alerted = false;
    for (const auto& e : two.chronicle().entries()) logged |= e.text.find("splits in two") != std::string::npos;
    for (const auto& e : two.alerts().entries()) alerted |= e.text.find("splits in two") != std::string::npos;
    CHECK(logged && alerted);

    // All alike: no split.
    Simulation one(small_config());
    run(false, false, one);
    for (const Species& s : one.species().all()) CHECK(!s.by_split);
    CHECK(one.alerts().entries().empty());              // nothing followed, nothing reported
}

// Following a species: alerts when most of it gains a part, when it grows in
// number, and none once it is no longer followed.
void test_follow_species() {
    using namespace luca;
    Simulation sim(small_config());
    const Genome plain = make_luca();
    Genome eyed = plain;
    eyed.parts.push_back({PartType::Sensor, 0.5f, 0.0f, 0.0f});
    CHECK(genome_distance(plain, eyed) < SPECIES_THRESHOLD);
    for (int k = 0; k < 20; ++k) sim.add_creature(4.0f + k * 2.0f, 8.0f, 300.0f, plain);
    sim.follow(0);
    CHECK(sim.following(0) && sim.followed().size() == 1);
    for (int k = 0; k < 40; ++k) sim.add_creature(4.0f + (k % 20) * 2.0f, 20.0f + (k / 20) * 4.0f, 300.0f, eyed);
    CHECK(sim.creatures().species.back() == 0);
    for (int t = 0; t < 200; ++t) sim.step();
    bool eyes = false, thriving = false;
    for (const auto& e : sim.alerts().entries()) {
        eyes |= e.text.find("now have eyes") != std::string::npos;
        thriving |= e.text.find("thriving") != std::string::npos;
    }
    CHECK(eyes && thriving);
    const std::size_t before = sim.alerts().entries().size();
    sim.follow(0, false);
    CHECK(!sim.following(0));
    for (int t = 0; t < 400; ++t) sim.step();
    CHECK(sim.alerts().entries().size() == before);
}

// The CTRNN brain: a neuron with tau 1 and no bias is the old instant
// neuron; a slow one creeps towards its input and holds on to it after; time
// constants cost upkeep and stay in range under mutation; removing a neuron
// keeps the others' genes with them.
// Touch: a body being drained feels it, and the one draining it feels it is
// eating; a body nobody touches feels neither. A brain wired to "hurt" can
// use it: a prey that only moves when drained stays put until something bites.
void test_touch() {
    using namespace luca;
    SimConfig cfg = small_config();
    cfg.nutrient_mean = 0.0f;
    Simulation sim(cfg);
    const uint64_t hunter = sim.add_creature(24.2f, 6.5f, 100.0f, aim_east(sim, with_part(PartType::Absorber, 1.5f)));
    Genome skittish = make_luca();
    skittish.parts.push_back({PartType::Flagellum, 1.0f, 3.14159265f, 0.0f});
    skittish.brain.connect(input_node(IN_BIAS), output_node(OUT_THROTTLE), -3.0f);   // sits still...
    skittish.brain.connect(input_node(IN_HURT), output_node(OUT_THROTTLE), 6.0f);   // ...until something bites
    skittish.brain.connect(input_node(IN_HURT), output_node(OUT_WANDER), 3.0f);
    const uint64_t prey  = sim.add_creature(24.6f, 6.5f, 300.0f, skittish);
    const uint64_t calm  = sim.add_creature(40.5f, 6.5f, 300.0f, skittish);     // nobody near it
    const float calm_x = sim.creatures().x[find(sim, calm)];
    for (int t = 0; t < 3; ++t) sim.step();
    const Creatures& c = sim.creatures();
    // In the middle of a cell, so its first steps don't climb a cliff it
    // can't power (a step costs at most what its enzymes release in a tick).
    const std::size_t ih = find(sim, hunter), ip = find(sim, prey), ic = find(sim, calm);
    CHECK(ih < c.size() && ip < c.size() && ic < c.size());
    if (failures) return;
    CHECK(c.ate[ih] > 0.0f && c.hurt[ih] == 0.0f);
    CHECK(c.hurt[ip] > 0.0f && c.ate[ip] == 0.0f);
    CHECK(c.hurt[ic] == 0.0f && c.ate[ic] == 0.0f);
    CHECK(c.moved[ip] > 0.0f);            // bitten, it runs
    CHECK(c.x[ic] == calm_x);             // untouched, it stays
}

void test_ctrnn() {
    using namespace luca;
    auto one_neuron = [](float tau, float bias) {
        Genome g = make_luca();
        g.parts.push_back({PartType::Flagellum, 1.0f, 3.14159265f, 0.0f});
        g.brain.hidden = 1;
        g.brain.tau[0] = tau;
        g.brain.bias[0] = bias;
        g.brain.connect(input_node(IN_HUNGER), hidden_node(0), 1.5f);
        g.brain.connect(hidden_node(0), output_node(OUT_WANDER), 2.0f);
        clamp_genome(g);
        return g;
    };
    float in[NUM_INPUTS] = {}, out[NUM_OUTPUTS];
    in[IN_BIAS] = 1.0f;
    in[IN_HUNGER] = 0.8f;

    {   // Fast: the old rule, out = 2 tanh(1.5 x 0.8), at once.
        const BrainPlan plan = compile_brain(one_neuron(1.0f, 0.0f));
        BrainState st;
        think(plan, st, in, out);
        CHECK(std::abs(out[OUT_WANDER] - 2.0f * std::tanh(1.2f)) < 1e-6f);
    }
    {   // Bias shifts where it fires.
        const BrainPlan plan = compile_brain(one_neuron(1.0f, -1.2f));
        BrainState st;
        think(plan, st, in, out);
        CHECK(std::abs(out[OUT_WANDER]) < 1e-6f);
    }
    {   // Slow (tau 100): a hundredth of the way the first tick, about 63% after
        // 100 ticks, and after the input stops it is still mostly there 20 ticks on.
        const BrainPlan plan = compile_brain(one_neuron(100.0f, 0.0f));
        BrainState st;
        think(plan, st, in, out);
        CHECK(std::abs(st.hidden[0] - 0.012f) < 1e-5f);
        for (int t = 1; t < 100; ++t) think(plan, st, in, out);
        CHECK(std::abs(st.hidden[0] - 1.2f * (1.0f - std::pow(0.99f, 100.0f))) < 1e-3f);
        const float held = st.hidden[0];
        in[IN_HUNGER] = 0.0f;
        for (int t = 0; t < 20; ++t) think(plan, st, in, out);
        CHECK(st.hidden[0] > 0.8f * held && out[OUT_WANDER] > 0.5f);
        in[IN_HUNGER] = 0.8f;
    }
    {   // Memory costs: a slow neuron dearer than a fast one, by MEMORY_COST per e-fold.
        const float fast = compile_phenotype(one_neuron(1.0f, 0.0f)).upkeep;
        const float slow = compile_phenotype(one_neuron(1000.0f, 0.0f)).upkeep;
        const float biased = compile_phenotype(one_neuron(1.0f, 1.0f)).upkeep;
        CHECK(slow > fast + 0.0001f && slow < fast + 0.0002f);   // MEMORY_COST x ln 1000
        CHECK(biased > fast);
    }
    {   // Range, mutation and removal.
        Genome g = one_neuron(5000.0f, 9.0f);
        CHECK(g.brain.tau[0] == limits::MAX_TAU && g.brain.bias[0] == limits::MAX_WEIGHT);
        g.brain.hidden = 3;
        g.brain.tau[1] = 7.0f;  g.brain.bias[1] = 0.5f;
        g.brain.tau[2] = 50.0f; g.brain.bias[2] = -0.5f;
        g.brain.connect(hidden_node(2), output_node(OUT_LIGHT), 1.0f);
        g.brain.remove_hidden(1);
        CHECK(g.brain.hidden == 2 && g.brain.tau[1] == 50.0f && g.brain.bias[1] == -0.5f && g.brain.tau[2] == 1.0f);
        bool linked = false;
        for (const BrainLink& l : g.brain.links) linked |= l.from == hidden_node(1) && l.to == output_node(OUT_LIGHT);
        CHECK(linked);   // the link followed its neuron down one place
        Rng rng(5);
        bool moved = false;
        for (int k = 0; k < 3000; ++k) {
            g = mutate(g, rng);
            for (int h = 0; h < g.brain.hidden; ++h) {
                CHECK(g.brain.tau[h] >= limits::MIN_TAU && g.brain.tau[h] <= limits::MAX_TAU);
                moved |= g.brain.tau[h] > 1.0f && g.brain.tau[h] < limits::MAX_TAU;
            }
        }
        CHECK(moved);
    }
}

void test_growing_up() {
    using namespace luca;
    // A young body is the adult scaled down by mass.
    const Phenotype adult = compile_phenotype(make_luca()), half = compile_phenotype(make_luca(), 0.5f);
    CHECK(make_luca().birth_size == 0.5f && adult.grown == 1.0f && half.grown == 0.5f);
    CHECK(std::abs(half.mass - 0.5f * adult.mass) < 1e-5f);
    CHECK(std::abs(half.pool_capacity - 0.5f * adult.pool_capacity) < 1e-4f);
    CHECK(std::abs(half.energy_capacity - 0.5f * adult.energy_capacity) < 1e-2f);
    CHECK(std::abs(half.radius - std::cbrt(0.5f) * adult.radius) < 1e-5f);
    CHECK(half.upkeep < adult.upkeep && half.light_capture < adult.light_capture);
    CHECK(half.heat_follow > adult.heat_follow);   // small bodies lose heat faster
    CHECK(flow(compile_phenotype(with_part(PartType::Absorber, 1.0f)), half) >
          flow(compile_phenotype(with_part(PartType::Absorber, 1.0f)), adult));   // and are easier to drain

    SimConfig cfg = small_config();
    cfg.day_length = 0;
    cfg.nutrient_patchiness = 0.0f;
    cfg.nutrient_mean = 20.0f;
    cfg.slope_shade = 0.0f;
    cfg.windfall_rate = 0.0f;
    cfg.wash_rate = 0.0f;   // so its cell's soil changes only by what it takes

    // A young LUCA with plenty to spend grows to full size, then breeds.
    // Its body is built of the chemicals in its store, which light refills
    // from its own cell's soil.
    Simulation sim(cfg);
    const uint64_t kid = sim.add_creature(20.5f, 20.5f, 300.0f, make_luca(), 0.5f);
    CHECK(sim.creatures().pheno[find(sim, kid)].grown == 0.5f);
    const double matter0 = sim.total_matter();
    const int cell = sim.world().cell_index(20.5f, 20.5f);
    const float soil0 = sim.soil().nutrient[cell];
    const float held0 = sim.creatures().pool[0].mass() + sim.creatures().tissue[0].mass();
    bool bred_young = false;
    float grown = 0.5f;
    double grew = 0.0, start = sim.stored_energy(), in = 0.0, out = 0.0;
    for (int t = 0; t < 40 && grown < 1.0f; ++t) {
        for (int k = 0; k < 50; ++k) {
            sim.step();
            grew += sim.last_stats().grown_mass;
            in += sim.last_stats().energy_in;
            out += sim.last_stats().energy_out;
        }
        const std::size_t i = find(sim, kid);
        CHECK(i < sim.creatures().size());
        if (i >= sim.creatures().size()) break;
        grown = sim.creatures().pheno[i].grown;
        if (grown < 1.0f && sim.creatures().size() > 1) bred_young = true;
    }
    CHECK(grown == 1.0f);
    CHECK(!bred_young);                                       // nothing is born before it is grown
    {
        const std::size_t i = find(sim, kid);
        // Whatever it holds now beyond what it started with came out of its cell.
        const float gained = sim.creatures().pool[i].mass() + sim.creatures().tissue[i].mass() - held0;
        CHECK(gained > 0.1f && std::abs(soil0 - sim.soil().nutrient[cell] - gained) < 1e-3f);
        // Its body is what its store held: D.
        CHECK(std::abs(sim.creatures().tissue[i].mass() - sim.creatures().pheno[i].mass) < 1e-4f);
        CHECK(sim.creatures().tissue[i].v[CHEM_D] > 0.99f * sim.creatures().tissue[i].mass());
    }
    CHECK(std::abs(grew - (adult.mass - half.mass)) < 1e-3);   // and is counted
    CHECK(std::abs(sim.creatures().pheno[find(sim, kid)].mass - adult.mass) < 1e-4f);
    CHECK(std::abs(sim.stored_energy() - (start + in - out)) < 1e-4 * sim.stored_energy());
    CHECK(std::abs(sim.total_matter() - matter0) < 1e-4 * matter0);

    // Its children are born at the size in their own genome.
    for (int t = 0; t < 4000 && sim.creatures().size() < 2; ++t) sim.step();
    const Creatures& c = sim.creatures();
    CHECK(c.size() >= 2);
    int newborns = 0;
    for (std::size_t i = 0; i < c.size(); ++i)
        if (c.generation[i] == 1 && c.age[i] < 4) {
            ++newborns;
            CHECK(std::abs(c.pheno[i].grown - c.genome[i].birth_size) < 1e-6f);
            CHECK(c.pheno[i].grown < 1.0f);
        }
    CHECK(newborns == 1);

    // No soil, nothing new to build with: on bare ground a young body can
    // only build itself out of what it carries, and its store never refills.
    SimConfig bare = cfg;
    bare.nutrient_mean = 0.0f;
    Simulation poor(bare);
    const uint64_t stunted = poor.add_creature(20.5f, 20.5f, 400.0f, make_luca(), 0.5f);
    const std::size_t s0 = find(poor, stunted);
    const float carried = poor.creatures().pool[s0].mass() + poor.creatures().tissue[s0].mass();
    for (int t = 0; t < 200; ++t) poor.step();
    const std::size_t si = find(poor, stunted);
    CHECK(si < poor.creatures().size());
    if (si < poor.creatures().size()) {
        CHECK(poor.creatures().pool[si].mass() + poor.creatures().tissue[si].mass() < carried);
    }
}

void test_vents() {
    using namespace luca;
    SimConfig cfg = small_config();
    cfg.sun_strength = 0.0f;      // a dark world: the vent is the only food
    // Soil to store the vent's energy in (A -> D); too little time to breed.
    cfg.nutrient_mean = 6.0f;
    cfg.nutrient_patchiness = 0.0f;
    cfg.windfall_rate = 0.0f;
    Simulation sim(cfg);
    const World& w = sim.world();
    const int cell = w.cell_index(20.5f, 20.5f);
    const float cold = w.mean_temperature(cell);
    CHECK(!w.any_vents() && w.vent_energy(cell) == 0.0f);

    PowerCommand open;
    open.kind = PowerKind::Vent;
    open.x = 20.5f; open.y = 20.5f; open.radius = 2.0f; open.amount = 1.0f;
    sim.cast(open);
    sim.flush_powers();
    CHECK(w.any_vents() && std::abs(w.vent(cell) - 1.0f) < 1e-4f);
    CHECK(std::abs(w.vent_energy(cell) - cfg.vent_power) < 1e-4f);
    CHECK(std::abs(w.mean_temperature(cell) - cold - cfg.vent_heat) < 1e-3f);   // vents are warm

    // A vent brings up loose C from below (chemistry stage 3): no gene, no
    // bodies needed. Energy and matter are both new; the soil is untouched.
    {
        const float soil_before = sim.soil().nutrient[cell], c_before = sim.soil().dissolved[cell].v[VENT_CHEM];
        const double matter = sim.total_matter(), brought = sim.matter_brought_up();
        sim.step();
        const float made = cfg.vent_power / LEVEL[VENT_CHEM];
        CHECK(std::abs(sim.soil().dissolved[cell].v[VENT_CHEM] - c_before - made) < 1e-5f);
        CHECK(sim.soil().nutrient[cell] == soil_before);
        CHECK(std::abs(sim.soil().vent_made[cell] - sim.soil().dissolved[cell].v[VENT_CHEM]) < 1e-6f);
        const double in = sim.last_stats().matter_in;
        CHECK(in >= made * 0.999 && std::abs(sim.matter_brought_up() - brought - in) < 1e-9);
        CHECK(std::abs(sim.total_matter() - matter - in) < 1e-5 * matter);   // the matter books: up by what came up
        CHECK(std::abs(sim.last_stats().in_vent - in * LEVEL[VENT_CHEM]) < 1e-3);
    }

    // Two bodies that can burn C live on it; a plain LUCA beside them can't,
    // and is fouled by it instead; one far from the vent gets nothing.
    Genome eater_g = make_luca();
    eater_g.enzymes.push_back(Enzyme{CHEM_A, CHEM_C, 0.015f});
    const uint64_t a = sim.add_creature(20.3f, 20.4f, 300.0f, eater_g);
    const uint64_t b = sim.add_creature(20.7f, 20.6f, 300.0f, eater_g);
    const uint64_t plain = sim.add_creature(20.5f, 20.2f, 300.0f, make_luca());
    const uint64_t far = sim.add_creature(40.5f, 40.5f, 300.0f, eater_g);   // no vent here
    double in_vent = 0.0, fouled = 0.0;
    for (int k = 0; k < 50; ++k) {
        sim.step();
        in_vent += sim.last_stats().in_vent;
        fouled += sim.last_stats().out_fouling;
    }
    const Creatures& c = sim.creatures();
    const std::size_t ia = find(sim, a), ib = find(sim, b), ip = find(sim, plain), iff = find(sim, far);
    CHECK(ia < c.size() && ib < c.size() && ip < c.size() && iff < c.size());
    if (failures) return;
    CHECK(c.fed_vent[ia] > 5.0f && std::abs(c.fed_vent[ia] - c.fed_vent[ib]) < 1e-3f * c.fed_vent[ia]);   // shared alike
    CHECK(c.fed_vent[ia] + c.fed_vent[ib] < in_vent);                                   // never more than the vents made
    CHECK(c.pool[ia].v[CHEM_C] > 0.0f);
    CHECK(c.fed_vent[ip] == 0.0f && c.fed_vent[iff] == 0.0f);
    CHECK(fouled > 0.0);
    CHECK(c.energy[ia] > c.energy[ip]);
    {
        const double kept = sim.total_matter() - sim.matter_brought_up();
        CHECK(run_audited(sim, 500) < 1e-4);
        CHECK(std::abs(sim.total_matter() - sim.matter_brought_up() - kept) < 1e-5 * kept);
    }

    // A vent works on bare ground: what it brings up needs nothing from the soil.
    {
        SimConfig bare = cfg;
        bare.nutrient_mean = 0.0f;
        Simulation dead(bare);
        dead.cast(open);
        dead.flush_powers();
        dead.step();
        CHECK(dead.world().any_vents() && dead.last_stats().in_vent >= cfg.vent_power * 0.999);
        CHECK(std::abs(dead.total_matter() - dead.matter_brought_up()) < 1e-6);
    }

    // Closing the vent takes it away.
    PowerCommand close = open;
    close.amount = -1.0f;
    sim.cast(close);
    sim.flush_powers();
    CHECK(!w.any_vents() && std::abs(w.mean_temperature(cell) - cold) < 1e-3f);

    // The script format round-trips.
    std::vector<PowerCommand> cmds;
    std::string error;
    std::istringstream in("100 vent 10 12 3 1.5 200\n");
    CHECK(parse_power_script(in, cmds, error) && cmds.size() == 1 && cmds[0].kind == PowerKind::Vent);
    std::ostringstream out;
    write_power_script(out, cmds);
    CHECK(out.str().find("100 vent 10 12 3 1.5 200") != std::string::npos);
}

// 2.9 B: loose product slows the enzyme that makes it; off at INHIBIT_K 0.
void test_product_inhibition() {
    using namespace luca;
    Genome g = make_luca();
    Enzyme e{CHEM_B, CHEM_D, 0.1f};   // D -> B (poorer side first)
    g.enzymes = {e};
    const Phenotype p = compile_phenotype(g);
    Mix loose; loose.v[CHEM_B] = 1.0f;
    auto run_once = [&](const Mix* l) {
        Mix pool; pool.v[CHEM_D] = 1.0f;
        float soil = 0.0f;
        burn(p, pool, soil, 1000.0f, nullptr, l);
        return 1.0f - pool.v[CHEM_D];   // units turned over
    };
    CHECK(std::abs(run_once(&loose) - 0.1f) < 1e-6f);            // off: full rate
    g_laws.INHIBIT_K = 1.0f;
    const float slowed = run_once(&loose);
    CHECK(std::abs(slowed - 0.1f / (1.0f + std::exp(-2.0f))) < 1e-5f);   // D -> B steps 148 = 2 scales: 1 loose B slows it to 1/1.135
    CHECK(std::abs(run_once(nullptr) - 0.1f) < 1e-6f);            // no cell given: full rate
    g_laws.INHIBIT_K = 0.0f;
}

// Chemistry stage 4: two-input reactions, X + Y <-> Z + W, one unit of each.
// They run as far as the scarcer input allows; one with equal levels on both
// sides never runs (neutral exchanges were deleted, 14.10.8).
void test_two_input_reactions() {
    using namespace luca;
    auto two = [](uint8_t x, uint8_t x2, uint8_t y, uint8_t y2, float rate) {
        Enzyme e{x, y, rate};
        e.x2 = x2;
        e.y2 = y2;
        return e;
    };

    // One way to write each reaction: sides sorted, the poorer side first.
    {
        const Enzyme e = canonical_enzyme(two(CHEM_B, CHEM_B, CHEM_D, CHEM_A, 0.01f));   // D + A (222) is richer than B + B (148)
        CHECK(e.x == CHEM_B && e.x2 == CHEM_B && e.y == CHEM_A && e.y2 == CHEM_D);
        const Enzyme n = canonical_enzyme(two(CHEM_D, CHEM_A, CHEM_C, CHEM_B, 0.01f));   // neutral: 222 both sides
        CHECK(n.x == CHEM_A && n.x2 == CHEM_D && n.y == CHEM_B && n.y2 == CHEM_C);
        const Enzyme same = canonical_enzyme(two(CHEM_B, CHEM_C, CHEM_C, CHEM_B, 0.01f));  // does nothing: mended
        CHECK(!(same.x == same.y && same.x2 == same.y2));
        Enzyme half = two(CHEM_A, CHEM_B, CHEM_D, NO_CHEM, 0.01f);                         // one slot only: one-to-one
        half = canonical_enzyme(half);
        CHECK(!half.two() && half.x == CHEM_A && half.y == CHEM_D);
        CHECK(enzyme_words(e) == "B+B-A+D" && enzyme_words(make_luca().enzymes[0]) == "A-D");
    }

    // Downhill D + A -> B + B: needs A from the soil as well as D, releases 74
    // a unit, and leaves the store a unit fuller (room permitting).
    {
        Genome g = make_luca();
        g.enzymes = {two(CHEM_D, CHEM_A, CHEM_B, CHEM_B, 0.01f)};
        const Phenotype p = compile_phenotype(g);
        CHECK(p.can_burn(CHEM_D) && !p.can_burn(CHEM_B) && p.any_two);
        Mix pool = pure(CHEM_D, 1.0f);
        float soil = 0.004f, flow = 0.0f;
        const float got = burn(p, pool, soil, 100.0f, &flow);
        CHECK(std::abs(got - 0.004f * 74.0f) < 1e-5f);            // as far as the soil's A goes
        CHECK(std::abs(pool.v[CHEM_B] - 0.008f) < 1e-6f && std::abs(pool.v[CHEM_D] - 0.996f) < 1e-6f && soil == 0.0f);
        CHECK(std::abs(flow - 0.004f) < 1e-6f);
        CHECK(std::abs(power(p, pure(CHEM_D, 1.0f), 1.0f) - 0.01f * 74.0f) < 1e-5f);   // capped by its rate
        CHECK(power(p, pure(CHEM_D, 1.0f), 0.0f) == 0.0f);                              // and by the soil
    }

    // Uphill A + A -> B + B on light: takes two A a unit from the soil.
    {
        Genome g = make_luca();
        g.enzymes = {two(CHEM_A, CHEM_A, CHEM_B, CHEM_B, 0.01f)};
        const Phenotype p = compile_phenotype(g);
        Mix pool;
        float soil = 1.0f;
        const float stored = charge(p, pool, soil, 0.74f);
        CHECK(std::abs(stored - 0.74f) < 1e-5f && std::abs(pool.v[CHEM_B] - 0.01f) < 1e-6f && std::abs(soil - 0.99f) < 1e-6f);
    }

    // One with equal levels (B + C <-> A + D) never runs: B and C are not
    // food for it, and it still costs upkeep.
    {
        Genome g = make_luca();
        g.enzymes.push_back(two(CHEM_B, CHEM_C, CHEM_A, CHEM_D, 0.01f));
        const Phenotype p = compile_phenotype(g);
        CHECK(!p.can_burn(CHEM_B) && !p.can_burn(CHEM_C) && p.can_burn(CHEM_D));
        CHECK(p.upkeep > compile_phenotype(make_luca()).upkeep);
    }

    // Mutation: one-to-one enzymes gain a second pair, and every genome stays canonical.
    {
        Rng rng(5);
        Genome g = make_luca();
        bool gained = false;
        for (int k = 0; k < 4000; ++k) {
            g = mutate(g, rng);
            for (const Enzyme& e : g.enzymes) {
                gained = gained || e.two();
                const Enzyme c = canonical_enzyme(e);
                CHECK(c.x == e.x && c.y == e.y && c.x2 == e.x2 && c.y2 == e.y2);
            }
            if (failures) return;
            if (g.enzymes.empty()) g = make_luca();
        }
        CHECK(gained);
        Genome a = make_luca(), b = make_luca();
        b.enzymes.push_back(two(CHEM_B, CHEM_C, CHEM_A, CHEM_D, 0.015f));
        CHECK(genome_distance(a, b) > 0.5f);   // a new kind of reaction
    }

    // In a world: a body burning loose B and C together (B + C -> A + A)
    // beside a body that gives them off. Both books balance.
    SimConfig cfg = small_config();
    cfg.nutrient_mean = 0.0f;   // nobody breeds
    cfg.light_mode = LightMode::Uniform;
    Simulation sim(cfg);
    Genome giver_g = make_luca();
    giver_g.enzymes = {Enzyme{CHEM_B, CHEM_D, 0.01f}, Enzyme{CHEM_C, CHEM_D, 0.01f}};   // burns D to B and to C, gives both off
    sim.add_creature(20.3f, 20.5f, 400.0f, giver_g);
    Genome user_g = make_luca();
    // First in the genome, so it burns B + C before D (enzymes burn in order).
    user_g.enzymes.insert(user_g.enzymes.begin(), two(CHEM_A, CHEM_A, CHEM_B, CHEM_C, 0.01f));
    const uint64_t uid = sim.add_creature(20.7f, 20.5f, 100.0f, user_g);
    const double matter0 = sim.total_matter();
    double flow = 0.0;
    for (int t = 0; t < 400; ++t) {
        sim.step();
        flow += sim.last_stats().two_flow;
    }
    const std::size_t iu = find(sim, uid);
    CHECK(iu < sim.creatures().size());
    if (failures) return;
    CHECK(flow > 0.0 && sim.creatures().fed_waste[iu] > 0.0f);
    CHECK(std::abs(sim.total_matter() - matter0) / matter0 < 1e-5);
    CHECK(run_audited(sim, 500) < 1e-4);
}

// Forks and knockouts (design doc 14.6.3): the same fork seed gives the same
// future, another gives another; a knockout removes a species and both books close.
void test_fork_and_knockout() {
    using namespace luca;
    SimConfig cfg;
    cfg.world_size = 48;
    cfg.initial_population = 150;
    Simulation base(cfg);
    base.seed_population();
    for (int t = 0; t < 2000; ++t) base.step();

    Simulation a = base, b = base, c = base, same = base;
    a.fork(1);
    b.fork(1);
    c.fork(2);
    for (int t = 0; t < 600; ++t) { a.step(); b.step(); c.step(); same.step(); }
    CHECK(snapshot_bytes(a) == snapshot_bytes(b));
    CHECK(snapshot_bytes(a) != snapshot_bytes(c));
    CHECK(snapshot_bytes(a) != snapshot_bytes(same));

    // Knock out the most numerous species.
    std::map<uint32_t, std::size_t> count;
    for (std::size_t i = 0; i < a.creatures().size(); ++i) ++count[a.creatures().species[i]];
    uint32_t top = 0;
    std::size_t most = 0;
    for (const auto& [sp, n] : count) if (n > most) { most = n; top = sp; }
    const double energy = a.stored_energy(), matter = a.total_matter();
    const std::size_t before = a.creatures().size();
    CHECK(a.knockout(top) == most);
    CHECK(a.creatures().size() == before - most);
    for (std::size_t i = 0; i < a.creatures().size(); ++i) CHECK(a.creatures().species[i] != top);
    CHECK(std::abs(a.stored_energy() - energy) < 1e-6 * energy);
    CHECK(std::abs(a.total_matter() - matter) < 1e-6 * matter);
    CHECK(a.species().get(top).population == 0);
    CHECK(a.knockout(top) == 0);
    for (int t = 0; t < 200; ++t) a.step();   // and the world goes on
}

// Scenarios (scenario.hpp): a goal is met from the census, time running out
// loses it, and a saved world keeps its scenario.
// Step 2's laws (CLAUDE.md 14.2), each off by default.
void test_geometry_laws() {
    using namespace luca;
    SimConfig cfg = small_config();
    cfg.day_length = 0;                  // steady light
    cfg.light_mode = LightMode::Uniform;
    cfg.windfall_rate = 0.0f;
    auto audit = [](Simulation& sim, int ticks, double& worst) {
        for (int t = 0; t < ticks; ++t) {
            const double before = sim.stored_energy();
            sim.step();
            const TickStats& st = sim.last_stats();
            const double after = sim.stored_energy();
            worst = std::max(worst, std::abs(after - (before + st.energy_in - st.energy_out)) / std::max(1.0, after));
        }
    };

    // DEAD_IN_REACH: a mouth pulls on a corpse in the next cell.
    auto dead_eaten = [&](float law, double& worst) {
        g_laws = legacy_laws();
        g_laws.DEAD_IN_REACH = law;
        Simulation sim(cfg);
        sim.add_creature(10.5f, 10.5f, 0.0f, with_part(PartType::Storage, 1.0f));   // dies at once: a corpse
        sim.step();
        sim.add_creature(11.3f, 10.5f, 300.0f, with_part(PartType::Absorber, 3.5f));   // the next cell east; a mouth all round
        const double m0 = sim.total_matter();
        double eaten = 0.0;
        for (int t = 0; t < 100; ++t) {
            audit(sim, 1, worst);
            eaten += sim.last_stats().eaten_dead;
        }
        CHECK(std::abs(sim.total_matter() - m0) / m0 < 1e-5);
        return eaten;
    };
    double worst = 0.0;
    CHECK(dead_eaten(0.0f, worst) == 0.0);
    CHECK(dead_eaten(1.0f, worst) > 0.0);

    // SPILL_LOST 0: the lost share of a meal is burned at the mouth, not spilled.
    auto hunt = [&](float law, double& transfer, double& given) {
        g_laws = legacy_laws();
        g_laws.SPILL_LOST = law;
        Simulation sim(cfg);
        sim.add_creature(20.5f, 20.5f, 400.0f, make_luca());
        sim.add_creature(20.9f, 20.5f, 100.0f, with_part(PartType::Absorber, 3.5f));
        transfer = given = 0.0;
        for (int t = 0; t < 60; ++t) {
            audit(sim, 1, worst);
            transfer += sim.last_stats().out_transfer;
            given += sim.last_stats().excreted;
        }
    };
    double tr1, gv1, tr0, gv0;
    hunt(1.0f, tr1, gv1);
    hunt(0.0f, tr0, gv0);
    CHECK(tr1 == 0.0 && gv1 > 0.0);
    CHECK(tr0 > 0.0 && gv0 < gv1);

    // LIGHT_ACROSS: the light gradient runs east to west instead of with warmth.
    SimConfig grad = cfg;
    grad.light_mode = LightMode::Gradient;
    grad.slope_shade = 0.0f;
    for (float law : {0.0f, 1.0f}) {
        g_laws = legacy_laws();
        g_laws.LIGHT_ACROSS = law;
        Simulation sim(grad);
        const World& w = sim.world();
        const float north = w.light(w.cell_index(24.5f, 1.5f)), south = w.light(w.cell_index(24.5f, 46.5f));
        const float west = w.light(w.cell_index(1.5f, 24.5f)), east = w.light(w.cell_index(46.5f, 24.5f));
        if (law == 0.0f) CHECK(north > 1.5f * south && std::abs(west - east) < 1e-5f);
        else CHECK(west > 1.5f * east && std::abs(north - south) < 1e-5f);
    }

    // LIGHT_FOOTPRINT: a big body catches light over the cells it covers, and
    // shades the small body beside it.
    Genome big = make_luca();
    big.core_radius = 2.0f;
    for (Part& part : big.parts)
        if (part.type == PartType::Photo) part.size = 12.0f;
    big.enzymes = {Enzyme{CHEM_A, CHEM_D, 0.2f}};
    auto light_in = [&](float law, float& small_gain) {
        g_laws = legacy_laws();
        g_laws.LIGHT_FOOTPRINT = law;
        Simulation sim(cfg);
        const uint64_t b = sim.add_creature(24.5f, 24.5f, 100.0f, big);
        const uint64_t s = sim.add_creature(25.5f, 24.5f, 100.0f, make_luca());
        double in = 0.0;
        for (int t = 0; t < 5; ++t) audit(sim, 1, worst);
        const std::size_t ib = find(sim, b), is = find(sim, s);
        small_gain = is < sim.creatures().size() ? sim.creatures().fed_light[is] : -1.0f;
        in = ib < sim.creatures().size() ? sim.creatures().fed_light[ib] : -1.0;
        return in / 5.0;
    };
    float small_off = 0.0f, small_on = 0.0f;
    const double big_off = light_in(0.0f, small_off), big_on = light_in(1.0f, small_on);
    g_laws = legacy_laws();
    const Simulation plain(cfg);
    const float cell_light = plain.world().light(plain.world().cell_index(24.5f, 24.5f));
    CHECK(big_off <= cell_light * 1.0001);   // one cell's light at most
    CHECK(big_on > 3.0 * cell_light);        // the cells it covers
    CHECK(small_on < small_off);             // in the big body's shade
    CHECK(Footprints::of(0.4f).ring == 0 && Footprints::of(1.0f).ring == 1);
    CHECK(std::abs(Footprints::of(1.0f).w[0][0] + 4 * Footprints::of(1.0f).w[1][0] + 4 * Footprints::of(1.0f).w[1][1] - 1.0f) < 1e-5f);

    CHECK(worst < 1e-5);
    g_laws = legacy_laws();
}

void test_food_web() {
    using namespace luca;
    SimConfig cfg = small_config();
    cfg.day_length = 0;
    cfg.windfall_rate = 0.0f;
    Simulation sim(cfg);
    // Plants, and a mouth on legs among them (far enough from LUCA to be a species of its own).
    for (int k = 0; k < 6; ++k) sim.add_creature(20.5f + k, 20.5f, 400.0f, make_luca());
    Genome hunter = with_part(PartType::Absorber, 3.5f);
    hunter.core_radius = 1.6f;
    hunter.bite = 1.0f;
    hunter.parts.push_back({PartType::Armour, 1.0f, 3.14159265f, 0.0f});
    hunter.parts.push_back({PartType::Poison, 1.0f, 1.0f, 0.0f});
    for (int k = 0; k < 3; ++k) sim.add_creature(20.9f + 2 * k, 21.0f, 600.0f, hunter);
    for (int t = 0; t < 120; ++t) sim.step();
    const FoodWeb web = food_web(sim, 1);
    CHECK(!web.nodes.empty() && !web.edges.empty());
    const std::size_t i = find(sim, sim.creatures().id[0]);
    const uint32_t plant = sim.creatures().species[i];
    uint32_t eater = plant;
    for (std::size_t k = 0; k < sim.creatures().size(); ++k)
        if (sim.creatures().pheno[k].absorb > 0.0f) eater = sim.creatures().species[k];
    bool sun_to_plant = false, plant_to_eater = false;
    for (const FoodWebEdge& e : web.edges) {
        sun_to_plant = sun_to_plant || (e.kind == FoodKind::Source && e.from == static_cast<uint32_t>(FoodSource::Sun) && e.to == plant);
        plant_to_eater = plant_to_eater || (e.kind == FoodKind::Living && e.from == plant && e.to == eater);
        CHECK(e.energy > 0.0);
    }
    CHECK(sun_to_plant && plant_to_eater);
    double plant_level = 0.0, eater_level = 0.0;
    for (const FoodWebNode& n : web.nodes) {
        if (n.species == plant) plant_level = n.level;
        if (n.species == eater) eater_level = n.level;
    }
    CHECK(eater != plant && plant_level >= 1.0 && plant_level < 1.2 && eater_level > 1.5);
}

void test_scenario() {
    using namespace luca;
    const int first = find_scenario("first-steps");
    CHECK(first > 0 && find_scenario("no-such") == -1);
    SimConfig cfg;
    cfg.world_size = 32;
    cfg.initial_population = 60;

    // No movers can appear in 10,000 ticks from 60 LUCAs here (tested
    // below): time runs out, and the chronicle says so.
    Simulation lose(cfg);
    lose.seed_population();
    lose.start_scenario(static_cast<uint32_t>(first));
    CHECK(lose.scenario().active() && lose.scenario().outcome == Outcome::Running);
    for (int t = 0; t < 5000; ++t) lose.step();
    CHECK(lose.scenario().outcome == Outcome::Running);
    std::string err;
    auto saved = snapshot_from_bytes(snapshot_bytes(lose), err);   // the scenario survives a save
    CHECK(saved && saved->scenario().id == static_cast<uint32_t>(first) && saved->scenario().start_tick == 0);
    for (int t = 0; t < 5050; ++t) lose.step();
    if (lose.creatures().size() > 0 && lose.census().movers < 10) {
        CHECK(lose.scenario().outcome == Outcome::Lost);
        CHECK(lose.scenario().verdict.find("Time is up") == 0);
        CHECK(std::any_of(lose.chronicle().entries().begin(), lose.chronicle().entries().end(),
                          [&](const ChronicleEntry& e) { return e.text == lose.scenario().verdict; }));
    }

    // Ten swimmers with brains: the goal is met at the next census.
    Simulation win(cfg);
    win.seed_population();
    win.start_scenario(static_cast<uint32_t>(first));
    Genome swimmer = make_luca();
    swimmer.parts.push_back({PartType::Flagellum, 1.0f, 3.14159265f, 0.0f});
    swimmer.brain.connect(input_node(IN_BIAS), output_node(OUT_WANDER), 1.0f);
    for (int k = 0; k < 12; ++k) win.add_creature(4.0f + k * 2.0f, 16.0f, 400.0f, swimmer);
    for (int t = 0; t < 60; ++t) win.step();
    CHECK(win.scenario().outcome == Outcome::Won);
    CHECK(win.scenario().end_tick == 50);
    CHECK(win.scenario().verdict.find("Goal met") == 0);
}

// Stoichiometry (docs/design/stoichiometry.md): off changes nothing a body
// needs; on, a body with energy but no P cannot breed; eaters give P off;
// the P book is exact; a world with P saves and loads exactly.
void test_stoichiometry() {
    using namespace luca;
    SimConfig cfg = small_config();
    cfg.day_length = 0;
    cfg.light_mode = LightMode::Uniform;
    cfg.windfall_rate = 0.0f;

    g_laws = legacy_laws();
    CHECK(compile_phenotype(make_luca()).p_need == 0.0f);   // off by default
    g_laws.P_BASE = 0.05f;
    const Phenotype luca_p = compile_phenotype(make_luca());
    CHECK(luca_p.p_need > 0.0f && luca_p.p_store_cap > luca_p.p_need);
    CHECK(compile_phenotype(with_part(PartType::Armour, 1.0f)).p_need > compile_phenotype(with_part(PartType::Storage, 1.0f)).p_need);
    CHECK(std::abs(compile_phenotype(make_luca(), 0.5f).p_need - 0.5f * luca_p.p_need) < 1e-6f);

    // Energy enough to breed; soil with no P, or plenty.
    auto breed = [&](float p_mean, std::size_t& births, std::size_t& held) {
        g_laws = legacy_laws();
        g_laws.P_BASE = 0.05f;
        g_laws.P_MEAN = p_mean;
        Simulation sim(cfg);
        sim.add_creature(20.5f, 20.5f, 640.0f, make_luca());
        const double p0 = sim.total_p();
        births = held = 0;
        for (int t = 0; t < 64; ++t) {
            sim.step();
            births += sim.last_stats().births;
            held += sim.last_stats().p_limited;
        }
        CHECK(std::abs(sim.total_p() - p0) < 1e-5 * std::max(1.0, p0));
    };
    std::size_t b0, h0, b1, h1;
    breed(0.0f, b0, h0);
    breed(5.0f, b1, h1);
    CHECK(b0 == 0 && h0 > 0);
    CHECK(b1 > 0);

    // An eater in rich soil (its store full of P) gives off the P in its meals.
    g_laws = legacy_laws();
    g_laws.P_BASE = 0.05f;
    g_laws.P_MEAN = 5.0f;
    {
        Simulation sim(cfg);
        sim.add_creature(20.5f, 20.5f, 400.0f, make_luca());
        sim.add_creature(20.9f, 20.5f, 100.0f, with_part(PartType::Absorber, 3.5f));
        const double p0 = sim.total_p();
        double given = 0.0, taken = 0.0;
        for (int t = 0; t < 120; ++t) {
            sim.step();
            given += sim.last_stats().p_given_off;
            taken += sim.last_stats().p_taken_up;
        }
        CHECK(taken > 0.0 && given > 0.0);
        CHECK(std::abs(sim.total_p() - p0) < 1e-5 * p0);

        // Saved and loaded with P, it goes on exactly as before.
        std::string err;
        auto b = snapshot_from_bytes(snapshot_bytes(sim), err);
        CHECK(b != nullptr);
        if (b) {
            for (int t = 0; t < 50; ++t) { sim.step(); b->step(); }
            CHECK(b->total_p() == sim.total_p());
            CHECK(snapshot_bytes(*b) == snapshot_bytes(sim));
        }
    }
    g_laws = legacy_laws();
}

int main() {
    // Most of these tests wire the old brain (the lean, wander and throttle outputs), which
    // is what SENSOR_BRAIN 0 runs. The sensor brain, now the default, has tests of its own
    // (test_sensor_brain, test_mouth_gate, test_sleep_drive), which set the law themselves.
    luca::g_laws.SENSOR_BRAIN = 0.0f;
    test_day_and_night();
    test_cold_nights();
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
    test_wander_turns_by_distance();
    test_dormancy();
    test_poison();
    test_loose_chemicals();
    test_gene_transfer();
    test_species();
    test_part_placement();
    test_body_shape();
    test_stalks();
    test_folding();
    test_species_split();
    test_follow_species();
    test_ctrnn();
    test_touch();
    test_growing_up();
    test_vents();
    test_windfalls();
    test_two_input_reactions();
    test_sensor_brain();
    test_mouth_gate();
    test_sleep_drive();
    test_product_inhibition();
    test_creature_files_and_meteors();
    test_snapshot_round_trip();
    test_laws();
    test_fork_and_knockout();
    test_scenario();
    test_keyed_draws();
    test_geometry_laws();
    test_food_web();
    test_stoichiometry();
    if (failures == 0) std::puts("all life tests passed");
    return failures == 0 ? 0 : 1;
}
