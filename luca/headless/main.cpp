// Headless runner: runs the core for N ticks with no graphics and writes
// metrics.csv (+ an optional end-of-run snapshot) for plotting.
//
//   luca_headless --ticks 20000 --seed 42 --size 128 --light gradient --out out/
//
// This is where tuning happens. It runs as fast as the core allows.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "laws.hpp"
#include "metrics.hpp"
#include "sim.hpp"
#include "snapshot.hpp"

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
        "  --day N          ticks per day, 0 = no night     (default 600)\n"
        "  --day-swing F    °C between warm afternoon and cold night (default 10)\n"
        "  --windfall F     food dropped per cell per tick, 0 = none (default 0.001)\n"
        "  --events F       the world acts on its own: storms, eruptions, meteors,\n"
        "                   climate drift; 1 = one every ~5,000 ticks (default 0)\n"
        "  --nutrients F    mean free matter per cell     (default 6)\n"
        "  --sea H          sea level in height units; ground below it is under water (default 0: dry)\n"
        "  --sea-vents N    lay N hot vents on the sea floor when the world is made (default 0)\n"
        "  --interval N     ticks between metrics rows    (default 100)\n"
        "  --script FILE    god-power script (format: core/powers.hpp)\n"
        "  --follow NAME    follow a species by name (from when it appears); alerts.txt\n"
        "  --load FILE      continue a saved world (its config comes from the file;\n"
        "                   --ticks counts on from the tick it was saved at)\n"
        "  --fork-seed N    with --load: give the loaded world new dice, so each N\n"
        "                   gives a different future of the same world\n"
        "  --knockout SP    with --load: every member of species SP (name or id) dies\n"
        "                   at once and leaves its corpse (repeatable)\n"
        "  --scenario KEY   set a goal (core/scenario.hpp), e.g. first-steps\n"
        "  --probe          also write probe.csv (what became of meals, how bodies died) and leavings.csv (who took up whose leavings)\n"
        "  --save FILE      save the world to FILE at the end\n"
        "  --save-every N   also save every N ticks, to FILE with the tick in its name\n"
        "  --set NAME=VALUE change a law for this run (repeatable; core/laws.hpp).\n"
        "                   With --load: applied on top of the laws it was saved under\n"
        "  --laws FILE      change the laws listed in FILE (\"NAME VALUE\" a line)\n"
        "  --list-laws      print every law with its value and exit\n"
        "  --out DIR        output directory              (default out)\n"
        "  --no-snapshot    skip end-of-run snapshot files\n"
        "  --quiet          no progress lines");
}

bool arg_is(const char* a, const char* name) { return std::strcmp(a, name) == 0; }

}  // namespace

int main(int argc, char** argv) {
    luca::SimConfig cfg;
    long long ticks = 20000;
    std::string out_dir = "out", script_path, follow_name, load_path, save_path;
    long long save_every = 0;
    bool snapshot = true, quiet = false;
    std::string world_flag;   // a flag that sets the world's config (not allowed with --load)
    bool events_given = false, interval_given = false, list_laws = false, probe = false;
    bool fork_given = false;
    unsigned long long fork_seed = 0;
    std::vector<std::string> knockouts;
    std::string scenario_key;
    luca::LawChanges law_changes;   // --set and --laws, in the order given

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        auto next = [&]() -> const char* {
            if (i + 1 >= argc) { std::fprintf(stderr, "missing value for %s\n", a); std::exit(2); }
            return argv[++i];
        };
        for (const char* w : {"--seed", "--size", "--pop", "--sun", "--day", "--day-swing", "--windfall", "--nutrients", "--light", "--sea", "--sea-vents"})
            if (arg_is(a, w)) world_flag = a;
        if (arg_is(a, "--events")) events_given = true;
        if (arg_is(a, "--interval")) interval_given = true;
        if      (arg_is(a, "--ticks"))       ticks = std::atoll(next());
        else if (arg_is(a, "--load"))        load_path = next();
        else if (arg_is(a, "--save"))        save_path = next();
        else if (arg_is(a, "--fork-seed"))   { fork_seed = std::strtoull(next(), nullptr, 10); fork_given = true; }
        else if (arg_is(a, "--knockout"))    knockouts.push_back(next());
        else if (arg_is(a, "--probe"))       probe = true;
        else if (arg_is(a, "--scenario"))    scenario_key = next();
        else if (arg_is(a, "--save-every"))  save_every = std::atoll(next());
        else if (arg_is(a, "--seed"))        cfg.seed = std::strtoull(next(), nullptr, 10);
        else if (arg_is(a, "--size"))        cfg.world_size = std::atoi(next());
        else if (arg_is(a, "--pop"))         cfg.initial_population = std::atoi(next());
        else if (arg_is(a, "--sun"))         cfg.sun_strength = static_cast<float>(std::atof(next()));
        else if (arg_is(a, "--day"))         cfg.day_length = std::atoi(next());
        else if (arg_is(a, "--day-swing"))   cfg.temp_day_swing = static_cast<float>(std::atof(next()));
        else if (arg_is(a, "--sea"))         cfg.sea_level = static_cast<float>(std::atof(next()));
        else if (arg_is(a, "--sea-vents"))   cfg.sea_vents = std::atoi(next());
        else if (arg_is(a, "--windfall"))    cfg.windfall_rate = static_cast<float>(std::atof(next()));
        else if (arg_is(a, "--events"))      cfg.event_rate = static_cast<float>(std::atof(next()));
        else if (arg_is(a, "--nutrients"))   cfg.nutrient_mean = static_cast<float>(std::atof(next()));
        else if (arg_is(a, "--interval"))    cfg.metrics_interval = std::atoi(next());
        else if (arg_is(a, "--out"))         out_dir = next();
        else if (arg_is(a, "--set") || arg_is(a, "--laws")) {
            const bool file = arg_is(a, "--laws");
            const std::string v = next();
            std::string error;
            if (!(file ? luca::read_laws_file(v, law_changes, error) : luca::parse_law(v, law_changes, error))) {
                std::fprintf(stderr, "%s %s: %s\n", a, v.c_str(), error.c_str());
                return 2;
            }
        }
        else if (arg_is(a, "--list-laws"))   list_laws = true;
        else if (arg_is(a, "--script"))      script_path = next();
        else if (arg_is(a, "--follow"))      follow_name = next();
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
    if (cfg.world_size < 2 || cfg.metrics_interval < 1 || ticks < 1 || save_every < 0) {
        std::fprintf(stderr, "size must be >= 2, interval and ticks >= 1, save-every >= 0\n");
        return 2;
    }
    if (!load_path.empty() && !world_flag.empty()) {
        std::fprintf(stderr, "%s sets up a new world; a loaded world keeps the config it was saved with\n", world_flag.c_str());
        return 2;
    }
    if (load_path.empty() && (fork_given || !knockouts.empty())) {
        std::fprintf(stderr, "--fork-seed and --knockout act on a saved world: use them with --load\n");
        return 2;
    }
    if (save_every > 0 && save_path.empty()) {
        std::fprintf(stderr, "--save-every needs --save FILE\n");
        return 2;
    }
    // world.snap -> world_12000.snap
    auto save_at = [&](const luca::Simulation& s, bool numbered) {
        std::string path = save_path;
        if (numbered) {
            const std::size_t dot = path.find_last_of('.'), slash = path.find_last_of('/');
            const std::string tag = "_" + std::to_string(s.tick());
            if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) path.insert(dot, tag);
            else path += tag;
        }
        std::string error;
        if (!luca::save_snapshot(s, path, error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return false;
        }
        if (!quiet) std::printf("saved %s\n", path.c_str());
        return true;
    };

    if (list_laws) {
        luca::apply_laws(luca::g_laws, law_changes);
        const luca::Laws defaults{};
        auto default_text = [](float d) {
            char b[48];
            std::snprintf(b, sizeof b, " (default %g)", static_cast<double>(d));
            return std::string(b);
        };
        for (const luca::LawInfo& law : luca::law_table()) {
            const float v = luca::g_laws.*law.field, d = defaults.*law.field;
            std::printf("%-22s %-10g %s%s\n", law.name, static_cast<double>(v), law.where,
                        v != d ? default_text(d).c_str() : "");
        }
        return 0;
    }

    std::filesystem::create_directories(out_dir);
    const std::string metrics_path = out_dir + "/metrics.csv";


    std::unique_ptr<luca::Simulation> owned;
    if (load_path.empty()) {
        luca::apply_laws(luca::g_laws, law_changes);   // before the first body is compiled
        owned = std::make_unique<luca::Simulation>(cfg);
        owned->seed_population();
    } else {
        std::string error;
        owned = luca::load_snapshot(load_path, error, law_changes);
        if (!owned) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
        const float rate = cfg.event_rate;
        const int interval = cfg.metrics_interval;
        cfg = owned->config();
        if (events_given) owned->set_event_rate(rate);
        if (interval_given) cfg.metrics_interval = interval;
        if (fork_given) owned->fork(fork_seed);
        for (const std::string& name : knockouts) {
            const auto& all = owned->species().all();
            char* end = nullptr;
            const unsigned long id = std::strtoul(name.c_str(), &end, 10);
            const luca::Species* found = nullptr;
            for (const luca::Species& sp : all)
                if (sp.name == name || (end && *end == '\0' && !name.empty() && sp.id == id)) { found = &sp; break; }
            if (!found) {
                std::fprintf(stderr, "--knockout: no species called %s\n", name.c_str());
                return 2;
            }
            const std::size_t gone = owned->knockout(found->id);
            if (!quiet) std::printf("knocked out %s: %zu bodies\n", name.c_str(), gone);
        }
        if (!quiet)
            std::printf("loaded %s: tick %llu, %zu creatures\n", load_path.c_str(), static_cast<unsigned long long>(owned->tick()),
                        owned->creatures().size());
    }
    luca::Simulation& sim = *owned;
    if (!scenario_key.empty()) {
        const int id = luca::find_scenario(scenario_key);
        if (id < 0) {
            std::fprintf(stderr, "unknown scenario %s; there are:", scenario_key.c_str());
            for (const auto& s : luca::scenario_list()) std::fprintf(stderr, " %s", s.key);
            std::fprintf(stderr, "\n");
            return 2;
        }
        sim.start_scenario(static_cast<uint32_t>(id));
    }
    {
        // The laws in force, readable back with --laws.
        const luca::LawChanges changed = luca::changed_laws(luca::g_laws);
        std::ofstream f(out_dir + "/laws.txt");
        f << "# laws that differ from this build's defaults (" << changed.size() << ")\n";
        for (const auto& [name, value] : changed) f << name << ' ' << value << '\n';
        if (!quiet)
            for (const auto& [name, value] : changed) std::printf("law %s = %g\n", name.c_str(), static_cast<double>(value));
    }

    if (!script_path.empty()) {
        std::ifstream script(script_path);
        std::vector<luca::PowerCommand> commands;
        std::string error;
        if (!script) { std::fprintf(stderr, "cannot read %s\n", script_path.c_str()); return 1; }
        if (!luca::parse_power_script(script, commands, error)) {
            std::fprintf(stderr, "%s: %s\n", script_path.c_str(), error.c_str());
            return 2;
        }
        for (const luca::PowerCommand& cmd : commands) sim.cast(cmd);
    }
    std::size_t chronicle_shown = 0, alerts_shown = 0, species_seen = 0;
    // Follow a species by name as soon as one with that name exists.
    auto look_for_followed = [&] {
        if (follow_name.empty()) return;
        const auto& all = sim.species().all();
        for (; species_seen < all.size(); ++species_seen)
            if (all[species_seen].name == follow_name) {
                sim.follow(all[species_seen].id);
                follow_name.clear();
                return;
            }
    };
    look_for_followed();

    luca::MetricsWriter metrics;
    if (!metrics.open(metrics_path)) {
        std::fprintf(stderr, "cannot write %s\n", metrics_path.c_str());
        return 1;
    }

    luca::ProbeWriter probes;
    if (probe && !probes.open(out_dir + "/probe.csv")) {
        std::fprintf(stderr, "cannot write %s/probe.csv\n", out_dir.c_str());
        return 1;
    }

    luca::IntervalTotals interval;
    interval.begin(sim);
    double worst_audit = 0.0, total_ms = 0.0;

    long long ticks_run = 0;
    for (long long t = 0; t < ticks; ++t) {
        sim.step();
        ++ticks_run;
        look_for_followed();
        interval.add(sim.last_stats());
        total_ms += sim.last_stats().tick_ms;

        if (!quiet)
            for (const auto& entries = sim.chronicle().entries(); chronicle_shown < entries.size(); ++chronicle_shown)
                std::printf("  ~ %s\n", luca::format_entry(entries[chronicle_shown]).c_str());
        if (!quiet)
            for (const auto& entries = sim.alerts().entries(); alerts_shown < entries.size(); ++alerts_shown)
                std::printf("  ! %s\n", luca::format_entry(entries[alerts_shown]).c_str());

        const bool last = (t + 1 == ticks);
        if (sim.tick() % static_cast<uint64_t>(cfg.metrics_interval) == 0 || last) {
            const double audit = metrics.write_row(sim, interval);
            if (probe) probes.write_row(sim, interval);
            if (std::abs(audit) > std::abs(worst_audit)) worst_audit = audit;
            if (!quiet)
                std::printf("tick %8llu  pop %7zu  births %6zu  deaths %6zu  sun/living/dead-fed %zu/%zu/%zu  movers %zu  species %zu  %.3f ms/tick  audit %+.2e\n",
                            static_cast<unsigned long long>(sim.tick()), sim.creatures().size(),
                            interval.births, interval.deaths,
                            sim.census().producers, sim.census().hunters, sim.census().scavengers, sim.census().movers,
                            sim.species().living_established(),
                            interval.total_ms / static_cast<double>(interval.ticks), audit);
            interval.begin(sim);
        }
        if (save_every > 0 && sim.tick() % static_cast<uint64_t>(save_every) == 0 && !last && !save_at(sim, true)) return 1;
        if (sim.creatures().size() == 0) {
            std::printf("extinction at tick %llu\n", static_cast<unsigned long long>(sim.tick()));
            break;
        }
    }

    if (!save_path.empty() && !save_at(sim, false)) return 1;

    if (snapshot) {
        luca::write_creature_snapshot(sim, out_dir + "/creatures.csv");
        luca::write_world_snapshot(sim, out_dir + "/world.csv");
    }

    luca::write_species_table(sim, out_dir + "/species.csv");
    if (probe) {
        // Who lived on whose leavings (the leavings tag; observation only), most first.
        std::vector<std::pair<uint64_t, double>> rows(sim.leavings_eaten().begin(), sim.leavings_eaten().end());
        std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) { return a.second != b.second ? a.second > b.second : a.first < b.first; });
        std::ofstream f(out_dir + "/leavings.csv");
        f << "taker,taker_name,giver,giver_name,energy\n";
        for (const auto& [key, e] : rows) {
            const uint32_t taker = static_cast<uint32_t>(key >> 32), giver = static_cast<uint32_t>(key & 0xFFFFFFFFu);
            f << taker << ',' << sim.species().get(taker).name << ',' << giver << ',' << sim.species().get(giver).name << ',' << e << '\n';
        }
    }
    {
        std::ofstream f(out_dir + "/chronicle.txt");
        for (const auto& e : sim.chronicle().entries()) f << luca::format_entry(e) << '\n';
        if (!sim.followed().empty()) {
            std::ofstream a(out_dir + "/alerts.txt");
            for (const auto& e : sim.alerts().entries()) a << luca::format_entry(e) << '\n';
        }
        // Everything that was cast, replayable with --script.
        std::ofstream p(out_dir + "/powers.txt");
        luca::write_power_script(p, sim.powers().log());
    }

    std::printf("\ndone: %llu ticks, final population %zu, %.3f ms/tick average, worst audit error %+.2e\n",
                static_cast<unsigned long long>(sim.tick()), sim.creatures().size(),
                total_ms / static_cast<double>(ticks_run ? ticks_run : 1), worst_audit);
    if (sim.scenario().active())
        std::printf("scenario %s: %s\n", sim.scenario().info().key,
                    sim.scenario().outcome == luca::Outcome::Running ? "still running" : sim.scenario().verdict.c_str());
    std::printf("metrics: %s\n", metrics_path.c_str());
    return 0;
}
