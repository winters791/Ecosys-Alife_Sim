#pragma once
// LucaWorld: the one class Godot sees. A thin wrapper that owns a
// luca::Simulation and hands Godot flat arrays to draw.
//
// Rule 7.3.1: creatures are NOT Godot nodes. Godot asks for one packed buffer
// per frame and feeds it to a MultiMesh. All game rules stay in core/.
//
// Coordinates: sim (x, y) on the ground plane -> Godot (X, Z); height -> Y.

#include <memory>

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "sim.hpp"

class LucaWorld : public godot::RefCounted {
    GDCLASS(LucaWorld, godot::RefCounted)

public:
    // Power ids for cast_power(); must match luca::PowerKind.
    enum Power { POWER_TERRAIN = 0, POWER_HEAT = 1, POWER_SUN = 2, POWER_CLIMATE = 3 };

    // (Re)create the world and scatter the first LUCAs.
    void setup(int size, int64_t seed, int population, bool light_gradient);

    // Run up to `max_ticks` ticks, stopping early once `budget_ms` of real
    // time is spent, so a crowded world slows down instead of freezing the
    // window (7.1). Returns the number of ticks actually run.
    int step(int max_ticks, double budget_ms);

    int64_t get_tick() const;
    int     get_population() const;
    double  get_tick_ms() const;       // wall-clock cost of the last tick
    int     get_world_size() const;    // cells per side
    double  get_extent() const;        // world units per side
    double  get_height_limit() const;
    double  get_sun_scale() const;
    double  get_climate_offset() const;

    // Fields, one float per cell, row-major (index = y * size + x).
    godot::PackedFloat32Array get_heights() const;
    godot::PackedFloat32Array get_temperatures() const;
    godot::PackedFloat32Array get_lights() const;
    int get_height_revision() const;
    int get_climate_revision() const;

    // MultiMesh buffer for all creatures: 20 floats each (3x4 transform,
    // colour, custom data), zero-padded up to `capacity` instances.
    //   colour = (pigment, photo cover, energy fullness, speed)
    //   custom = (random phase, shape gene 0, shape gene 1, shape gene 2)
    godot::PackedFloat32Array get_instance_buffer(int capacity, double blob_scale) const;

    // Terrain height under a ground position, smoothly interpolated.
    double height_at(double x, double z) const;

    // Where a ray hits the terrain. Returns Vector3(0, -1e9, 0) on a miss.
    godot::Vector3 raycast(godot::Vector3 origin, godot::Vector3 direction) const;

    // Cast a god power at the current tick. x, z, radius are ignored for the
    // global powers (sun, climate). duration 0 = instant.
    void cast_power(int power, double x, double z, double radius, double amount, int duration, bool quiet);

    // Chronicle lines from entry `from` onwards, already formatted.
    int get_chronicle_size() const;
    godot::PackedStringArray get_chronicle(int from) const;

    // The creature nearest to a ground position (within `max_distance`), as a
    // dictionary of facts for the inspector. Empty if there is none.
    godot::Dictionary inspect(double x, double z, double max_distance) const;

    // Save every power cast so far; replay it with luca_headless --script.
    bool save_powers(const godot::String& path) const;

protected:
    static void _bind_methods();

private:
    std::unique_ptr<luca::Simulation> sim_;
};

VARIANT_ENUM_CAST(LucaWorld::Power);
