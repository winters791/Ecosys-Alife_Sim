// Headless runner: runs the core for N ticks with no graphics and writes
// metrics.csv (+ an optional end-of-run snapshot) for plotting.
//
//   luca_headless --ticks 20000 --seed 42 --size 128 --light gradient --out out/
//
// This is where tuning happens. It runs as fast as the core allows.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#include "metrics.hpp"
#include "sim.hpp"

namespace {

void usage() {
    std::puts(
        "luca_headless [options]\n"
        "  --ticks N        ticks to run                  (default 20000)\n"
        "  --seed N         RNG seed                      (default 42)\n"
        "  --size N         world cells per side          (default 128)\n"
        "  --pop N          initial LUCA count            (default 200)\n"
        "  --light MODE     uniform | gradient            (default gradient)\n"
        "  --sun F          energy per cell per tick      (default 1.0)\n"
        "  --interval N     ticks between metrics rows    (default 100)\n"
        "  --out DIR        output directory              (default out)\n"
        "  --no-snapshot    skip end-of-run snapshot files\n"
        "  --quiet          no progress lines");
}

bool arg_is(const char* a, const char* name) { return std::strcmp(a, name) == 0; }

}  // namespace

int main(int argc, char** argv) {
    luca::SimConfig cfg;
    long long ticks = 20000;
    std::string out_dir = "out";
    bool snapshot = true, quiet = false;

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        auto next = [&]() -> const char* {
            if (i + 1 >= argc) { std::fprintf(stderr, "missing value for %s\n", a); std::exit(2); }
            return argv[++i];
        };
        if      (arg_is(a, "--ticks"))       ticks = std::atoll(next());
        else if (arg_is(a, "--seed"))        cfg.seed = std::strtoull(next(), nullptr, 10);
        else if (arg_is(a, "--size"))        cfg.world_size = std::atoi(next());
        else if (arg_is(a, "--pop"))         cfg.initial_population = std::atoi(next());
        else if (arg_is(a, "--sun"))         cfg.sun_strength = static_cast<float>(std::atof(next()));
        else if (arg_is(a, "--interval"))    cfg.metrics_interval = std::atoi(next());
        else if (arg_is(a, "--out"))         out_dir = next();
        else if (arg_is(a, "--no-snapshot")) snapshot = false;
        else if (arg_is(a, "--quiet"))       quiet = true;
        else if (arg_is(a, "--light")) {
            const std::string m = next();
            if (m == "uniform")       cfg.light_mode = luca::LightMode::Uniform;
            else if (m == "gradient") cfg.light_mode = luca::LightMode::Gradient;
            else { std::fprintf(stderr, "unknown light mode: %s\n", m.c_str()); return 2; }
        }
        else if (arg_is(a, "--help") || arg_is(a, "-h")) { usage(); return 0; }
        else { std::fprintf(stderr, "unknown option: %s\n", a); usage(); return 2; }
    }
    if (cfg.world_size < 2 || cfg.metrics_interval < 1 || ticks < 1) {
        std::fprintf(stderr, "size must be >= 2, interval and ticks >= 1\n");
        return 2;
    }

    std::filesystem::create_directories(out_dir);
    const std::string metrics_path = out_dir + "/metrics.csv";

    luca::Simulation sim(cfg);
    sim.seed_population();

    luca::MetricsWriter metrics;
    if (!metrics.open(metrics_path)) {
        std::fprintf(stderr, "cannot write %s\n", metrics_path.c_str());
        return 1;
    }

    luca::IntervalTotals interval;
    interval.begin(sim);
    double worst_audit = 0.0, total_ms = 0.0;

    for (long long t = 0; t < ticks; ++t) {
        sim.step();
        interval.add(sim.last_stats());
        total_ms += sim.last_stats().tick_ms;

        const bool last = (t + 1 == ticks);
        if ((t + 1) % cfg.metrics_interval == 0 || last) {
            const double audit = metrics.write_row(sim, interval);
            if (std::abs(audit) > std::abs(worst_audit)) worst_audit = audit;
            if (!quiet)
                std::printf("tick %8llu  pop %7zu  births %6zu  deaths %6zu  %.3f ms/tick  audit %+.2e\n",
                            static_cast<unsigned long long>(sim.tick()), sim.creatures().size(),
                            interval.births, interval.deaths,
                            interval.total_ms / static_cast<double>(interval.ticks), audit);
            interval.begin(sim);
        }
        if (sim.creatures().size() == 0) {
            std::printf("extinction at tick %llu\n", static_cast<unsigned long long>(sim.tick()));
            break;
        }
    }

    if (snapshot) {
        luca::write_creature_snapshot(sim, out_dir + "/creatures.csv");
        luca::write_world_snapshot(sim, out_dir + "/world.csv");
    }

    std::printf("\ndone: %llu ticks, final population %zu, %.3f ms/tick average, worst audit error %+.2e\n",
                static_cast<unsigned long long>(sim.tick()), sim.creatures().size(),
                total_ms / static_cast<double>(sim.tick() ? sim.tick() : 1), worst_audit);
    std::printf("metrics: %s\n", metrics_path.c_str());
    return 0;
}
