#include "luca_world.h"

#include <algorithm>
#include <chrono>
#include <cmath>
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

// Stable-ish random number in [0, 1) from a position. Creatures change array
// slots when others die, so the phase is tied to where they are, not to
// their index, or blobs would flicker.
float hash01(float x, float y) {
    uint32_t h = static_cast<uint32_t>(x * 97.0f) * 0x9E3779B1u ^ static_cast<uint32_t>(y * 89.0f) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    return static_cast<float>(h & 0xFFFFu) / 65536.0f;
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

void LucaWorld::setup(int size, int64_t seed, int population, bool light_gradient) {
    luca::SimConfig cfg;
    cfg.world_size = std::max(size, 2);
    cfg.seed = static_cast<uint64_t>(seed);
    cfg.initial_population = std::max(population, 0);
    cfg.light_mode = light_gradient ? luca::LightMode::Gradient : luca::LightMode::Uniform;
    sim_ = std::make_unique<luca::Simulation>(cfg);
    sim_->seed_population();
}

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

PackedFloat32Array LucaWorld::get_heights() const {
    return sim_ ? to_packed(sim_->world().heights()) : PackedFloat32Array();
}
PackedFloat32Array LucaWorld::get_temperatures() const {
    return sim_ ? to_packed(sim_->world().temperatures()) : PackedFloat32Array();
}
PackedFloat32Array LucaWorld::get_lights() const {
    return sim_ ? to_packed(sim_->world().lights()) : PackedFloat32Array();
}
int LucaWorld::get_height_revision() const { return sim_ ? static_cast<int>(sim_->world().height_revision()) : 0; }
int LucaWorld::get_climate_revision() const { return sim_ ? static_cast<int>(sim_->world().climate_revision()) : 0; }

PackedFloat32Array LucaWorld::get_instance_buffer(int capacity, double blob_scale) const {
    constexpr int STRIDE = 20;
    PackedFloat32Array out;
    capacity = std::max(capacity, 0);
    out.resize(static_cast<int64_t>(capacity) * STRIDE);  // resize() zero-fills
    if (!sim_ || capacity == 0) return out;

    const luca::Creatures& c = sim_->creatures();
    const luca::World&     w = sim_->world();
    const std::size_t n = std::min(c.size(), static_cast<std::size_t>(capacity));
    const float scale = static_cast<float>(blob_scale);
    float* f = out.ptrw();

    for (std::size_t i = 0; i < n; ++i, f += STRIDE) {
        const luca::Genome&    g = c.genome[i];
        const luca::Phenotype& p = c.pheno[i];
        const float r = std::max(g.core_radius, 0.05f) * scale;

        // Transform3D as 3 rows of 4: uniform scale, then the position.
        // The blob sits on the ground, so its centre is one radius up.
        f[0] = r;    f[1] = 0.0f; f[2]  = 0.0f; f[3]  = c.x[i];
        f[4] = 0.0f; f[5] = r;    f[6]  = 0.0f; f[7]  = smooth_height(w, c.x[i], c.y[i]) + r * 0.8f;
        f[8] = 0.0f; f[9] = 0.0f; f[10] = r;    f[11] = c.y[i];

        // Colour channel: raw traits. The shader decides how they look.
        f[12] = std::clamp(g.pigment, 0.0f, 1.0f);
        f[13] = p.surface_area > 0.0f ? std::clamp(p.light_capture / p.surface_area, 0.0f, 1.0f) : 0.0f;
        f[14] = p.repro_threshold > 0.0f ? std::clamp(c.energy[i] / p.repro_threshold, 0.0f, 1.0f) : 0.0f;
        f[15] = std::clamp(p.max_speed * 0.5f, 0.0f, 1.0f);

        // Custom data.
        f[16] = hash01(c.x[i], c.y[i]);
        f[17] = g.shape[0];
        f[18] = g.shape[1];
        f[19] = g.shape[2];
    }
    return out;
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
    if (!sim_ || power < POWER_TERRAIN || power > POWER_CLIMATE) return;
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
    if (who == c.size()) return d;

    const luca::Genome&    g = c.genome[who];
    const luca::Phenotype& p = c.pheno[who];
    const luca::World&     w = sim_->world();
    const int cell = w.cell_index(c.x[who], c.y[who]);
    d["x"] = c.x[who];
    d["z"] = c.y[who];
    d["energy"] = c.energy[who];
    d["age"] = static_cast<int64_t>(c.age[who]);
    d["lineage"] = static_cast<int64_t>(c.lineage[who]);
    d["generation"] = static_cast<int64_t>(c.generation[who]);
    d["radius"] = g.core_radius;
    d["pigment"] = g.pigment;
    d["mass"] = p.mass;
    d["upkeep"] = p.upkeep;
    d["light_capture"] = p.light_capture;
    d["max_speed"] = p.max_speed;
    d["repro_threshold"] = p.repro_threshold;
    d["thermal_optimum"] = p.thermal_optimum;
    d["thermal_tolerance"] = p.thermal_tolerance;
    d["cell_light"] = w.light(cell);
    d["cell_temperature"] = w.temperature(cell);
    PackedStringArray parts;
    for (const luca::Part& part : g.parts)
        parts.push_back(String(luca::part_name(part.type)) + " " + String::num(part.size, 2));
    d["parts"] = parts;
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
    ClassDB::bind_method(D_METHOD("get_instance_buffer", "capacity", "blob_scale"), &LucaWorld::get_instance_buffer);
    ClassDB::bind_method(D_METHOD("height_at", "x", "z"), &LucaWorld::height_at);
    ClassDB::bind_method(D_METHOD("raycast", "origin", "direction"), &LucaWorld::raycast);
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
}
