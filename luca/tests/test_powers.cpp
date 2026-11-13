// Tests for the god-power layer: brushes, ramps, the script format, and the
// two promises powers must keep (determinism, and never breaking the energy
// audit). No test framework; a failed CHECK prints the line and the run
// exits non-zero.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <vector>

#include "powers.hpp"
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

bool near(double a, double b, double eps = 1e-3) { return std::abs(a - b) <= eps; }

luca::SimConfig small_config() {
    luca::SimConfig cfg;
    cfg.world_size = 64;
    cfg.initial_population = 100;
    return cfg;
}

void test_brush() {
    const luca::SimConfig cfg = small_config();
    luca::World w;
    w.generate(cfg);
    const int centre = w.cell_index(32.5f, 32.5f);
    const int far    = w.cell_index(50.5f, 32.5f);  // 18 away, outside radius 10
    const float h0 = w.height(centre), t0 = w.temperature(centre);
    const float hf = w.height(far), tf = w.temperature(far);

    w.brush(luca::BrushField::Height, 32.5f, 32.5f, 10.0f, 12.0f);
    CHECK(near(w.height(centre), h0 + 12.0f));
    CHECK(near(w.temperature(centre), t0 - cfg.temp_lapse * 12.0f));  // higher = colder
    CHECK(w.height(far) == hf);
    CHECK(w.temperature(far) == tf);

    // Terrain stays inside [0, height_limit] however hard you push.
    w.brush(luca::BrushField::Height, 32.5f, 32.5f, 10.0f, 1e6f);
    CHECK(w.height(centre) == cfg.height_limit);
    w.brush(luca::BrushField::Height, 32.5f, 32.5f, 10.0f, -1e7f);
    CHECK(w.height(centre) == 0.0f);

    // Heat changes temperature but not the land.
    const float t1 = w.temperature(far);
    w.brush(luca::BrushField::Heat, 50.5f, 32.5f, 5.0f, 8.0f);
    CHECK(near(w.temperature(far), t1 + 8.0f));
    CHECK(w.height(far) == hf);

    // A brush hanging off the edge of the map is fine.
    w.brush(luca::BrushField::Height, -3.0f, 70.0f, 10.0f, 5.0f);
}

void test_globals() {
    const luca::SimConfig cfg = small_config();
    luca::World w;
    w.generate(cfg);
    const int c = w.cell_index(10.0f, 10.0f);
    const float l0 = w.light(c), t0 = w.temperature(c);

    w.add_sun(-0.5f);
    CHECK(near(w.light(c), l0 * 0.5f));
    w.add_sun(-2.0f);  // past zero: darkness, never negative light
    CHECK(w.light(c) == 0.0f);
    w.add_sun(2.5f);   // and the deltas still add back up
    CHECK(near(w.light(c), l0));

    w.add_climate(-15.0f);
    CHECK(near(w.temperature(c), t0 - 15.0f));
}

// A ramp must end in the same world as the same power cast instantly.
void test_ramp_matches_instant() {
    const luca::SimConfig cfg = small_config();
    luca::World a, b;
    a.generate(cfg);
    b.generate(cfg);
    luca::Chronicle ca, cb;
    luca::PowerSystem pa, pb;

    const luca::PowerCommand cmds[] = {
        {5, luca::PowerKind::Terrain, 20.0f, 40.0f, 12.0f, 15.0f, 0},
        {5, luca::PowerKind::Heat, 45.0f, 10.0f, 6.0f, -9.0f, 0},
        {7, luca::PowerKind::Sun, 0, 0, 0, -0.4f, 0},
        {9, luca::PowerKind::Climate, 0, 0, 0, -12.0f, 0},
    };
    for (luca::PowerCommand c : cmds) {
        pa.cast(c);
        c.duration = 333;  // deliberately not a multiple of the ramp step
        pb.cast(c);
    }
    for (uint64_t t = 0; t < 400; ++t) {
        pa.update(t, a, ca);
        pb.update(t, b, cb);
    }
    CHECK(pa.idle() && pb.idle());

    double worst_h = 0, worst_t = 0, worst_l = 0;
    for (int i = 0; i < a.num_cells(); ++i) {
        worst_h = std::max(worst_h, static_cast<double>(std::abs(a.height(i) - b.height(i))));
        worst_t = std::max(worst_t, static_cast<double>(std::abs(a.temperature(i) - b.temperature(i))));
        worst_l = std::max(worst_l, static_cast<double>(std::abs(a.light(i) - b.light(i))));
    }
    CHECK(worst_h < 1e-3);
    CHECK(worst_t < 1e-3);
    CHECK(worst_l < 1e-4);

    // Chronicle: 4 "begins" entries each; the ramped run also gets 4 endings.
    CHECK(ca.entries().size() == 4);
    CHECK(cb.entries().size() == 8);
    CHECK(cb.entries().front().tick == 5);

    // Half-way through a ramp, half the change is in.
    luca::World h;
    h.generate(cfg);
    luca::PowerSystem ph;
    luca::Chronicle ch;
    ph.cast({0, luca::PowerKind::Climate, 0, 0, 0, -10.0f, 1000});
    for (uint64_t t = 0; t < 500; ++t) ph.update(t, h, ch);
    CHECK(near(h.climate_offset(), -5.0f, 0.1));
}

void test_script() {
    std::istringstream in(
        "# a comment\n"
        "\n"
        "1000 terrain 32 32 10 12.5 2000   # raise a mountain\n"
        "500 sun -0.4\n"
        "3000 climate -15 4000\n"
        "0 heat 5 6 7 8\n"
        "10 terrain 1 2 3 4 quiet\n"
        "11 sun 0.1 50 quiet\n");
    std::vector<luca::PowerCommand> cmds;
    std::string err;
    CHECK(luca::parse_power_script(in, cmds, err));
    CHECK(cmds.size() == 6);
    CHECK(!cmds[0].quiet && cmds[4].quiet && cmds[4].duration == 0);
    CHECK(cmds[5].quiet && cmds[5].duration == 50);
    CHECK(cmds[0].kind == luca::PowerKind::Terrain && cmds[0].tick == 1000 && cmds[0].duration == 2000);
    CHECK(cmds[0].amount == 12.5f && cmds[0].radius == 10.0f);
    CHECK(cmds[1].kind == luca::PowerKind::Sun && cmds[1].amount == -0.4f && cmds[1].duration == 0);
    CHECK(cmds[3].kind == luca::PowerKind::Heat && cmds[3].x == 5.0f && cmds[3].amount == 8.0f);

    // Write -> parse gives back exactly the same commands.
    std::ostringstream out;
    luca::write_power_script(out, cmds);
    std::istringstream again(out.str());
    std::vector<luca::PowerCommand> back;
    CHECK(luca::parse_power_script(again, back, err));
    CHECK(back.size() == cmds.size());
    for (std::size_t i = 0; i < back.size() && i < cmds.size(); ++i) {
        CHECK(back[i].tick == cmds[i].tick && back[i].kind == cmds[i].kind);
        CHECK(back[i].x == cmds[i].x && back[i].y == cmds[i].y && back[i].radius == cmds[i].radius);
        CHECK(back[i].amount == cmds[i].amount && back[i].duration == cmds[i].duration);
        CHECK(back[i].quiet == cmds[i].quiet);
    }

    // Bad lines are rejected with a line number, not silently skipped.
    const char* bad[] = {
        "10 volcano 1 2 3 4\n", "x sun 1\n",           "-5 sun 1\n",          "10 sun\n",
        "10 terrain 1 2 3\n",   "10 terrain 1 2 0 4\n", "10 sun 1 -5\n",       "10 sun 1 2 3\n",
        "10 sun nan\n",         "10 sun 1 soon\n",    "10 sun 1 quiet 5\n",  "10 sun 1 quiet quiet\n",
    };
    for (const char* line : bad) {
        std::istringstream b(std::string("0 sun 1\n") + line);
        std::vector<luca::PowerCommand> c;
        std::string e;
        const bool ok = luca::parse_power_script(b, c, e);
        if (ok) std::printf("accepted bad line: %s", line);
        CHECK(!ok);
        CHECK(e.rfind("line 2:", 0) == 0);
    }
}

struct RunResult {
    std::size_t pop = 0;
    double stored = 0.0, worst_audit = 0.0;
    std::size_t chronicle = 0;
    float sample_height = 0.0f;
};

RunResult run(bool with_powers) {
    luca::Simulation sim(small_config());
    sim.seed_population();
    if (with_powers) {
        sim.cast({300, luca::PowerKind::Sun, 0, 0, 0, -0.7f, 400});
        sim.cast({300, luca::PowerKind::Terrain, 30.0f, 30.0f, 15.0f, 20.0f, 500});
        sim.cast({900, luca::PowerKind::Sun, 0, 0, 0, 0.7f, 0});
        sim.cast({1000, luca::PowerKind::Climate, 0, 0, 0, -12.0f, 300});
    }
    RunResult r;
    double start = sim.stored_energy(), in = 0, out = 0;
    for (int t = 0; t < 1500; ++t) {
        sim.step();
        in += sim.last_stats().energy_in;
        out += sim.last_stats().energy_out;
        if ((t + 1) % 100 == 0) {
            const double stored = sim.stored_energy();
            const double err = (stored - (start + in - out)) / std::max(1.0, std::abs(stored));
            r.worst_audit = std::max(r.worst_audit, std::abs(err));
            start = stored;
            in = out = 0;
        }
    }
    r.pop = sim.creatures().size();
    r.stored = sim.stored_energy();
    r.chronicle = sim.chronicle().entries().size();
    r.sample_height = sim.world().height(sim.world().cell_index(30.0f, 30.0f));
    return r;
}

void test_sim() {
    const RunResult plain = run(false);
    const RunResult a = run(true);
    const RunResult b = run(true);

    // Determinism: same seed + same commands = the same world, bit for bit.
    CHECK(a.pop == b.pop);
    CHECK(a.stored == b.stored);
    CHECK(a.chronicle == b.chronicle);

    // Powers changed the world and left a history...
    CHECK(a.sample_height != plain.sample_height);
    CHECK(a.chronicle > plain.chronicle);
    // ...without creating or destroying a drop of energy.
    CHECK(plain.worst_audit < 1e-4);
    CHECK(a.worst_audit < 1e-4);
}

}  // namespace

// The world acting on its own: off means nothing happens; each kind of event
// leaves what it should (a hill, a crater, a drifted climate) and takes back
// what it should (a storm's cold, a volcano's heat and vent, an impact's dust);
// and a run with events replays exactly from its power log with events off.
void test_world_events() {
    using namespace luca;
    SimConfig cfg = small_config();
    cfg.initial_population = 0;   // only the ground matters here

    auto snapshot = [](const World& w, std::vector<float>& h, std::vector<float>& t) {
        h.clear(); t.clear();
        for (int c = 0; c < w.num_cells(); ++c) { h.push_back(w.height(c)); t.push_back(w.mean_temperature(c)); }
    };
    auto worst = [](const std::vector<float>& a, const std::vector<float>& b) {
        double d = 0.0;
        for (std::size_t i = 0; i < a.size(); ++i) d = std::max(d, static_cast<double>(std::abs(a[i] - b[i])));
        return d;
    };
    std::vector<float> h0, t0, h1, t1;

    {   // Off by default: nothing is cast.
        Simulation sim(cfg);
        for (int t = 0; t < 20000; ++t) sim.step();
        CHECK(sim.powers().log().empty());
    }
    {   // A storm comes and goes, leaving the ground as it was.
        Simulation sim(cfg);
        snapshot(sim.world(), h0, t0);
        sim.trigger_event(EventKind::Storm);
        sim.step();
        for (int t = 0; t < 100; ++t) sim.step();
        snapshot(sim.world(), h1, t1);
        CHECK(worst(t0, t1) > 1.0);                      // it is cold under the storm
        for (int t = 0; t < 3100; ++t) sim.step();
        snapshot(sim.world(), h1, t1);
        CHECK(worst(t0, t1) < 1e-3 && worst(h0, h1) == 0.0);
        bool came = false, went = false;
        for (const auto& e : sim.chronicle().entries()) {
            came |= e.text.find("storm gathers") != std::string::npos;
            went |= e.text.find("storm over") != std::string::npos;
        }
        CHECK(came && went && sim.events().count(EventKind::Storm) == 1);
    }
    // Raise the sea over every hill a volcano could make (vents open only under water).
    auto flood = [](Simulation& sim) {
        float top = 0.0f;
        for (int c = 0; c < sim.world().num_cells(); ++c) top = std::max(top, sim.world().height(c));
        PowerCommand sea;
        sea.kind = PowerKind::Sea;
        sea.amount = top + 30.0f;
        sim.cast(sea);
        sim.flush_powers();
    };
    {   // A volcano on land is hot rock and a hill, with no vent.
        Simulation sim(cfg);
        sim.trigger_event(EventKind::Eruption);
        for (int t = 0; t < 400; ++t) sim.step();
        CHECK(!sim.world().any_vents());
    }
    {   // Sea vents need a sea: with none, nothing opens; with one, a vent opens under water and closes again.
        Simulation dry(cfg);
        dry.trigger_event(EventKind::SeaVent);
        for (int t = 0; t < 400; ++t) dry.step();
        CHECK(!dry.world().any_vents() && dry.events().count(EventKind::SeaVent) == 0);
        Simulation sim(cfg);
        flood(sim);
        sim.trigger_event(EventKind::SeaVent);
        for (int t = 0; t < 400; ++t) sim.step();
        CHECK(sim.world().any_vents() && sim.events().count(EventKind::SeaVent) == 1);
        bool wet = true;
        for (int c = 0; c < sim.world().num_cells(); ++c)
            if (sim.world().vent(c) > 0.0f) wet = wet && sim.world().under_water(c);
        CHECK(wet);
        for (int t = 0; t < 61000; ++t) sim.step();
        CHECK(!sim.world().any_vents());
    }
    {   // A volcano under the sea: the hill stays, the heat and the vent fade.
        Simulation sim(cfg);
        flood(sim);
        snapshot(sim.world(), h0, t0);
        sim.trigger_event(EventKind::Eruption);
        for (int t = 0; t < 400; ++t) sim.step();
        CHECK(sim.world().any_vents());
        for (int t = 0; t < 40000; ++t) sim.step();
        snapshot(sim.world(), h1, t1);
        CHECK(!sim.world().any_vents());
        double rise = 0.0;
        for (std::size_t i = 0; i < h0.size(); ++i) rise = std::max(rise, static_cast<double>(h1[i] - h0[i]));
        CHECK(rise > 4.0);
    }
    {   // A meteor: a crater stays, its heat fades, and any dust clears.
        Simulation sim(cfg);
        snapshot(sim.world(), h0, t0);
        sim.trigger_event(EventKind::Meteor);
        for (int t = 0; t < 41000; ++t) sim.step();
        snapshot(sim.world(), h1, t1);
        CHECK(worst(h0, h1) > 2.0);
        CHECK(near(sim.world().sun_scale(), 1.0f, 1e-4));
    }
    {   // Climate drift moves the whole world and stays.
        Simulation sim(cfg);
        sim.trigger_event(EventKind::ClimateDrift);
        for (int t = 0; t < 31000; ++t) sim.step();
        CHECK(std::abs(sim.world().climate_offset()) > 1e-4f);
    }
    {   // Same seed, same events; and the log replays them exactly with events off.
        SimConfig busy = cfg;
        busy.event_rate = 10.0f;
        Simulation a(busy), b(busy);
        for (int t = 0; t < 20000; ++t) { a.step(); b.step(); }
        CHECK(a.powers().log().size() > 10 && a.powers().log().size() == b.powers().log().size());
        Simulation replay(cfg);
        for (const PowerCommand& c : a.powers().log()) replay.cast(c);
        for (int t = 0; t < 20000; ++t) replay.step();
        snapshot(a.world(), h0, t0);
        snapshot(replay.world(), h1, t1);
        CHECK(worst(h0, h1) == 0.0 && worst(t0, t1) == 0.0);
        CHECK(a.world().sun_scale() == replay.world().sun_scale());
    }
}

int main() {
    test_brush();
    test_globals();
    test_ramp_matches_instant();
    test_script();
    test_sim();
    test_world_events();
    if (failures == 0) std::puts("all power tests passed");
    return failures == 0 ? 0 : 1;
}
