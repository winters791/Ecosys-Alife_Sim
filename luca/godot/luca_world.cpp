#include "luca_world.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace {

PackedFloat32Array to_packed(const std::vector<float>& v) {
    PackedFloat32Array out;
    out.resize(static_cast<int64_t>(v.size()));
    if (!v.empty()) std::memcpy(out.ptrw(), v.data(), v.size() * sizeof(float));
    return out;
}

// A fixed random number in [0, 1) per creature, so each blob breathes and
// wiggles on its own beat for its whole life.
float hash01(uint64_t id) {
    uint64_t h = id * 0x9E3779B97F4A7C15ull;
    h ^= h >> 29;
    h *= 0xBF58476D1CE4E5B9ull;
    h ^= h >> 32;
    return static_cast<float>(h & 0xFFFFu) / 65536.0f;
}

// One colour per species, as a hue: steps of the golden ratio keep species
// with neighbouring ids far apart on the colour wheel.
float species_hue(uint32_t id) {
    const float h = 0.13f + static_cast<float>(id % 4096u) * 0.61803399f;
    return h - std::floor(h);
}

// One creature's sprite data, as documented in luca_world.h.
void write_sprite(float* f, const luca::Genome& g, const luca::Phenotype& p, float x, float y, float hx, float hy,
                  float sleep, float energy, float hue, float phase) {
    auto part = [&](luca::PartType t) { return p.part_total[static_cast<int>(t)]; };
    // Rows of the 3x4 transform (row-major: f[0], f[4], f[8] are basis x ...).
    // f[0] is the radius as grown so far, so the young are drawn small.
    f[0] = p.radius;        f[1] = part(luca::PartType::Storage);   f[2] = 0.0f;   f[3] = x;
    f[4] = g.elongation;    f[5] = sleep;                           f[6] = hx;     f[7] = hue;
    f[8] = g.squareness;    f[9] = static_cast<float>(g.parts.size()); f[10] = hy; f[11] = y;
    // Colour channel: raw traits. The shader decides how they look.
    f[12] = std::clamp(g.pigment, 0.0f, 1.0f);
    f[13] = p.surface_area > 0.0f ? std::clamp(p.light_capture / p.surface_area, 0.0f, 1.0f) : 0.0f;
    f[14] = p.repro_threshold > 0.0f ? std::clamp(energy / p.repro_threshold, 0.0f, 1.0f) : 0.0f;
    f[15] = p.has_brain ? std::clamp(p.max_speed * 4.0f, 0.0f, 1.0f) : 0.0f;
    // Custom data.
    f[16] = phase;
    f[17] = g.shape[0];
    f[18] = g.shape[1];
    f[19] = part(luca::PartType::Poison);
}

// One creature's parts: PARTS_PER_CREATURE texels of (type + 1, size, angle, stalk).
void write_parts(float* t, const luca::Genome& g, int per_creature) {
    const std::size_t k_max = std::min(g.parts.size(), static_cast<std::size_t>(per_creature));
    for (std::size_t k = 0; k < k_max; ++k, t += 4) {
        t[0] = static_cast<float>(static_cast<int>(g.parts[k].type) + 1);
        t[1] = g.parts[k].size;
        t[2] = g.parts[k].theta;
        t[3] = g.parts[k].stalk;
    }
}

// Bilinear height between cell centres.
float smooth_height(const luca::World& w, float x, float y) {
    const int   n  = w.size();
    const float cs = w.cell_size();
    const float fx = std::clamp(x / cs - 0.5f, 0.0f, static_cast<float>(n - 1));
    const float fy = std::clamp(y / cs - 0.5f, 0.0f, static_cast<float>(n - 1));
    const int x0 = std::min(static_cast<int>(fx), n - 2);
    const int y0 = std::min(static_cast<int>(fy), n - 2);
    const float tx = fx - static_cast<float>(x0), ty = fy - static_cast<float>(y0);
    const auto& h = w.heights();
    const float a = h[y0 * n + x0],       b = h[y0 * n + x0 + 1];
    const float c = h[(y0 + 1) * n + x0], d = h[(y0 + 1) * n + x0 + 1];
    return (a + (b - a) * tx) * (1.0f - ty) + (c + (d - c) * tx) * ty;
}

}  // namespace

// The chemicals a body can burn (and so take up from its cell), e.g. "D" or "B D".
static String burns_words(const luca::Phenotype& p) {
    String out;
    for (int k = 1; k < luca::NCHEM; ++k)
        if (p.can_burn(k)) out += String(out.is_empty() ? "" : " ") + luca::chem_name(k);
    return out.is_empty() ? String("nothing") : out;
}

void LucaWorld::setup(int size, int64_t seed, int population, bool light_gradient) {
    luca::SimConfig cfg;
    cfg.world_size = std::max(size, 2);
    cfg.seed = static_cast<uint64_t>(seed);
    cfg.initial_population = std::max(population, 0);
    cfg.light_mode = light_gradient ? luca::LightMode::Gradient : luca::LightMode::Uniform;
    sim_ = std::make_unique<luca::Simulation>(cfg);
    sim_->seed_population();
}

void LucaWorld::set_event_rate(double rate) {
    if (sim_) sim_->set_event_rate(static_cast<float>(rate));
}

double LucaWorld::get_event_rate() const { return sim_ ? sim_->event_rate() : 0.0; }

int LucaWorld::step(int max_ticks, double budget_ms) {
    if (!sim_) return 0;
    const auto start = std::chrono::steady_clock::now();
    int done = 0;
    while (done < max_ticks) {
        sim_->step();
        ++done;
        const std::chrono::duration<double, std::milli> spent = std::chrono::steady_clock::now() - start;
        if (spent.count() >= budget_ms) break;
    }
    return done;
}

int64_t LucaWorld::get_tick() const { return sim_ ? static_cast<int64_t>(sim_->tick()) : 0; }
int     LucaWorld::get_population() const { return sim_ ? static_cast<int>(sim_->creatures().size()) : 0; }
double  LucaWorld::get_tick_ms() const { return sim_ ? sim_->last_stats().tick_ms : 0.0; }
int     LucaWorld::get_world_size() const { return sim_ ? sim_->world().size() : 0; }
double  LucaWorld::get_extent() const { return sim_ ? sim_->world().extent() : 0.0; }
double  LucaWorld::get_height_limit() const { return sim_ ? sim_->config().height_limit : 0.0; }
double  LucaWorld::get_sun_scale() const { return sim_ ? sim_->world().sun_scale() : 1.0; }
double  LucaWorld::get_climate_offset() const { return sim_ ? sim_->world().climate_offset() : 0.0; }
double  LucaWorld::get_daylight() const {
    if (!sim_) return 1.0;
    return sim_->world().day_length() > 0 ? sim_->world().daylight() * 0.5 : 1.0;
}
double  LucaWorld::get_day_phase() const { return sim_ ? sim_->world().day_phase() : 0.25; }
int     LucaWorld::get_day_length() const { return sim_ ? sim_->world().day_length() : 0; }

Dictionary LucaWorld::get_census() const {
    Dictionary d;
    if (!sim_) return d;
    const luca::Census& k = sim_->census();
    d["producers"]  = static_cast<int64_t>(k.producers);
    d["hunters"]    = static_cast<int64_t>(k.hunters);
    d["scavengers"] = static_cast<int64_t>(k.scavengers);
    d["cleaners"]   = static_cast<int64_t>(k.cleaners);
    d["chemotrophs"] = static_cast<int64_t>(k.chemotrophs);
    d["mixed"]      = static_cast<int64_t>(k.mixed);
    d["young"]      = static_cast<int64_t>(k.young);
    d["movers"]     = static_cast<int64_t>(k.movers);
    d["sighted"]    = static_cast<int64_t>(k.sighted);
    d["armoured"]   = static_cast<int64_t>(k.armoured);
    d["poisonous"]  = static_cast<int64_t>(k.poisonous);
    return d;
}

int LucaWorld::get_species_count(int min_population) const {
    if (!sim_) return 0;
    int n = 0;
    for (const luca::Species& s : sim_->species().all())
        n += (s.population > 0 && s.population >= static_cast<uint32_t>(std::max(min_population, 0))) ? 1 : 0;
    return n;
}

Array LucaWorld::get_top_species(int max_count) const {
    Array out;
    if (!sim_ || max_count <= 0) return out;
    const auto& all = sim_->species().all();
    std::vector<uint32_t> ids;
    for (const luca::Species& s : all)
        if (s.alive()) ids.push_back(s.id);
    const std::size_t keep = std::min(ids.size(), static_cast<std::size_t>(max_count));
    // Biggest first; ties go to the older species, so the order is stable.
    std::partial_sort(ids.begin(), ids.begin() + static_cast<std::ptrdiff_t>(keep), ids.end(),
                      [&](uint32_t a, uint32_t b) {
                          return all[a].population != all[b].population ? all[a].population > all[b].population : a < b;
                      });
    for (std::size_t k = 0; k < keep; ++k) {
        const luca::Species& s = all[ids[k]];
        Dictionary d;
        d["id"] = static_cast<int64_t>(s.id);
        d["name"] = String::utf8(s.name.c_str());
        d["population"] = static_cast<int64_t>(s.population);
        d["parent"] = s.parent != luca::NO_SPECIES ? String::utf8(all[s.parent].name.c_str()) : String();
        d["born"] = static_cast<int64_t>(s.born_tick);
        d["hue"] = species_hue(s.id);
        out.push_back(d);
    }
    return out;
}

Array LucaWorld::get_species_tree(int min_peak) const {
    Array out;
    if (!sim_) return out;
    const auto& all = sim_->species().all();
    const uint32_t need = static_cast<uint32_t>(std::max(min_peak, 1));
    const std::size_t n = all.size();

    // Nearest ancestor that is itself listed. Parents always have smaller
    // ids, so one pass in id order is enough.
    std::vector<uint32_t> anchor(n, luca::NO_SPECIES);
    std::vector<std::vector<uint32_t>> children(n);
    std::vector<uint32_t> roots;
    for (std::size_t i = 0; i < n; ++i) {
        const uint32_t p = all[i].parent;
        if (p != luca::NO_SPECIES) anchor[i] = all[p].peak >= need ? p : anchor[p];
        if (all[i].peak < need) continue;
        if (anchor[i] == luca::NO_SPECIES) roots.push_back(static_cast<uint32_t>(i));
        else children[anchor[i]].push_back(static_cast<uint32_t>(i));   // already in birth order
    }

    // Depth-first, without recursion: a species, then its branches.
    std::vector<int64_t> row(n, -1);
    std::vector<uint32_t> stack(roots.rbegin(), roots.rend());
    while (!stack.empty()) {
        const uint32_t id = stack.back();
        stack.pop_back();
        const luca::Species& s = all[id];
        row[id] = out.size();
        Dictionary d;
        d["id"] = static_cast<int64_t>(s.id);
        d["name"] = String::utf8(s.name.c_str());
        d["row_parent"] = anchor[id] != luca::NO_SPECIES ? row[anchor[id]] : static_cast<int64_t>(-1);
        d["born"] = static_cast<int64_t>(s.born_tick);
        d["extinct"] = s.alive() ? static_cast<int64_t>(-1) : static_cast<int64_t>(s.extinct_tick);
        d["population"] = static_cast<int64_t>(s.population);
        d["peak"] = static_cast<int64_t>(s.peak);
        d["hue"] = species_hue(s.id);
        out.push_back(d);
        for (auto it = children[id].rbegin(); it != children[id].rend(); ++it) stack.push_back(*it);
    }
    return out;
}

PackedFloat32Array LucaWorld::get_heights() const {
    return sim_ ? to_packed(sim_->world().heights()) : PackedFloat32Array();
}
PackedFloat32Array LucaWorld::get_temperatures() const {
    return sim_ ? to_packed(sim_->world().temperatures()) : PackedFloat32Array();
}
PackedFloat32Array LucaWorld::get_lights() const {
    return sim_ ? to_packed(sim_->world().lights()) : PackedFloat32Array();
}
PackedFloat32Array LucaWorld::get_nutrients() const {
    return sim_ ? to_packed(sim_->soil().nutrient) : PackedFloat32Array();
}
PackedFloat32Array LucaWorld::get_corpses() const {
    return sim_ ? to_packed(sim_->soil().corpse_energy) : PackedFloat32Array();
}
PackedFloat32Array LucaWorld::get_soil() const {
    PackedFloat32Array out;
    if (!sim_) return out;
    const auto& a = sim_->soil().nutrient;
    const auto& b = sim_->soil().corpse_energy;
    const auto& w = sim_->soil().waste;
    const auto& v = sim_->world().vents();
    out.resize(static_cast<int64_t>(a.size()) * 4);
    float* f = out.ptrw();
    for (std::size_t i = 0; i < a.size(); ++i) { f[i * 4] = a[i]; f[i * 4 + 1] = b[i]; f[i * 4 + 2] = w[i]; f[i * 4 + 3] = v[i]; }
    return out;
}
int LucaWorld::get_height_revision() const { return sim_ ? static_cast<int>(sim_->world().height_revision()) : 0; }
int LucaWorld::get_climate_revision() const { return sim_ ? static_cast<int>(sim_->world().climate_revision()) : 0; }

PackedFloat32Array LucaWorld::get_instance_buffer(int capacity) const {
    constexpr int STRIDE = 20;
    PackedFloat32Array out;
    capacity = std::max(capacity, 0);
    out.resize(static_cast<int64_t>(capacity) * STRIDE);  // resize() zero-fills
    if (!sim_ || capacity == 0) return out;

    const luca::Creatures& c = sim_->creatures();
    const std::size_t n = std::min(c.size(), static_cast<std::size_t>(capacity));
    float* f = out.ptrw();
    for (std::size_t i = 0; i < n; ++i, f += STRIDE)
        write_sprite(f, c.genome[i], c.pheno[i], c.x[i], c.y[i], c.hx[i], c.hy[i], c.sleep[i], c.energy[i],
                     species_hue(c.species[i]), hash01(c.id[i]));
    return out;
}

PackedFloat32Array LucaWorld::get_part_buffer(int capacity) const {
    PackedFloat32Array out;
    capacity = std::max(capacity, 0);
    const int64_t texels = static_cast<int64_t>(capacity) * PARTS_PER_CREATURE;
    const int64_t rows = std::max<int64_t>(1, (texels + PART_TEX_WIDTH - 1) / PART_TEX_WIDTH);
    out.resize(rows * PART_TEX_WIDTH * 4);  // zero-filled: type 0 = no part
    if (!sim_) return out;
    const luca::Creatures& c = sim_->creatures();
    const std::size_t n = std::min(c.size(), static_cast<std::size_t>(capacity));
    float* f = out.ptrw();
    for (std::size_t i = 0; i < n; ++i)
        write_parts(f + static_cast<int64_t>(i) * PARTS_PER_CREATURE * 4, c.genome[i], PARTS_PER_CREATURE);
    return out;
}

int LucaWorld::spawn_parade(double x, double z) {
    if (!sim_) return 0;
    using luca::PartType;
    constexpr float PI = 3.14159265f;
    auto part = [](PartType t, float size, float theta, float stalk = 0.0f) { return luca::Part{t, size, theta, 0.0f, stalk}; };
    auto wander = [](luca::Genome& g, float throttle) {
        g.brain.connect(luca::input_node(luca::IN_BIAS), luca::output_node(luca::OUT_WANDER), 1.0f);
        g.brain.connect(luca::input_node(luca::IN_BIAS), luca::output_node(luca::OUT_INERTIA), 2.0f);
        g.brain.connect(luca::input_node(luca::IN_BIAS), luca::output_node(luca::OUT_THROTTLE), throttle);
    };
    std::vector<luca::Genome> cast;
    cast.push_back(luca::make_luca());                                   // a plain sitter
    {   luca::Genome g = luca::make_luca();                              // a long swimmer: tail behind, mouth and eyes in front
        g.elongation = 0.8f;
        g.parts.push_back(part(PartType::Flagellum, 1.2f, PI));
        g.parts.push_back(part(PartType::Absorber, 0.5f, 0.0f));
        g.parts.push_back(part(PartType::Sensor, 0.6f, 0.0f));
        wander(g, -0.3f);
        cast.push_back(g); }
    {   luca::Genome g = luca::make_luca();                              // an armoured box: plates all round but the mouth and tail
        g.elongation = 0.4f;
        g.squareness = 1.0f;
        g.parts[0].size = 0.4f;
        g.parts.push_back(part(PartType::Armour, 0.75f, PI * 0.5f));
        g.parts.push_back(part(PartType::Armour, 0.75f, -PI * 0.5f));
        g.parts.push_back(part(PartType::Absorber, 0.9f, 0.0f));
        g.parts.push_back(part(PartType::Flagellum, 1.0f, PI));
        g.parts.push_back(part(PartType::Sensor, 0.3f, 0.3f));
        g.bite = 1.0f;
        wander(g, -0.35f);
        cast.push_back(g); }
    {   luca::Genome g = luca::make_luca();                              // a tentacle monster: mouths out on stalks, armoured body
        g.core_radius = 1.3f;
        g.parts[0].size = 0.3f;
        for (int k = 0; k < 5; ++k) g.parts.push_back(part(PartType::Absorber, 0.4f, -PI * 0.8f + PI * 0.4f * k, 0.8f));
        g.parts.push_back(part(PartType::Armour, 2.0f, 0.0f));
        g.parts.push_back(part(PartType::Sensor, 0.8f, 0.0f));
        g.pigment = 0.6f;
        cast.push_back(g); }
    {   luca::Genome g = luca::make_luca();                              // a frond: sun patches held up on stalks
        g.parts[0].stalk = 0.7f;
        g.parts.push_back(part(PartType::Photo, 0.8f, PI * 0.66f, 0.9f));
        g.parts.push_back(part(PartType::Photo, 0.8f, -PI * 0.66f, 0.5f));
        g.pigment = 0.15f;
        cast.push_back(g); }
    {   luca::Genome g = luca::make_luca();                              // a walker with legs along both sides
        g.elongation = 0.5f;
        g.parts.push_back(part(PartType::Legs, 1.0f, PI * 0.35f));
        g.parts.push_back(part(PartType::Legs, 1.0f, PI * 0.7f));
        g.parts.push_back(part(PartType::Sensor, 1.0f, 0.0f));
        g.parts.push_back(part(PartType::Absorber, 0.4f, 0.0f));
        wander(g, -0.42f);
        cast.push_back(g); }
    {   luca::Genome g = luca::make_luca();                              // fat and poisonous on one side
        g.parts.push_back(part(PartType::Poison, 1.0f, PI));
        g.parts.push_back(part(PartType::Storage, 1.5f, 0.0f));
        g.pigment = 0.7f;
        g.squareness = 0.5f;
        cast.push_back(g); }
    for (std::size_t k = 0; k < cast.size(); ++k)
        sim_->add_creature(static_cast<float>(x) + 3.2f * static_cast<float>(k), static_cast<float>(z),
                          0.8f * cast[k].repro_threshold, cast[k]);   // well fed, but not yet ready to divide
    return static_cast<int>(cast.size());
}

double LucaWorld::height_at(double x, double z) const {
    return sim_ ? smooth_height(sim_->world(), static_cast<float>(x), static_cast<float>(z)) : 0.0;
}

Vector3 LucaWorld::raycast(Vector3 origin, Vector3 direction) const {
    const Vector3 miss(0.0f, -1e9f, 0.0f);
    if (!sim_ || direction.length_squared() < 1e-12f) return miss;
    const luca::World& w = sim_->world();
    const float ext = w.extent();
    const Vector3 dir = direction.normalized();

    // March along the ray in half-cell steps until it dips under the terrain,
    // then bisect for a precise hit. A few hundred samples per click.
    const float step = w.cell_size() * 0.5f;
    const float max_dist = ext * 4.0f + 2000.0f;
    auto above = [&](float t) {
        const Vector3 p = origin + dir * t;
        return static_cast<float>(p.y) - smooth_height(w, static_cast<float>(p.x), static_cast<float>(p.z));
    };
    auto inside = [&](float t) {
        const Vector3 p = origin + dir * t;
        return p.x >= 0.0f && p.x <= ext && p.z >= 0.0f && p.z <= ext;
    };

    float prev = 0.0f;
    bool  prev_ok = inside(0.0f) && above(0.0f) > 0.0f;
    for (float t = step; t < max_dist; t += step) {
        if (!inside(t)) { prev_ok = false; prev = t; continue; }
        if (above(t) <= 0.0f) {
            float lo = prev_ok ? prev : t, hi = t;
            for (int k = 0; k < 16 && prev_ok; ++k) {
                const float mid = 0.5f * (lo + hi);
                (above(mid) > 0.0f ? lo : hi) = mid;
            }
            Vector3 hit = origin + dir * hi;
            hit.y = smooth_height(w, static_cast<float>(hit.x), static_cast<float>(hit.z));
            return hit;
        }
        prev_ok = true;
        prev = t;
    }
    return miss;
}

void LucaWorld::cast_power(int power, double x, double z, double radius, double amount, int duration, bool quiet) {
    if (!sim_ || power < POWER_TERRAIN || power > POWER_VENT) return;
    if (!std::isfinite(x) || !std::isfinite(z) || !std::isfinite(radius) || !std::isfinite(amount)) return;
    luca::PowerCommand cmd;
    cmd.tick = sim_->tick();
    cmd.kind = static_cast<luca::PowerKind>(power);
    cmd.x = static_cast<float>(x);
    cmd.y = static_cast<float>(z);
    cmd.radius = static_cast<float>(radius);
    cmd.amount = static_cast<float>(amount);
    cmd.duration = static_cast<uint32_t>(std::max(duration, 0));
    cmd.quiet = quiet;
    sim_->cast(cmd);
    sim_->flush_powers();  // visible at once, even while paused
}

int LucaWorld::get_chronicle_size() const {
    return sim_ ? static_cast<int>(sim_->chronicle().entries().size()) : 0;
}

PackedStringArray LucaWorld::get_chronicle(int from) const {
    PackedStringArray out;
    if (!sim_) return out;
    const auto& entries = sim_->chronicle().entries();
    for (std::size_t i = static_cast<std::size_t>(std::max(from, 0)); i < entries.size(); ++i)
        out.push_back(String::utf8(luca::format_entry(entries[i]).c_str()));
    return out;
}

void LucaWorld::follow_species(int64_t species, bool on) {
    if (sim_ && species >= 0) sim_->follow(static_cast<uint32_t>(species), on);
}

bool LucaWorld::is_following(int64_t species) const {
    return sim_ && species >= 0 && sim_->following(static_cast<uint32_t>(species));
}

Array LucaWorld::get_followed() const {
    Array out;
    if (!sim_) return out;
    for (uint32_t id : sim_->followed()) {
        const luca::Species& s = sim_->species().get(id);
        Dictionary d;
        d["id"] = static_cast<int64_t>(s.id);
        d["name"] = String::utf8(s.name.c_str());
        d["population"] = static_cast<int64_t>(s.population);
        d["hue"] = species_hue(s.id);
        d["alive"] = s.alive();
        out.push_back(d);
    }
    return out;
}

int LucaWorld::get_alerts_size() const {
    return sim_ ? static_cast<int>(sim_->alerts().entries().size()) : 0;
}

PackedStringArray LucaWorld::get_alerts(int from) const {
    PackedStringArray out;
    if (!sim_) return out;
    const auto& entries = sim_->alerts().entries();
    for (std::size_t i = static_cast<std::size_t>(std::max(from, 0)); i < entries.size(); ++i)
        out.push_back(String::utf8(luca::format_entry(entries[i]).c_str()));
    return out;
}

Dictionary LucaWorld::inspect(double x, double z, double max_distance) const {
    Dictionary d;
    if (!sim_) return d;
    const luca::Creatures& c = sim_->creatures();
    const float px = static_cast<float>(x), py = static_cast<float>(z);
    float best = static_cast<float>(max_distance * max_distance);
    std::size_t who = c.size();
    // A linear scan is fine here: it runs once per click, not per tick.
    for (std::size_t i = 0; i < c.size(); ++i) {
        const float dx = c.x[i] - px, dy = c.y[i] - py;
        const float d2 = dx * dx + dy * dy;
        if (d2 < best) { best = d2; who = i; }
    }
    return who == c.size() ? d : describe(who);
}

Dictionary LucaWorld::inspect_id(int64_t id) const {
    if (!sim_) return Dictionary();
    const auto& ids = sim_->creatures().id;
    for (std::size_t i = 0; i < ids.size(); ++i)
        if (ids[i] == static_cast<uint64_t>(id)) return describe(i);
    return Dictionary();
}

Dictionary LucaWorld::describe(std::size_t who) const {
    Dictionary d;
    const luca::Creatures& c = sim_->creatures();
    const luca::Genome&    g = c.genome[who];
    const luca::Phenotype& p = c.pheno[who];
    const luca::World&     w = sim_->world();
    const int cell = w.cell_index(c.x[who], c.y[who]);
    d["id"] = static_cast<int64_t>(c.id[who]);
    d["x"] = c.x[who];
    d["z"] = c.y[who];
    d["energy"] = c.energy[who];
    d["energy_capacity"] = p.energy_capacity;
    d["age"] = static_cast<int64_t>(c.age[who]);
    d["lineage"] = static_cast<int64_t>(c.lineage[who]);
    d["generation"] = static_cast<int64_t>(c.generation[who]);
    const luca::Species& sp = sim_->species().get(c.species[who]);
    d["species"] = String::utf8(sp.name.c_str());
    d["species_parent"] = sp.parent != luca::NO_SPECIES ? String::utf8(sim_->species().get(sp.parent).name.c_str()) : String();
    d["species_population"] = static_cast<int64_t>(sp.population);
    d["species_hue"] = species_hue(sp.id);
    d["species_id"] = static_cast<int64_t>(sp.id);
    d["species_followed"] = sim_->following(sp.id);
    d["radius"] = p.radius;   // as grown so far
    d["grown"] = p.grown;
    d["birth_size"] = p.birth_size;
    d["pigment"] = g.pigment;
    d["mass"] = p.mass;
    d["upkeep"] = p.upkeep;
    d["light_capture"] = p.light_capture;
    d["max_speed"] = p.has_brain ? p.max_speed : 0.0f;
    d["throw_range"] = p.throw_range;
    d["bite"] = p.bite;
    d["toxicity"] = p.toxicity;
    d["dormancy"] = p.dormancy;
    d["sleep"] = c.sleep[who];
    d["brain_neurons"] = p.has_brain ? static_cast<int64_t>(c.brain[who].hidden) : static_cast<int64_t>(-1);
    d["brain_links"] = static_cast<int64_t>(c.brain[who].links);
    // Each hidden neuron: its time constant, and how it is firing right now.
    PackedFloat32Array taus, firing;
    for (int k = 0; k < c.brain[who].hidden; ++k) {
        taus.push_back(1.0f / c.brain[who].rate[k]);
        firing.push_back(std::tanh(c.brain_state[who].hidden[k] + c.brain[who].bias[k]));
    }
    d["brain_taus"] = taus;
    d["brain_firing"] = firing;
    d["efficiency"] = p.efficiency;
    d["absorb"] = p.absorb;
    d["membrane"] = p.membrane;
    d["sense_cells"] = p.sense_cells;
    d["repro_threshold"] = p.repro_threshold;
    d["thermal_optimum"] = p.thermal_optimum;
    d["thermal_tolerance"] = p.thermal_tolerance;
    d["body_temperature"] = c.body_temp[who];
    d["fed_light"] = c.fed_light[who];
    d["fed_living"] = c.fed_living[who];
    d["fed_dead"] = c.fed_dead[who];
    d["fed_waste"] = c.fed_waste[who];
    d["fed_vent"] = c.fed_vent[who];
    d["chemo"] = p.chemo;
    d["burns"] = burns_words(p);
    d["mutability"] = g.mutability;
    d["repair"] = g.repair;
    // Chemistry (stage 1): its enzymes, and what its store and body hold.
    PackedStringArray enzymes;
    for (const luca::Enzyme& e : g.enzymes)
        enzymes.push_back(String(luca::chem_name(e.x)) + "<->" + luca::chem_name(e.y) + " " + String::num(e.rate, 3));
    d["enzymes"] = enzymes;
    String held, body;
    for (int k = 1; k < luca::NCHEM; ++k) {
        if (c.pool[who].v[k] > 0.005f) held += String(held.is_empty() ? "" : " ") + luca::chem_name(k) + " " + String::num(c.pool[who].v[k], 2);
        if (c.tissue[who].v[k] > 0.005f) body += String(body.is_empty() ? "" : " ") + luca::chem_name(k) + " " + String::num(c.tissue[who].v[k], 2);
    }
    d["store_chemicals"] = held;
    d["body_chemicals"] = body;
    d["cell_waste"] = sim_->soil().waste[cell];
    d["cell_light"] = w.light(cell);
    d["cell_temperature"] = w.temperature(cell);
    d["cell_nutrient"] = sim_->soil().nutrient[cell];
    d["cell_vent"] = w.vent_energy(cell);
    // Where each part sits, in words: eight directions from the front.
    static const char* const SIDES[8] = { "front", "front-left", "left", "back-left", "back", "back-right", "right", "front-right" };
    PackedStringArray parts;
    for (const luca::Part& part : g.parts) {
        const int octant = static_cast<int>(std::lround(part.theta / (3.14159265f / 4.0f))) & 7;
        String line = String(luca::part_name(part.type)) + " " + String::num(part.size, 2) + " " + SIDES[octant];
        if (part.stalk > 0.0f) line += " on a " + String::num(part.stalk, 2) + " stalk";
        parts.push_back(line);
    }
    d["parts"] = parts;
    d["elongation"] = g.elongation;
    d["squareness"] = g.squareness;
    return d;
}

Dictionary LucaWorld::get_species_profile(int64_t species_id) const {
    Dictionary d;
    if (!sim_ || species_id < 0 || species_id >= static_cast<int64_t>(sim_->species().all().size())) return d;
    const auto& reg = sim_->species();
    const luca::Species& s = reg.get(static_cast<uint32_t>(species_id));
    const luca::Creatures& c = sim_->creatures();
    const auto& all = reg.all();

    d["id"] = static_cast<int64_t>(s.id);
    d["name"] = String::utf8(s.name.c_str());
    d["hue"] = species_hue(s.id);
    d["parent"] = s.parent != luca::NO_SPECIES ? String::utf8(all[s.parent].name.c_str()) : String();
    d["parent_id"] = s.parent != luca::NO_SPECIES ? static_cast<int64_t>(s.parent) : static_cast<int64_t>(-1);
    d["born"] = static_cast<int64_t>(s.born_tick);
    d["extinct"] = s.alive() ? static_cast<int64_t>(-1) : static_cast<int64_t>(s.extinct_tick);
    d["population"] = static_cast<int64_t>(s.population);
    d["peak"] = static_cast<int64_t>(s.peak);
    d["ever_born"] = static_cast<int64_t>(s.ever_born);
    d["died_eaten"] = static_cast<int64_t>(s.died_eaten);
    d["died_other"] = static_cast<int64_t>(s.died_other);
    d["kills"] = static_cast<int64_t>(s.kills);
    d["tick"] = static_cast<int64_t>(sim_->tick());
    int daughters = 0;
    for (const luca::Species& o : all) daughters += (o.parent == s.id && o.established) ? 1 : 0;
    d["daughters"] = daughters;

    // The living members: the best of them may beat the record book, and
    // the most remarkable are drawn.
    std::size_t eldest = SIZE_MAX, deadliest = SIZE_MAX, fertile = SIZE_MAX, any = SIZE_MAX, other = SIZE_MAX;
    for (std::size_t i = 0; i < c.size(); ++i) {
        if (c.species[i] != s.id) continue;
        if (any == SIZE_MAX) any = i; else if (other == SIZE_MAX) other = i;
        if (eldest == SIZE_MAX || c.age[i] > c.age[eldest]) eldest = i;
        if (c.kills[i] > 0 && (deadliest == SIZE_MAX || c.kills[i] > c.kills[deadliest])) deadliest = i;
        if (c.children[i] > 0 && (fertile == SIZE_MAX || c.children[i] > c.children[fertile])) fertile = i;
    }
    auto record = [&](const luca::Record& r, std::size_t live, const std::vector<uint32_t>& field) {
        Dictionary out;
        const bool alive_best = live != SIZE_MAX && field[live] > r.value;
        const uint64_t who = alive_best ? c.id[live] : r.who;
        out["value"] = static_cast<int64_t>(alive_best ? field[live] : r.value);
        out["who"] = static_cast<int64_t>(who);
        out["name"] = String::utf8(luca::creature_name(who).c_str());
        out["alive"] = alive_best;
        out["tick"] = static_cast<int64_t>(alive_best ? sim_->tick() : r.tick);
        return out;
    };
    d["eldest"] = record(s.eldest, eldest, c.age);
    d["deadliest"] = record(s.deadliest, deadliest, c.kills);
    d["most_children"] = record(s.most_children, fertile, c.children);

    PackedInt64Array specimens;
    PackedStringArray specimen_names;
    for (std::size_t i : {eldest, deadliest, fertile, any, other}) {
        if (i == SIZE_MAX || specimens.size() >= 3) continue;
        const int64_t id = static_cast<int64_t>(c.id[i]);
        if (specimens.has(id)) continue;
        specimens.push_back(id);
        specimen_names.push_back(String::utf8(luca::creature_name(c.id[i]).c_str()));
    }
    d["specimens"] = specimens;
    d["specimen_names"] = specimen_names;
    if (eldest != SIZE_MAX) {
        d["eldest_alive_name"] = String::utf8(luca::creature_name(c.id[eldest]).c_str());
        d["eldest_alive_age"] = static_cast<int64_t>(c.age[eldest]);
    }

    // What its members are like.
    const luca::Habits& h = s.habits;
    Dictionary hd;
    hd["count"] = static_cast<int64_t>(h.count);
    hd["tick"] = static_cast<int64_t>(h.tick);
    hd["light_food"] = h.diet[0];
    hd["living_food"] = h.diet[1];
    hd["dead_food"] = h.diet[2];
    hd["waste_food"] = h.diet[3];
    hd["vent_food"] = h.diet[4];
    hd["movers"] = h.movers;
    hd["sighted"] = h.sighted;
    hd["sleepers"] = h.sleepers;
    hd["armoured"] = h.armoured;
    hd["poisonous"] = h.poisonous;
    hd["radius"] = h.radius;
    hd["age"] = h.age;
    hd["speed"] = h.speed;
    hd["light"] = h.light;
    hd["temperature"] = h.temperature;
    hd["height"] = h.height;
    hd["x"] = h.x;
    hd["z"] = h.y;
    hd["spread"] = h.spread;
    d["habits"] = hd;

    // The living world, for comparisons ("colder than most"): the ground
    // everything alive stands on, not the empty cold corners of the map.
    const luca::World& w = sim_->world();
    double mt = 0.0, ml = 0.0, mh = 0.0, radius_sum = 0.0;
    for (std::size_t i = 0; i < c.size(); ++i) {
        const int cell = w.cell_index(c.x[i], c.y[i]);
        mt += w.temperatures()[cell];
        ml += w.lights()[cell];
        mh += w.heights()[cell];
        radius_sum += c.genome[i].core_radius;
    }
    const double alive_n = static_cast<double>(std::max<std::size_t>(c.size(), 1));
    d["world_temperature"] = mt / alive_n;
    d["world_light"] = ml / alive_n;
    d["world_height"] = mh / alive_n;
    d["world_radius"] = c.size() > 0 ? radius_sum / alive_n : 1.0;
    d["world_extent"] = w.extent();

    // Its body: the eldest living member's, or the founder's.
    const bool from_living = eldest != SIZE_MAX;
    const luca::Genome& g = from_living ? c.genome[eldest] : s.representative;
    const luca::Phenotype p = from_living ? c.pheno[eldest] : luca::compile_phenotype(g);
    Dictionary b;
    b["from"] = from_living ? "eldest" : "founder";
    b["radius"] = g.core_radius;
    b["pigment"] = g.pigment;
    b["elongation"] = g.elongation;
    b["squareness"] = g.squareness;
    b["thermal_optimum"] = g.thermal_optimum;
    b["thermal_tolerance"] = g.thermal_tolerance;
    b["repro_threshold"] = p.repro_threshold;
    b["offspring_share"] = g.offspring_share;
    b["bite"] = g.bite;
    b["efficiency"] = p.efficiency;
    b["burns"] = burns_words(p);
    b["chemo"] = g.chemo;
    b["dormancy"] = g.dormancy;
    b["throw_range"] = p.throw_range;
    b["mutability"] = g.mutability;
    b["repair"] = g.repair;
    b["toxicity"] = p.toxicity;
    b["mass"] = p.mass;
    b["upkeep"] = p.upkeep;
    b["energy_capacity"] = p.energy_capacity;
    b["max_speed"] = p.has_brain ? p.max_speed : 0.0f;
    b["sense_cells"] = p.sense_cells;
    b["canopy_lift"] = p.canopy - g.core_radius;
    b["brain_neurons"] = p.has_brain ? static_cast<int64_t>(g.brain.hidden) : static_cast<int64_t>(-1);
    b["brain_links"] = static_cast<int64_t>(g.brain.links.size());
    Array parts;
    for (const luca::Part& part : g.parts) {
        Dictionary pd;
        pd["type"] = static_cast<int64_t>(part.type);
        pd["name"] = String(luca::part_name(part.type));
        pd["size"] = part.size;
        pd["theta"] = part.theta;
        pd["stalk"] = part.stalk;
        parts.push_back(pd);
    }
    b["parts"] = parts;
    d["body"] = b;

    // The food web as far as killing goes: whom it finishes off, and who finishes it off.
    std::vector<std::pair<uint64_t, uint32_t>> prey, predators;
    uint64_t lost_to_others = 0;
    for (const auto& [key, count] : reg.kill_web()) {
        if (key.first == s.id && key.second != s.id) prey.emplace_back(count, key.second);
        if (key.second == s.id && key.first != s.id) { predators.emplace_back(count, key.first); lost_to_others += count; }
        if (key.first == s.id && key.second == s.id) d["cannibal_kills"] = static_cast<int64_t>(count);
    }
    auto top = [&](std::vector<std::pair<uint64_t, uint32_t>>& v) {
        std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
        Array out;
        for (std::size_t k = 0; k < std::min<std::size_t>(v.size(), 5); ++k) {
            Dictionary e;
            const luca::Species& o = all[v[k].second];
            e["id"] = static_cast<int64_t>(o.id);
            e["name"] = String::utf8(o.name.c_str());
            e["hue"] = species_hue(o.id);
            e["alive"] = o.alive();
            e["kills"] = static_cast<int64_t>(v[k].first);
            out.push_back(e);
        }
        return out;
    };
    d["prey_species"] = static_cast<int64_t>(prey.size());
    d["predator_species"] = static_cast<int64_t>(predators.size());
    d["prey"] = top(prey);
    d["predators"] = top(predators);
    d["lost_to_others"] = static_cast<int64_t>(lost_to_others);
    if (!d.has("cannibal_kills")) d["cannibal_kills"] = static_cast<int64_t>(0);
    return d;
}

Dictionary LucaWorld::get_sprites(const PackedInt64Array& ids, int64_t species_id) const {
    constexpr int STRIDE = 20;
    Dictionary d;
    PackedFloat32Array inst, parts, speeds;
    PackedInt64Array drawn;
    if (!sim_) return d;
    const luca::Creatures& c = sim_->creatures();
    for (int64_t k = 0; k < ids.size(); ++k) {
        for (std::size_t i = 0; i < c.size(); ++i) {
            if (static_cast<int64_t>(c.id[i]) != ids[k]) continue;
            const int64_t at = inst.size();
            inst.resize(at + STRIDE);
            write_sprite(inst.ptrw() + at, c.genome[i], c.pheno[i], 0.0f, 0.0f, c.hx[i], c.hy[i], c.sleep[i], c.energy[i],
                         species_hue(c.species[i]), hash01(c.id[i]));
            const int64_t pt = parts.size();
            parts.resize(pt + PARTS_PER_CREATURE * 4);
            float* t = parts.ptrw() + pt;
            std::fill(t, t + PARTS_PER_CREATURE * 4, 0.0f);
            write_parts(t, c.genome[i], PARTS_PER_CREATURE);
            speeds.push_back(c.pheno[i].has_brain ? c.pheno[i].max_speed : 0.0f);
            drawn.push_back(ids[k]);
            break;
        }
    }
    const bool fossil = drawn.is_empty() && species_id >= 0 && species_id < static_cast<int64_t>(sim_->species().all().size());
    if (fossil) {
        const luca::Species& s = sim_->species().get(static_cast<uint32_t>(species_id));
        const luca::Phenotype p = luca::compile_phenotype(s.representative);
        inst.resize(STRIDE);
        write_sprite(inst.ptrw(), s.representative, p, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, p.repro_threshold * 0.6f,
                     species_hue(s.id), hash01(s.id));
        parts.resize(PARTS_PER_CREATURE * 4);
        std::fill(parts.ptrw(), parts.ptrw() + PARTS_PER_CREATURE * 4, 0.0f);
        write_parts(parts.ptrw(), s.representative, PARTS_PER_CREATURE);
        speeds.push_back(0.0f);
    }
    d["instances"] = inst;
    d["parts"] = parts;
    d["speeds"] = speeds;
    d["ids"] = drawn;
    d["fossil"] = fossil;
    return d;
}

bool LucaWorld::save_powers(const String& path) const {
    if (!sim_) return false;
    std::ofstream f(path.utf8().get_data());
    if (!f) return false;
    luca::write_power_script(f, sim_->powers().log());
    return static_cast<bool>(f);
}

void LucaWorld::_bind_methods() {
    ClassDB::bind_method(D_METHOD("setup", "size", "seed", "population", "light_gradient"), &LucaWorld::setup);
    ClassDB::bind_method(D_METHOD("step", "max_ticks", "budget_ms"), &LucaWorld::step);
    ClassDB::bind_method(D_METHOD("get_tick"), &LucaWorld::get_tick);
    ClassDB::bind_method(D_METHOD("get_population"), &LucaWorld::get_population);
    ClassDB::bind_method(D_METHOD("get_tick_ms"), &LucaWorld::get_tick_ms);
    ClassDB::bind_method(D_METHOD("get_world_size"), &LucaWorld::get_world_size);
    ClassDB::bind_method(D_METHOD("get_extent"), &LucaWorld::get_extent);
    ClassDB::bind_method(D_METHOD("get_height_limit"), &LucaWorld::get_height_limit);
    ClassDB::bind_method(D_METHOD("get_sun_scale"), &LucaWorld::get_sun_scale);
    ClassDB::bind_method(D_METHOD("get_climate_offset"), &LucaWorld::get_climate_offset);
    ClassDB::bind_method(D_METHOD("get_heights"), &LucaWorld::get_heights);
    ClassDB::bind_method(D_METHOD("get_temperatures"), &LucaWorld::get_temperatures);
    ClassDB::bind_method(D_METHOD("get_lights"), &LucaWorld::get_lights);
    ClassDB::bind_method(D_METHOD("get_height_revision"), &LucaWorld::get_height_revision);
    ClassDB::bind_method(D_METHOD("get_climate_revision"), &LucaWorld::get_climate_revision);
    ClassDB::bind_method(D_METHOD("get_instance_buffer", "capacity"), &LucaWorld::get_instance_buffer);
    ClassDB::bind_method(D_METHOD("get_part_buffer", "capacity"), &LucaWorld::get_part_buffer);
    ClassDB::bind_method(D_METHOD("get_part_tex_width"), &LucaWorld::get_part_tex_width);
    ClassDB::bind_method(D_METHOD("get_parts_per_creature"), &LucaWorld::get_parts_per_creature);
    ClassDB::bind_method(D_METHOD("spawn_parade", "x", "z"), &LucaWorld::spawn_parade);
    ClassDB::bind_method(D_METHOD("get_daylight"), &LucaWorld::get_daylight);
    ClassDB::bind_method(D_METHOD("get_day_phase"), &LucaWorld::get_day_phase);
    ClassDB::bind_method(D_METHOD("get_day_length"), &LucaWorld::get_day_length);
    ClassDB::bind_method(D_METHOD("get_census"), &LucaWorld::get_census);
    ClassDB::bind_method(D_METHOD("get_species_count", "min_population"), &LucaWorld::get_species_count);
    ClassDB::bind_method(D_METHOD("get_top_species", "max_count"), &LucaWorld::get_top_species);
    ClassDB::bind_method(D_METHOD("get_species_tree", "min_peak"), &LucaWorld::get_species_tree);
    ClassDB::bind_method(D_METHOD("get_species_profile", "species_id"), &LucaWorld::get_species_profile);
    ClassDB::bind_method(D_METHOD("get_sprites", "ids", "species_id"), &LucaWorld::get_sprites);
    ClassDB::bind_method(D_METHOD("get_nutrients"), &LucaWorld::get_nutrients);
    ClassDB::bind_method(D_METHOD("get_corpses"), &LucaWorld::get_corpses);
    ClassDB::bind_method(D_METHOD("get_soil"), &LucaWorld::get_soil);
    ClassDB::bind_method(D_METHOD("inspect_id", "id"), &LucaWorld::inspect_id);
    ClassDB::bind_method(D_METHOD("follow_species", "species", "on"), &LucaWorld::follow_species);
    ClassDB::bind_method(D_METHOD("is_following", "species"), &LucaWorld::is_following);
    ClassDB::bind_method(D_METHOD("get_followed"), &LucaWorld::get_followed);
    ClassDB::bind_method(D_METHOD("get_alerts_size"), &LucaWorld::get_alerts_size);
    ClassDB::bind_method(D_METHOD("get_alerts", "from"), &LucaWorld::get_alerts);
    ClassDB::bind_method(D_METHOD("height_at", "x", "z"), &LucaWorld::height_at);
    ClassDB::bind_method(D_METHOD("raycast", "origin", "direction"), &LucaWorld::raycast);
    ClassDB::bind_method(D_METHOD("set_event_rate", "rate"), &LucaWorld::set_event_rate);
    ClassDB::bind_method(D_METHOD("get_event_rate"), &LucaWorld::get_event_rate);
    ClassDB::bind_method(D_METHOD("cast_power", "power", "x", "z", "radius", "amount", "duration", "quiet"),
                         &LucaWorld::cast_power);
    ClassDB::bind_method(D_METHOD("get_chronicle_size"), &LucaWorld::get_chronicle_size);
    ClassDB::bind_method(D_METHOD("get_chronicle", "from"), &LucaWorld::get_chronicle);
    ClassDB::bind_method(D_METHOD("inspect", "x", "z", "max_distance"), &LucaWorld::inspect);
    ClassDB::bind_method(D_METHOD("save_powers", "path"), &LucaWorld::save_powers);

    BIND_ENUM_CONSTANT(POWER_TERRAIN);
    BIND_ENUM_CONSTANT(POWER_HEAT);
    BIND_ENUM_CONSTANT(POWER_SUN);
    BIND_ENUM_CONSTANT(POWER_CLIMATE);
    BIND_ENUM_CONSTANT(POWER_VENT);
}
