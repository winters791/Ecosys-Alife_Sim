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
    out.resize(static_cast<int64_t>(a.size()) * 2);
    float* f = out.ptrw();
    for (std::size_t i = 0; i < a.size(); ++i) { f[i * 2] = a[i]; f[i * 2 + 1] = b[i]; }
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
    auto part = [](const luca::Phenotype& p, luca::PartType t) { return p.part_total[static_cast<int>(t)]; };

    for (std::size_t i = 0; i < n; ++i, f += STRIDE) {
        const luca::Genome&    g = c.genome[i];
        const luca::Phenotype& p = c.pheno[i];

        // Rows of the 3x4 transform; see the header for what sits where.
        f[0] = g.core_radius;                    f[1] = part(p, luca::PartType::Legs);
        f[2] = part(p, luca::PartType::Storage); f[3] = c.x[i];
        f[4] = part(p, luca::PartType::Absorber); f[5] = part(p, luca::PartType::Sensor);
        f[6] = c.hx[i];                          f[7] = species_hue(c.species[i]);
        f[8] = part(p, luca::PartType::Flagellum); f[9] = part(p, luca::PartType::Armour);
        f[10] = c.hy[i];                         f[11] = c.y[i];

        // Colour channel: raw traits. The shader decides how they look.
        f[12] = std::clamp(g.pigment, 0.0f, 1.0f);
        f[13] = p.surface_area > 0.0f ? std::clamp(p.light_capture / p.surface_area, 0.0f, 1.0f) : 0.0f;
        f[14] = p.repro_threshold > 0.0f ? std::clamp(c.energy[i] / p.repro_threshold, 0.0f, 1.0f) : 0.0f;
        f[15] = p.has_brain ? std::clamp(p.max_speed * 4.0f, 0.0f, 1.0f) : 0.0f;

        // Custom data.
        f[16] = hash01(c.id[i]);
        f[17] = g.shape[0];
        f[18] = g.shape[1];
        f[19] = part(p, luca::PartType::Poison);
    }
    return out;
}

namespace {

constexpr int STRIDE3D = 20;

struct V3 { float x, y, z; };
V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 operator*(V3 a, float k) { return {a.x * k, a.y * k, a.z * k}; }
V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
V3 normalized(V3 a) {
    const float l = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
    return l > 1e-6f ? a * (1.0f / l) : V3{0.0f, 1.0f, 0.0f};
}

// One MultiMesh instance: basis columns (bx, by, bz), origin, colour, custom.
void emit(std::vector<float>& out, V3 bx, V3 by, V3 bz, V3 o, const float colour[4], const float custom[4]) {
    const float f[STRIDE3D] = {bx.x, by.x, bz.x, o.x, bx.y, by.y, bz.y, o.y, bx.z, by.z, bz.z, o.z,
                               colour[0], colour[1], colour[2], colour[3], custom[0], custom[1], custom[2], custom[3]};
    out.insert(out.end(), f, f + STRIDE3D);
}

PackedFloat32Array packed(const std::vector<float>& v) { return to_packed(v); }

}  // namespace

// How a body is laid out, in "body units": a sphere of radius 1 stretched by
// its shape, facing +Z, centred one half-height above the ground it stands on.
// The numbers here are the art direction; none of them feeds back into the sim.
Dictionary LucaWorld::get_creature_meshes(double height_scale) const {
    Dictionary out;
    std::vector<float> body, shadow, eye, leg, tail, leaf, mouth, horn;
    if (sim_) {
        const luca::Creatures& c = sim_->creatures();
        const luca::World& w = sim_->world();
        const std::size_t n = c.size();
        body.reserve(n * STRIDE3D);
        shadow.reserve(n * STRIDE3D);
        auto part = [](const luca::Phenotype& p, luca::PartType t) { return p.part_total[static_cast<int>(t)]; };
        const float hs = static_cast<float>(height_scale);

        for (std::size_t i = 0; i < n; ++i) {
            const luca::Genome& g = c.genome[i];
            const luca::Phenotype& p = c.pheno[i];
            const float full = p.repro_threshold > 0.0f ? std::clamp(c.energy[i] / p.repro_threshold, 0.0f, 1.0f) : 0.0f;
            const float legs = part(p, luca::PartType::Legs), flag = part(p, luca::PartType::Flagellum);
            const float sensor = part(p, luca::PartType::Sensor), armour = part(p, luca::PartType::Armour);
            const float absorber = part(p, luca::PartType::Absorber), store = part(p, luca::PartType::Storage);
            const float photo = p.surface_area > 0.0f ? std::clamp(p.light_capture / p.surface_area, 0.0f, 1.0f) : 0.0f;

            // Size and proportions. Fat is visible: a full body is plump.
            const float R = 0.5f * g.core_radius * (0.85f + 0.25f * full);
            const float fat = std::min(store, 2.0f) * 0.12f;
            const float sx = std::clamp(1.0f + 0.22f * g.shape[0] + fat, 0.7f, 1.5f);
            const float sy = std::clamp(1.0f - 0.18f * g.shape[0] + 0.14f * g.shape[1], 0.65f, 1.35f);
            const float sz = std::clamp(1.0f + 0.25f * g.shape[1] + fat, 0.7f, 1.5f);

            // Facing: sim heading (hx, hy) is Godot (X, Z).
            V3 fwd = normalized({c.hx[i], 0.0f, c.hy[i]});
            if (std::abs(fwd.y) > 0.5f) fwd = {0.0f, 0.0f, 1.0f};
            const V3 up{0.0f, 1.0f, 0.0f};
            const V3 right = cross(up, fwd);
            const V3 ground{c.x[i], smooth_height(w, c.x[i], c.y[i]) * hs, c.y[i]};

            const bool mover = p.has_brain && p.max_speed > 0.0f;
            const float going = mover ? std::clamp(c.moved[i] / std::max(p.max_speed, 1e-4f), 0.0f, 1.0f) : 0.0f;
            const float lift = legs > 0.0f ? R * (0.22f + 0.2f * std::min(legs, 2.0f)) : 0.0f;   // legs hold it off the ground
            const float phase = hash01(c.id[i]);
            const float hue = species_hue(c.species[i]);
            const float pigment = std::clamp(g.pigment, 0.0f, 1.0f);
            const float sleep = c.sleep[i];
            const float custom[4] = {phase, going, lift, R};
            // -1 lift tells the shaders "this one hops": it moves with a tail and no legs.
            const float hop_custom[4] = {phase, going, (mover && legs <= 0.0f) ? -1.0f : lift, R};
            const float* cu = (mover && legs <= 0.0f) ? hop_custom : custom;

            // A point on the body, given in body units.
            const V3 centre = ground + up * (R * sy);
            auto at = [&](float px, float py, float pz) {
                return centre + right * (px * sx * R) + up * (py * sy * R) + fwd * (pz * sz * R);
            };

            {   // The body itself.
                const float colour[4] = {hue, pigment, full, sleep};
                const float bc[4] = {cu[0], cu[1], cu[2], p.toxicity};
                emit(body, right * (R * sx), up * (R * sy), fwd * (R * sz), centre, colour, bc);
                const float sc[4] = {hue, pigment, 0.0f, sleep};
                // The shadow lies along the slope it falls on.
                const float e = 0.5f;
                const V3 slope = normalized({(smooth_height(w, c.x[i] - e, c.y[i]) - smooth_height(w, c.x[i] + e, c.y[i])) * hs, 2.0f * e,
                                             (smooth_height(w, c.x[i], c.y[i] - e) - smooth_height(w, c.x[i], c.y[i] + e)) * hs});
                const V3 sr = normalized(cross(slope, fwd));
                const V3 sf = cross(sr, slope);
                emit(shadow, sr * (R * sx * 1.4f), slope, sf * (R * sz * 1.4f), ground + slope * 0.04f, sc, cu);
            }

            // Eyes: sensors. One small eye, or a pair when the sensor is large.
            if (sensor > 0.0f) {
                const float e = R * (0.17f + 0.09f * std::min(sensor, 2.0f));
                const float colour[4] = {hue, pigment, e, sleep};
                const int eyes = sensor >= 0.5f ? 2 : 1;
                for (int k = 0; k < eyes; ++k) {
                    const float side = eyes == 1 ? 0.0f : (k == 0 ? -0.36f : 0.36f);
                    const V3 d = normalized({side, 0.32f, 0.88f});
                    emit(eye, right * e, up * e, fwd * e, at(d.x, d.y, d.z), colour, cu);
                }
            }

            // A blind body still gets a face, so it reads as a creature: two small
            // dark dots (colour.b = -1 tells the shader these are not real eyes).
            if (sensor <= 0.0f) {
                const float e = R * 0.085f;
                const float colour[4] = {hue, pigment, -1.0f, sleep};
                for (int k = 0; k < 2; ++k) {
                    const V3 d = normalized({k == 0 ? -0.3f : 0.3f, 0.22f, 0.92f});
                    emit(eye, right * e, up * e, fwd * e, at(d.x, d.y, d.z), colour, cu);
                }
            }

            // Legs: pairs down both sides, more of them for bigger legs.
            if (legs > 0.0f) {
                const int pairs = std::clamp(1 + static_cast<int>(legs * 1.5f), 1, 3);
                const float thick = R * 0.2f;
                const float hip = -0.45f;                                   // body units below the middle
                const float length = lift + (1.0f + hip) * sy * R;          // from hip to the ground
                for (int k = 0; k < pairs; ++k) {
                    const float along = pairs == 1 ? 0.0f : -0.55f + 1.1f * static_cast<float>(k) / static_cast<float>(pairs - 1);
                    for (int s = -1; s <= 1; s += 2) {
                        const float leads = static_cast<float>((k + (s > 0 ? 1 : 0)) & 1);   // neighbours step in turn
                        const float colour[4] = {hue, pigment, leads, sleep};
                        emit(leg, right * thick, up * length, fwd * thick, at(0.7f * static_cast<float>(s), hip, along), colour, cu);
                    }
                }
            }

            // Tail: the flagellum, trailing behind and a little up.
            if (flag > 0.0f) {
                const float length = R * (0.7f + 0.5f * std::min(flag, 2.0f));
                const float thick = R * 0.16f;
                const V3 back = normalized(fwd * -1.0f + up * 0.25f);
                const float colour[4] = {hue, pigment, flag, sleep};
                emit(tail, right * thick, back * length, cross(right, back) * thick, at(0.0f, -0.05f, -0.92f), colour, cu);
            }

            // Leaves: photosynthetic patches, sprouting from the top.
            if (photo > 0.02f) {
                const int leaves = std::clamp(1 + static_cast<int>(photo * 3.0f), 1, 3);
                const float size = R * (0.55f + 0.45f * photo);
                const float colour[4] = {hue, pigment, photo, sleep};
                for (int k = 0; k < leaves; ++k) {
                    const float turn = 2.0943951f * static_cast<float>(k) + phase * 6.2831853f;   // spread round the crown
                    const V3 lean = normalized(up + (right * std::cos(turn) + fwd * std::sin(turn)) * (leaves == 1 ? 0.15f : 0.6f));
                    const V3 side = normalized(cross(lean, right * std::sin(turn) + fwd * -std::cos(turn)));
                    const V3 d = normalized({std::cos(turn) * 0.22f, 1.0f, std::sin(turn) * 0.22f});
                    emit(leaf, side * (size * 0.5f), lean * size, cross(side, lean) * (size * 0.5f), at(d.x, d.y * 0.93f, d.z), colour, cu);
                }
            }

            // Mouth: the absorber, low on the face.
            if (absorber > 0.0f) {
                const float m = R * (0.2f + 0.16f * std::min(absorber, 2.0f)) * (1.0f + 0.15f * std::min(p.bite - 1.0f, 2.0f));
                const float colour[4] = {hue, pigment, m, sleep};
                const V3 d = normalized({0.0f, -0.32f, 0.9f});
                emit(mouth, right * (m * 1.25f), up * (m * 0.75f), fwd * (m * 0.5f), at(d.x, d.y, d.z), colour, cu);
            }

            // Horns: armour, along the back.
            if (armour > 0.0f) {
                const int horns = std::clamp(2 + static_cast<int>(armour * 2.0f), 2, 5);
                const float size = R * (0.3f + 0.2f * std::min(armour, 2.0f));
                const float colour[4] = {hue, pigment, size, sleep};
                for (int k = 0; k < horns; ++k) {
                    const float t = horns == 1 ? 0.5f : static_cast<float>(k) / static_cast<float>(horns - 1);
                    const float side = (k & 1) ? 0.3f : -0.3f;
                    const V3 d = normalized({side, 0.85f, 0.45f - 1.0f * t});
                    const V3 axis = normalized(V3{d.x, d.y, d.z * 0.6f});                     // points out from the body
                    const V3 world_axis = normalized(right * axis.x + up * axis.y + fwd * axis.z);
                    const V3 a = normalized(cross(world_axis, fwd));
                    emit(horn, a * (size * 0.4f), world_axis * size, cross(a, world_axis) * (size * 0.4f), at(d.x, d.y * 0.92f, d.z), colour, cu);
                }
            }
        }
    }
    out["body"] = packed(body);
    out["shadow"] = packed(shadow);
    out["eye"] = packed(eye);
    out["leg"] = packed(leg);
    out["tail"] = packed(tail);
    out["leaf"] = packed(leaf);
    out["mouth"] = packed(mouth);
    out["horn"] = packed(horn);
    return out;
}

int LucaWorld::spawn_parade(double x, double z) {
    if (!sim_) return 0;
    using luca::PartType;
    auto wander = [](luca::Genome& g, float throttle) {
        g.brain.connect(luca::input_node(luca::IN_BIAS), luca::output_node(luca::OUT_WANDER), 1.0f);
        g.brain.connect(luca::input_node(luca::IN_BIAS), luca::output_node(luca::OUT_INERTIA), 2.0f);
        g.brain.connect(luca::input_node(luca::IN_BIAS), luca::output_node(luca::OUT_THROTTLE), throttle);
    };
    std::vector<luca::Genome> cast;
    cast.push_back(luca::make_luca());                                   // a plain sitter
    {   luca::Genome g = luca::make_luca();                              // a walker with eyes
        g.parts.push_back({PartType::Legs, 1.5f, 0.0f, 0.0f});
        g.parts.push_back({PartType::Sensor, 1.0f, 0.0f, 0.0f});
        g.parts.push_back({PartType::Absorber, 0.5f, 0.0f, 0.0f});
        wander(g, -0.42f);
        cast.push_back(g); }
    {   luca::Genome g = luca::make_luca();                              // a one-eyed hopper
        g.core_radius = 0.8f;
        g.parts.push_back({PartType::Flagellum, 1.0f, 0.0f, 0.0f});
        g.parts.push_back({PartType::Sensor, 0.3f, 0.0f, 0.0f});
        wander(g, -0.4f);
        cast.push_back(g); }
    {   luca::Genome g = luca::make_luca();                              // armoured
        g.parts.push_back({PartType::Armour, 1.5f, 0.0f, 0.0f});
        g.shape[0] = 0.6f;
        cast.push_back(g); }
    {   luca::Genome g = luca::make_luca();                              // fat and poisonous
        g.parts.push_back({PartType::Poison, 1.0f, 0.0f, 0.0f});
        g.parts.push_back({PartType::Storage, 1.5f, 0.0f, 0.0f});
        g.pigment = 0.7f;
        cast.push_back(g); }
    {   luca::Genome g = luca::make_luca();                              // a big hunter
        g.core_radius = 1.6f;
        g.parts[0].size = 0.6f;
        g.parts.push_back({PartType::Absorber, 2.0f, 0.0f, 0.0f});
        g.parts.push_back({PartType::Legs, 3.0f, 0.0f, 0.0f});
        g.parts.push_back({PartType::Sensor, 1.5f, 0.0f, 0.0f});
        g.parts.push_back({PartType::Armour, 0.5f, 0.0f, 0.0f});
        g.parts.push_back({PartType::Flagellum, 1.0f, 0.0f, 0.0f});
        g.bite = 1.0f;
        g.shape[1] = 0.8f;
        wander(g, -0.44f);
        cast.push_back(g); }
    {   luca::Genome g = luca::make_luca();                              // sleeps at night
        g.dormancy = 1.0f;
        g.parts.push_back({PartType::Sensor, 0.8f, 0.0f, 0.0f});
        cast.push_back(g); }
    for (std::size_t k = 0; k < cast.size(); ++k)
        sim_->add_creature(static_cast<float>(x) + 2.6f * static_cast<float>(k), static_cast<float>(z), 5000.0f, cast[k]);   // as full as each can hold
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
    d["radius"] = g.core_radius;
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
    d["efficiency"] = p.efficiency;
    d["absorb"] = p.absorb;
    d["membrane"] = p.membrane;
    d["sense_cells"] = p.sense_cells;
    d["repro_threshold"] = p.repro_threshold;
    d["thermal_optimum"] = p.thermal_optimum;
    d["thermal_tolerance"] = p.thermal_tolerance;
    d["fed_light"] = c.fed_light[who];
    d["fed_living"] = c.fed_living[who];
    d["fed_dead"] = c.fed_dead[who];
    d["cell_light"] = w.light(cell);
    d["cell_temperature"] = w.temperature(cell);
    d["cell_nutrient"] = sim_->soil().nutrient[cell];
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
    ClassDB::bind_method(D_METHOD("get_instance_buffer", "capacity"), &LucaWorld::get_instance_buffer);
    ClassDB::bind_method(D_METHOD("get_creature_meshes", "height_scale"), &LucaWorld::get_creature_meshes);
    ClassDB::bind_method(D_METHOD("spawn_parade", "x", "z"), &LucaWorld::spawn_parade);
    ClassDB::bind_method(D_METHOD("get_daylight"), &LucaWorld::get_daylight);
    ClassDB::bind_method(D_METHOD("get_day_phase"), &LucaWorld::get_day_phase);
    ClassDB::bind_method(D_METHOD("get_day_length"), &LucaWorld::get_day_length);
    ClassDB::bind_method(D_METHOD("get_census"), &LucaWorld::get_census);
    ClassDB::bind_method(D_METHOD("get_species_count", "min_population"), &LucaWorld::get_species_count);
    ClassDB::bind_method(D_METHOD("get_top_species", "max_count"), &LucaWorld::get_top_species);
    ClassDB::bind_method(D_METHOD("get_species_tree", "min_peak"), &LucaWorld::get_species_tree);
    ClassDB::bind_method(D_METHOD("get_nutrients"), &LucaWorld::get_nutrients);
    ClassDB::bind_method(D_METHOD("get_corpses"), &LucaWorld::get_corpses);
    ClassDB::bind_method(D_METHOD("get_soil"), &LucaWorld::get_soil);
    ClassDB::bind_method(D_METHOD("inspect_id", "id"), &LucaWorld::inspect_id);
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
