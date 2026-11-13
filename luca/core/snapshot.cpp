#include "snapshot.hpp"
#include "laws.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <type_traits>

namespace luca {

namespace {

// --- Writing and reading with one field list ------------------------------------
// Every record below is written by one function template that takes either an
// Out or an In, so what is written and what is read can never drift apart.

struct Out {
    std::string buf;

    void raw(const void* p, std::size_t n) { buf.append(static_cast<const char*>(p), n); }
    template <class T>
    void operator()(T& v) {
        static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T>);
        raw(&v, sizeof v);
    }
    void operator()(std::string& s) {
        uint32_t n = static_cast<uint32_t>(s.size());
        (*this)(n);
        raw(s.data(), s.size());
    }
    void operator()(Mix& m) { for (float& v : m.v) (*this)(v); }

    // A record: its length first, so a reader can skip what it doesn't know.
    template <class F>
    void rec(F&& f) {
        const std::size_t at = buf.size();
        uint32_t n = 0;
        (*this)(n);
        f(*this);
        n = static_cast<uint32_t>(buf.size() - at - sizeof n);
        std::memcpy(&buf[at], &n, sizeof n);
    }
    // A list of records.
    template <class T, class F>
    void list(std::vector<T>& v, F&& f) {
        uint32_t n = static_cast<uint32_t>(v.size());
        (*this)(n);
        for (T& e : v) rec([&](auto& io) { f(io, e); });
    }
    // A plain array of numbers, written in one go (per-cell fields).
    template <class T>
    void array(std::vector<T>& v) {
        static_assert(std::is_trivially_copyable_v<T>);
        uint32_t n = static_cast<uint32_t>(v.size());
        (*this)(n);
        raw(v.data(), v.size() * sizeof(T));
    }
};

struct In {
    const char* p = nullptr;
    const char* end = nullptr;
    bool bad = false;   // a length ran past its record: the file is damaged

    std::size_t left() const { return static_cast<std::size_t>(end - p); }
    // Past the end of a record a field keeps its default (an older file).
    template <class T>
    void operator()(T& v) {
        static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T>);
        if (left() < sizeof v) { p = end; return; }
        std::memcpy(&v, p, sizeof v);
        p += sizeof v;
    }
    void operator()(std::string& s) {
        uint32_t n = 0;
        if (left() < sizeof n) { p = end; return; }
        (*this)(n);
        if (n > left()) { bad = true; p = end; return; }
        s.assign(p, n);
        p += n;
    }
    void operator()(Mix& m) { for (float& v : m.v) (*this)(v); }

    In sub() {
        uint32_t n = 0;
        In r;
        if (left() < sizeof n) { p = end; r.p = r.end = end; return r; }
        (*this)(n);
        if (n > left()) { bad = true; n = static_cast<uint32_t>(left()); }
        r.p = p;
        r.end = p + n;
        p += n;
        return r;
    }
    template <class F>
    void rec(F&& f) {
        In r = sub();
        f(r);
        bad = bad || r.bad;
    }
    template <class T, class F>
    void list(std::vector<T>& v, F&& f) {
        uint32_t n = 0;
        if (left() < sizeof n) { v.clear(); p = end; return; }
        (*this)(n);
        // Every record takes at least its length: a count beyond that is damage.
        if (static_cast<std::size_t>(n) * sizeof(uint32_t) > left()) { bad = true; v.clear(); return; }
        v.assign(n, T{});
        for (T& e : v) rec([&](auto& io) { f(io, e); });
    }
    template <class T>
    void array(std::vector<T>& v) {
        uint32_t n = 0;
        if (left() < sizeof n) { p = end; return; }
        (*this)(n);
        if (static_cast<std::size_t>(n) * sizeof(T) > left()) { bad = true; p = end; return; }
        v.resize(n);
        std::memcpy(v.data(), p, n * sizeof(T));
        p += n * sizeof(T);
    }
};

// A bool travels as one byte.
template <class IO>
void flag(IO& io, bool& b) {
    uint8_t v = b ? 1 : 0;
    io(v);
    b = v != 0;
}

// Set while reading a version 2 snapshot, whose brain links number outputs from
// 9 and hidden neurons from 20.
bool g_legacy_nodes = false;
uint8_t legacy_source(uint8_t n) { return n >= 20 ? static_cast<uint8_t>(n + (FIRST_HIDDEN - 20)) : n; }
uint8_t legacy_target(uint8_t n) {
    if (n >= 20) return static_cast<uint8_t>(n + (FIRST_HIDDEN - 20));
    return n >= 9 ? static_cast<uint8_t>(n + (FIRST_OUTPUT - 9)) : n;
}

// --- The genome ---------------------------------------------------------------
// Add a new gene here, at the end, and an older snapshot reads it as its default.
template <class IO>
void io_genome(IO& io, Genome& g) {
    io(g.core_radius);
    for (float& s : g.shape) io(s);
    io(g.pigment);
    io(g.hue);
    io(g.thermal_optimum);
    io(g.thermal_tolerance);
    io(g.repro_threshold);
    io(g.offspring_share);
    io(g.bite);
    io(g.dormancy);
    io(g.seed_throw);
    io(g.elongation);
    io(g.squareness);
    // Two genes deleted after the ablation pass (mutability, repair; 14.10.8):
    // still written as 0 and read and dropped, so the format is unchanged.
    float retired[2] = {0.0f, 0.0f};
    io(retired[0]);
    io(retired[1]);
    io(g.birth_size);
    io.list(g.parts, [](auto& r, Part& p) {
        r(p.type);
        r(p.size);
        r(p.theta);
        r(p.phi);
        r(p.stalk);
        r(p.channel);   // 2026-11-13; an older snapshot reads 0
    });
    io.list(g.enzymes, [](auto& r, Enzyme& e) {
        r(e.x);
        r(e.y);
        r(e.rate);
        r(e.x2);
        r(e.y2);
    });
    io(g.brain.hidden);
    for (int k = 0; k < MAX_HIDDEN; ++k) {
        io(g.brain.tau[k]);
        io(g.brain.bias[k]);
    }
    io.list(g.brain.links, [](auto& r, BrainLink& l) {
        r(l.from);
        r(l.to);
        r(l.weight);
    });
    if constexpr (std::is_same_v<IO, In>) {
        if (g_legacy_nodes)
            for (BrainLink& l : g.brain.links) { l.from = legacy_source(l.from); l.to = legacy_target(l.to); }
    }
}

// --- The config ---------------------------------------------------------------
// By name, so the header shows it and a renamed or new setting is harmless.
template <class F>
void config_fields(SimConfig& c, F&& f) {
    f("seed", c.seed);
    f("world_size", c.world_size);
    f("cell_size", c.cell_size);
    f("max_height", c.max_height);
    f("height_limit", c.height_limit);
    f("light_mode", c.light_mode);
    f("sun_strength", c.sun_strength);
    f("gradient_min", c.gradient_min);
    f("slope_shade", c.slope_shade);
    f("day_length", c.day_length);
    f("nutrient_mean", c.nutrient_mean);
    f("nutrient_patchiness", c.nutrient_patchiness);
    f("wash_rate", c.wash_rate);
    f("temp_sea_level", c.temp_sea_level);
    f("temp_lapse", c.temp_lapse);
    f("temp_latitude_drop", c.temp_latitude_drop);
    f("temp_day_swing", c.temp_day_swing);
    f("gene_transfer_chance", c.gene_transfer_chance);
    f("vent_power", c.vent_power);
    f("vent_heat", c.vent_heat);
    f("vent_max", c.vent_max);
    f("windfall_rate", c.windfall_rate);
    f("windfall_size", c.windfall_size);
    f("event_rate", c.event_rate);
    f("initial_population", c.initial_population);
    f("initial_energy", c.initial_energy);
    f("birth_max_step", c.birth_max_step);
    f("metrics_interval", c.metrics_interval);
}

// Six significant digits, for people (the chronicle).
std::string short_number(float v) {
    std::ostringstream o;
    o << v;
    return o.str();
}

template <class T>
std::string config_text(const T& v) {
    if constexpr (std::is_same_v<T, LightMode>) return v == LightMode::Uniform ? "uniform" : "gradient";
    else {
        std::ostringstream o;
        o.precision(9);
        o << v;
        return o.str();
    }
}
template <class T>
bool config_parse(const std::string& s, T& v) {
    if constexpr (std::is_same_v<T, LightMode>) {
        if (s == "uniform") v = LightMode::Uniform;
        else if (s == "gradient") v = LightMode::Gradient;
        else return false;
        return true;
    } else {
        std::istringstream i(s);
        T x{};
        if (!(i >> x)) return false;
        v = x;
        return true;
    }
}

constexpr char MAGIC[] = "luca-snapshot";

}  // namespace

// Friend of Simulation, World, PowerSystem, WorldEvents and SpeciesRegistry.
struct SnapshotIO {
    // Sections, in the order they are written. Each is one function for both ways.

    template <class IO>
    static void clocks(IO& io, Simulation& s) {
        io(s.tick_);
        io(s.next_lineage_);
        io(s.next_id_);
        io(s.matter_brought_up_);
        io(s.arrived_energy_);
        io(s.arrived_matter_);
        io(s.draw_seed_);
    }

    template <class IO>
    static void detectors(IO& io, Simulation& s) {
        uint64_t peak = s.peak_pop_, from = s.crash_from_, milestone = s.next_milestone_, day_peak = s.day_peak_;
        io(peak);
        io(from);
        io(s.crash_tick_);
        io(milestone);
        io(day_peak);
        io(s.day_seen_);
        s.peak_pop_ = peak;
        s.crash_from_ = from;
        s.next_milestone_ = milestone;
        s.day_peak_ = day_peak;
        for (bool* b : {&s.extinct_logged_, &s.movers_logged_, &s.hunters_logged_, &s.scavengers_logged_, &s.cleaners_logged_,
                        &s.chemotrophs_logged_, &s.sighted_logged_, &s.armoured_logged_, &s.poisonous_logged_, &s.transfer_logged_})
            flag(io, *b);
        // The census is refreshed every 50 ticks and read by the detectors in between.
        Census& k = s.census_;
        for (std::size_t* v : {&k.producers, &k.hunters, &k.scavengers, &k.cleaners, &k.chemotrophs, &k.mixed, &k.young, &k.growing,
                               &k.movers, &k.sighted, &k.armoured, &k.poisonous}) {
            uint64_t x = *v;
            io(x);
            *v = static_cast<std::size_t>(x);
        }
    }

    template <class IO>
    static void scenario(IO& io, ScenarioState& sc) {
        uint32_t outcome = static_cast<uint32_t>(sc.outcome);
        io(sc.id);
        io(sc.start_tick);
        io(outcome);
        io(sc.end_tick);
        io(sc.verdict);
        sc.outcome = static_cast<Outcome>(outcome);
    }

    template <class IO>
    static void events(IO& io, WorldEvents& e) {
        io(e.seed_);
        io(e.rate_);
        for (uint32_t& c : e.counts_) io(c);
        io.list(e.notes_, [](auto& r, WorldEvents::Note& n) {
            r(n.tick);
            r(n.text);
        });
    }

    template <class IO>
    static void world(IO& io, World& w) {
        io.array(w.height_);
        io.array(w.heat_);
        io.array(w.vent_);
        io(w.sun_scale_);
        io(w.climate_);
        io(w.height_rev_);
        io(w.climate_rev_);
        io(w.vent_rev_);
    }

    template <class IO>
    static void soil(IO& io, Soil& s) {
        io.array(s.nutrient);
        io.array(s.corpse);
        io.array(s.dissolved);
        io.array(s.vent_made);
    }

    // Stoichiometry (docs/design/stoichiometry.md): the mineral P, in sections
    // of its own so worlds saved before it still load. Missing, the soil's P
    // is laid out afresh and every body holds exactly the P its body needs.
    template <class IO>
    static void minerals(IO& io, Simulation& s) {
        io(s.p_brought_up_);
        io(s.arrived_p_);
        io.array(s.soil_.mineral);
        io.array(s.soil_.corpse_p);
    }

    template <class IO>
    static void command(IO& r, PowerCommand& c) {
        r(c.tick);
        r(c.kind);
        r(c.x);
        r(c.y);
        r(c.radius);
        r(c.amount);
        r(c.duration);
        flag(r, c.quiet);
        r(c.creature);
    }

    template <class IO>
    static void powers(IO& io, PowerSystem& p) {
        io.list(p.pending_, [](auto& r, PowerCommand& c) { command(r, c); });
        io.list(p.active_, [](auto& r, PowerSystem::Active& a) {
            r.rec([&](auto& rr) { command(rr, a.cmd); });
            r(a.start);
            r(a.applied);
        });
        io.list(p.log_, [](auto& r, PowerCommand& c) { command(r, c); });
        io.list(p.arrivals_, [](auto& r, PowerCommand& c) { command(r, c); });
    }

    template <class IO>
    static void record(IO& r, Record& x) {
        r(x.value);
        r(x.who);
        r(x.tick);
    }

    template <class IO>
    static void species(IO& io, SpeciesRegistry& reg) {
        io.list(reg.species_, [](auto& r, Species& s) {
            r(s.id);
            r(s.parent);
            r(s.name);
            r.rec([&](auto& g) { io_genome(g, s.representative); });
            r(s.born_tick);
            r(s.extinct_tick);
            r(s.population);
            r(s.peak);
            flag(r, s.established);
            flag(r, s.by_split);
            r.array(s.living_daughters);
            r(s.ever_born);
            r(s.died_eaten);
            r(s.died_other);
            r(s.kills);
            record(r, s.eldest);
            record(r, s.deadliest);
            record(r, s.most_children);
            Habits& h = s.habits;
            r(h.count);
            r(h.tick);
            for (float& d : h.diet) r(d);
            for (float* v : {&h.movers, &h.sighted, &h.sleepers, &h.armoured, &h.poisonous, &h.radius, &h.age, &h.speed, &h.light,
                             &h.temperature, &h.height, &h.x, &h.y, &h.spread})
                r(*v);
        });
        uint64_t living = reg.living_;
        io(living);
        reg.living_ = static_cast<std::size_t>(living);
        // The kill web, as a list of (killer, victim, count).
        struct Kill { uint32_t killer = 0, victim = 0; uint64_t count = 0; };
        std::vector<Kill> kills;
        for (const auto& [key, n] : reg.kill_web_) kills.push_back({key.first, key.second, n});
        io.list(kills, [](auto& r, Kill& k) {
            r(k.killer);
            r(k.victim);
            r(k.count);
        });
        reg.kill_web_.clear();
        for (const Kill& k : kills) reg.kill_web_[{k.killer, k.victim}] = k.count;
    }

    template <class IO>
    static void chronicle(IO& io, Chronicle& ch) {
        std::vector<ChronicleEntry> e = ch.entries();
        io.list(e, [](auto& r, ChronicleEntry& x) {
            r(x.tick);
            r(x.text);
        });
        ch = Chronicle{};
        for (ChronicleEntry& x : e) ch.add(x.tick, std::move(x.text));
    }

    template <class IO>
    static void watches(IO& io, Simulation& s) {
        io.list(s.watch_, [](auto& r, Simulation::Watch& w) {
            r(w.species);
            r(w.seen.count);
            r(w.seen.traits);
            r(w.seen.mass);
        });
    }

    // One creature: state only. Its stats and plans are compiled again on loading.
    struct Body {
        float x = 0, y = 0, hx = 1, hy = 0, moved = 0, ate = 0, hurt = 0, unfold = 1, wander = 0, sleep = 0, body_temp = 20, shut = 0;
        Mix pool, tissue;
        uint32_t age = 0;
        float grown = 1.0f;
        float fed_light = 0, fed_living = 0, fed_dead = 0, fed_waste = 0, fed_vent = 0;
        uint32_t kills = 0, children = 0;
        uint64_t id = 0;
        uint32_t lineage = 0, species = 0, generation = 0;
        Genome genome;
        std::vector<float> brain_state;
        bool borrowed = false;
        Genome birth, built;   // only for bodies with a borrowed blueprint (Simulation::borrowed_)
    };

    template <class IO>
    static void body(IO& r, Body& b) {
        for (float* v : {&b.x, &b.y, &b.hx, &b.hy, &b.moved, &b.ate, &b.hurt, &b.unfold, &b.wander, &b.sleep, &b.body_temp, &b.shut}) r(*v);
        r(b.pool);
        r(b.tissue);
        r(b.age);
        r(b.grown);
        for (float* v : {&b.fed_light, &b.fed_living, &b.fed_dead, &b.fed_waste, &b.fed_vent}) r(*v);
        r(b.kills);
        r(b.children);
        r(b.id);
        r(b.lineage);
        r(b.species);
        r(b.generation);
        r.rec([&](auto& g) { io_genome(g, b.genome); });
        r.array(b.brain_state);
        flag(r, b.borrowed);
        if (b.borrowed) {
            r.rec([&](auto& g) { io_genome(g, b.birth); });
            r.rec([&](auto& g) { io_genome(g, b.built); });
        }
    }

    static std::vector<Body> bodies_of(const Simulation& s) {
        const Creatures& c = s.c_;
        std::vector<Body> out(c.size());
        for (std::size_t i = 0; i < c.size(); ++i) {
            Body& b = out[i];
            b.x = c.x[i]; b.y = c.y[i]; b.hx = c.hx[i]; b.hy = c.hy[i]; b.moved = c.moved[i]; b.ate = c.ate[i]; b.hurt = c.hurt[i];
            b.unfold = c.unfold[i]; b.wander = c.wander[i]; b.sleep = c.sleep[i]; b.body_temp = c.body_temp[i]; b.shut = c.shut[i];
            b.pool = c.pool[i]; b.tissue = c.tissue[i]; b.age = c.age[i]; b.grown = c.pheno[i].grown;
            b.fed_light = c.fed_light[i]; b.fed_living = c.fed_living[i]; b.fed_dead = c.fed_dead[i];
            b.fed_waste = c.fed_waste[i]; b.fed_vent = c.fed_vent[i];
            b.kills = c.kills[i]; b.children = c.children[i];
            b.id = c.id[i]; b.lineage = c.lineage[i]; b.species = c.species[i]; b.generation = c.generation[i];
            b.genome = c.genome[i];
            b.brain_state.assign(std::begin(c.brain_state[i].hidden), std::end(c.brain_state[i].hidden));
            if (auto it = s.borrowed_.find(c.id[i]); it != s.borrowed_.end()) {
                b.borrowed = true;
                b.birth = it->second.birth;
                b.built = it->second.built;
            }
        }
        return out;
    }

    // Rebuild every creature from its state. Under the same build this gives
    // back exactly the stats it had; under a build with other rules, the
    // stats those rules give its genome.
    static void restore_bodies(Simulation& s, std::vector<Body>& bodies) {
        Creatures& c = s.c_;
        c = Creatures{};
        s.borrowed_.clear();
        c.reserve(bodies.size());
        for (Body& b : bodies) {
            // A body with a borrowed blueprint was built from an earlier genome.
            const Genome& birth = b.borrowed ? b.birth : b.genome;
            const Genome& built = b.borrowed ? b.built : b.genome;
            const Phenotype at_birth = compile_phenotype(birth, b.grown);   // decides its brain and body plans, as at birth
            const Phenotype now = b.borrowed ? compile_phenotype(built, b.grown) : at_birth;
            const std::size_t i = c.size();
            c.add(b.x, b.y, b.pool, b.tissue, b.body_temp, b.id, b.lineage, b.species, b.generation, b.genome, at_birth);
            if (b.borrowed) {
                c.brain[i] = at_birth.has_brain ? compile_brain(birth) : BrainPlan{};
                c.body[i] = at_birth.absorb > 0.0f || at_birth.part_total[static_cast<int>(PartType::Armour)] > 0.0f ? compile_body(birth)
                                                                                                                       : BodyPlan{};
                c.pheno[i] = now;
                s.borrowed_.emplace(b.id, Simulation::Borrowed{b.birth, b.built});
            }
            c.hx[i] = b.hx; c.hy[i] = b.hy; c.moved[i] = b.moved; c.ate[i] = b.ate; c.hurt[i] = b.hurt;
            c.unfold[i] = b.unfold; c.wander[i] = b.wander; c.sleep[i] = b.sleep; c.shut[i] = b.shut;
            c.age[i] = b.age;
            c.fed_light[i] = b.fed_light; c.fed_living[i] = b.fed_living; c.fed_dead[i] = b.fed_dead;
            c.fed_waste[i] = b.fed_waste; c.fed_vent[i] = b.fed_vent;
            c.kills[i] = b.kills; c.children[i] = b.children;
            for (std::size_t k = 0; k < b.brain_state.size() && k < static_cast<std::size_t>(MAX_HIDDEN); ++k)
                c.brain_state[i].hidden[k] = b.brain_state[k];

            // Under other rules a store may no longer fit its body: what doesn't fit
            // spills into its cell, as chemicals, so both books still close.
            const float over = c.pool[i].mass() - c.pheno[i].pool_capacity;
            if (over > 1e-6f) {
                const int cell = s.world_.cell_index(c.x[i], c.y[i]);
                s.soil_.dissolved[static_cast<std::size_t>(cell)].add(take_share(c.pool[i], over));
            }
            c.energy[i] = c.pool[i].energy();
        }
    }

    // Recompute what the world derives from its layers.
    static void refresh_world(World& w, uint64_t tick) {
        const uint32_t hr = w.height_rev_, cr = w.climate_rev_, vr = w.vent_rev_;
        w.vent_cells_ = 0;
        for (float v : w.vent_) w.vent_cells_ += v > 0.0f;
        w.refresh_terrain(0, 0, w.n_ - 1, w.n_ - 1);
        w.refresh_light();
        w.refresh_temperature(0, 0, w.n_ - 1, w.n_ - 1);
        // The time of day as the last tick left it.
        w.set_time(tick > 0 ? tick - 1 : 0);
        w.height_rev_ = hr;   // as saved: a viewer handed a new world re-uploads everything anyway
        w.climate_rev_ = cr;
        w.vent_rev_ = vr;
    }

    static std::string to_bytes(const Simulation& sim);
    static std::unique_ptr<Simulation> from_bytes(const std::string& bytes, std::string& error, const LawChanges& changes);
};

namespace {

// A section: four letters, then a record. On reading, sections are found by tag.
using Section = std::pair<std::string, In>;

}  // namespace

std::string SnapshotIO::to_bytes(const Simulation& sim_in) {
    Simulation& s = const_cast<Simulation&>(sim_in);   // Out only reads; the field lists take references
    SimConfig cfg = s.cfg_;

    std::ostringstream head;
    head << MAGIC << ' ' << SNAPSHOT_VERSION << '\n';
    head << "tick " << s.tick_ << '\n';
    head << "creatures " << s.c_.size() << '\n';
    head << "species " << s.species_.living_established() << '\n';
    head << "built " << __DATE__ << ' ' << __TIME__ << '\n';
    config_fields(cfg, [&](const char* name, auto& v) { head << "config " << name << ' ' << config_text(v) << '\n'; });
    // Every law in force, so a world made under other laws, or by a build
    // whose defaults have since changed, loads under the laws it grew up with.
    for (const LawInfo& law : law_table()) head << "law " << law.name << ' ' << config_text(g_laws.*law.field) << '\n';
    head << '\n';

    Out out;
    auto section = [&](const char* tag, auto&& f) {
        out.raw(tag, 4);
        out.rec(f);
    };
    std::vector<Body> bodies = bodies_of(s);
    section("CLOK", [&](auto& io) { clocks(io, s); });
    section("DETS", [&](auto& io) { detectors(io, s); });
    section("EVNT", [&](auto& io) { events(io, s.events_); });
    section("WRLD", [&](auto& io) { world(io, s.world_); });
    section("SOIL", [&](auto& io) { soil(io, s.soil_); });
    section("CRTR", [&](auto& io) { io.list(bodies, [](auto& r, Body& b) { body(r, b); }); });
    section("MINR", [&](auto& io) { minerals(io, s); });
    std::vector<float> body_p;   // tissue then store, a pair a body, in creature order
    body_p.reserve(2 * s.c_.size());
    for (std::size_t i = 0; i < s.c_.size(); ++i) {
        body_p.push_back(s.c_.tissue_p[i]);
        body_p.push_back(s.c_.pool_p[i]);
    }
    section("BODP", [&](auto& io) { io.array(body_p); });
    section("POWR", [&](auto& io) { powers(io, s.powers_); });
    section("SPEC", [&](auto& io) { species(io, s.species_); });
    section("CHRN", [&](auto& io) { chronicle(io, s.chronicle_); });
    section("ALRT", [&](auto& io) { chronicle(io, s.alerts_); });
    section("WTCH", [&](auto& io) { watches(io, s); });
    section("SCEN", [&](auto& io) { scenario(io, s.scenario_); });
    return head.str() + out.buf;
}

std::unique_ptr<Simulation> SnapshotIO::from_bytes(const std::string& bytes, std::string& error, const LawChanges& changes) {
    // The text header: the config is read from it.
    std::size_t pos = 0;
    auto line = [&](std::string& l) {
        const std::size_t nl = bytes.find('\n', pos);
        if (nl == std::string::npos) return false;
        l = bytes.substr(pos, nl - pos);
        pos = nl + 1;
        return true;
    };
    std::string l;
    if (!line(l) || l.rfind(MAGIC, 0) != 0) {
        error = "not a snapshot";
        return nullptr;
    }
    const int version = std::atoi(l.c_str() + sizeof(MAGIC));
    if (version < 2) {
        error = "snapshot version " + std::to_string(version) + " was made before counter-based draws and cannot be continued; make it again";
        return nullptr;
    }
    if (version > SNAPSHOT_VERSION) {
        error = "snapshot version " + std::to_string(version) + " is newer than this build reads (" + std::to_string(SNAPSHOT_VERSION) + ")";
        return nullptr;
    }
    g_legacy_nodes = version == 2;
    SimConfig cfg;
    Laws laws;   // a file with no law lines was made under the defaults
    while (true) {
        if (!line(l)) {
            error = "snapshot header has no end";
            return nullptr;
        }
        if (l.empty()) break;
        std::istringstream words(l);
        std::string key, name, value;
        words >> key;
        if (key == "law") {
            words >> name >> value;
            const LawInfo* law = find_law(name);
            float v = 0.0f;
            if (!law) continue;   // a law this build doesn't have
            if (!config_parse(value, v)) {
                error = "bad value for law " + name + ": " + value;
                return nullptr;
            }
            laws.*law->field = v;
            continue;
        }
        if (key != "config") continue;   // tick, creatures, built: for people
        words >> name >> value;
        bool known = false, ok = true;
        config_fields(cfg, [&](const char* n, auto& v) {
            if (name != n) return;
            known = true;
            ok = config_parse(value, v);
        });
        if (known && !ok) {
            error = "bad value for config " + name + ": " + value;
            return nullptr;
        }
    }
    if (cfg.world_size < 2) {
        error = "bad world size in snapshot";
        return nullptr;
    }

    // The sections, by tag.
    In all{bytes.data() + pos, bytes.data() + bytes.size()};
    std::vector<Section> found;
    while (all.left() >= 4 && !all.bad) {
        std::string tag(all.p, 4);
        all.p += 4;
        found.emplace_back(tag, all.sub());
    }
    if (all.bad) {
        error = "snapshot is cut short or damaged";
        return nullptr;
    }
    auto read = [&](const char* tag, auto&& f) {
        for (Section& sec : found)
            if (sec.first == tag) {
                f(sec.second);
                return !sec.second.bad;
            }
        return true;   // missing: keep what a new world of this config has
    };

    // The laws it was saved under, then any changes asked for, before a
    // single body is compiled.
    g_laws = laws;
    apply_laws(g_laws, changes);
    const LawChanges changed = laws_diff(laws, g_laws);

    // A world of the same config, then everything in it replaced.
    auto sim = std::make_unique<Simulation>(cfg);
    Simulation& s = *sim;
    const std::size_t cells = static_cast<std::size_t>(s.world_.num_cells());
    std::vector<Body> bodies;
    bool ok = true;
    ok = ok && read("CLOK", [&](In& io) { clocks(io, s); });
    ok = ok && read("DETS", [&](In& io) { detectors(io, s); });
    ok = ok && read("EVNT", [&](In& io) { events(io, s.events_); });
    ok = ok && read("WRLD", [&](In& io) { world(io, s.world_); });
    ok = ok && read("SOIL", [&](In& io) { soil(io, s.soil_); });
    ok = ok && read("CRTR", [&](In& io) { io.list(bodies, [](auto& r, Body& b) { body(r, b); }); });
    ok = ok && read("MINR", [&](In& io) { minerals(io, s); });
    std::vector<float> body_p;
    ok = ok && read("BODP", [&](In& io) { io.array(body_p); });
    ok = ok && read("POWR", [&](In& io) { powers(io, s.powers_); });
    ok = ok && read("SPEC", [&](In& io) { species(io, s.species_); });
    ok = ok && read("CHRN", [&](In& io) { chronicle(io, s.chronicle_); });
    ok = ok && read("ALRT", [&](In& io) { chronicle(io, s.alerts_); });
    ok = ok && read("WTCH", [&](In& io) { watches(io, s); });
    ok = ok && read("SCEN", [&](In& io) { scenario(io, s.scenario_); });
    if (s.scenario_.id >= scenario_list().size()) s.scenario_ = ScenarioState{};   // one this build doesn't know
    if (!ok) {
        error = "snapshot is damaged";
        return nullptr;
    }
    const World& w = s.world_;
    const Soil& so = s.soil_;
    if (w.height_.size() != cells || w.heat_.size() != cells || w.vent_.size() != cells || so.nutrient.size() != cells ||
        so.corpse.size() != cells || so.dissolved.size() != cells || so.vent_made.size() != cells || so.mineral.size() != cells ||
        so.corpse_p.size() != cells || (!body_p.empty() && body_p.size() != 2 * bodies.size())) {
        error = "snapshot's fields don't match its world size";
        return nullptr;
    }
    const std::size_t nspecies = s.species_.all().size();
    for (const Body& b : bodies)
        if (b.species >= nspecies) {
            error = "a creature in the snapshot belongs to a species it doesn't list";
            return nullptr;
        }

    // What is derived: the world's fields, the soil's sums, every creature's stats.
    refresh_world(s.world_, s.tick_);
    s.soil_.corpse_energy.resize(cells);
    s.soil_.waste.resize(cells);
    for (std::size_t c = 0; c < cells; ++c) {
        s.soil_.corpse_energy[c] = s.soil_.corpse[c].energy();
        s.soil_.waste[c] = s.soil_.dissolved[c].energy();
    }
    restore_bodies(s, bodies);
    for (std::size_t i = 0; i < s.c_.size(); ++i) {
        s.c_.tissue_p[i] = body_p.empty() ? s.c_.pheno[i].p_need : body_p[2 * i];
        s.c_.pool_p[i] = body_p.empty() ? 0.0f : body_p[2 * i + 1];
    }
    s.soil_.rebuild_bits();
    if (!changed.empty()) {
        std::string text = "The laws of the world change:";
        for (std::size_t k = 0; k < changed.size(); ++k) {
            const LawInfo* law = find_law(changed[k].first);
            text += (k ? ", " : " ") + changed[k].first + " " + short_number(laws.*law->field) + " to " + short_number(changed[k].second);
        }
        s.chronicle_.add(s.tick_, text + ".");
    }
    // Living mass per cell, as the last tick left it (the viewer reads it between ticks).
    std::fill(s.biomass_.begin(), s.biomass_.end(), 0.0f);
    s.scratch_cells_.clear();
    for (std::size_t i = 0; i < s.c_.size(); ++i) {
        const int cell = w.cell_index(s.c_.x[i], s.c_.y[i]);
        s.biomass_[cell] += s.c_.pheno[i].mass;
        s.scratch_cells_.push_back(cell);
    }
    return sim;
}

std::string snapshot_bytes(const Simulation& sim) { return SnapshotIO::to_bytes(sim); }
std::unique_ptr<Simulation> snapshot_from_bytes(const std::string& bytes, std::string& error, const LawChanges& changes) {
    return SnapshotIO::from_bytes(bytes, error, changes);
}

bool save_snapshot(const Simulation& sim, const std::string& path, std::string& error) {
    const std::string bytes = snapshot_bytes(sim);
    // Written beside the target and moved into place, so a crash never leaves half a file.
    const std::string tmp = path + ".part";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) {
            error = "cannot write " + tmp;
            return false;
        }
        f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!f) {
            error = "cannot write " + tmp;
            return false;
        }
    }
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        error = "cannot move " + tmp + " to " + path;
        return false;
    }
    return true;
}

std::unique_ptr<Simulation> load_snapshot(const std::string& path, std::string& error, const LawChanges& changes) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        error = "cannot read " + path;
        return nullptr;
    }
    std::ostringstream all;
    all << f.rdbuf();
    auto sim = snapshot_from_bytes(all.str(), error, changes);
    if (!sim) error = path + ": " + error;
    return sim;
}

}  // namespace luca
