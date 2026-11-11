#pragma once
// LucaWorld: the one class Godot sees. A thin wrapper that owns a
// luca::Simulation and hands Godot flat arrays to draw.
//
// Rule 7.3.1: creatures are NOT Godot nodes. Godot asks for one packed buffer
// per frame and feeds it to a MultiMesh. All game rules stay in core/.
//
// Coordinates: sim (x, y) on the ground plane -> Godot (X, Z). The viewer is
// top-down pixel art, so height is only a field to shade the ground with.

#include <memory>

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
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
    double  get_daylight() const;      // 0 night .. 1 full day
    double  get_day_phase() const;     // 0 sunrise, 0.25 noon, 0.5 sunset, 0.75 midnight
    int     get_day_length() const;    // ticks per day; 0 = no cycle

    // Who lives how right now (producers, hunters, scavengers, mixed, young,
    // movers, sighted, armoured), counted from real energy flows.
    godot::Dictionary get_census() const;

    // Species (observation only; see core/species.hpp). The count is of
    // living species with at least `min_population` members.
    int get_species_count(int min_population) const;
    // The `max_count` most numerous living species, biggest first. Each is a
    // dictionary: id, name, population, parent (a name, or ""), born (tick),
    // hue (0..1, the same colour the "by species" view paints it).
    godot::Array get_top_species(int max_count) const;
    // The tree of life: every species that ever had `min_peak` alive at once,
    // in drawing order (each species is followed by its descendants, oldest
    // branch first). Each is a dictionary: id, name, row_parent (index into
    // this array of the nearest listed ancestor, -1 for a root), born,
    // extinct (tick, or -1 while alive), population, peak, hue.
    godot::Array get_species_tree(int min_peak) const;

    // Fields, one float per cell, row-major (index = y * size + x).
    godot::PackedFloat32Array get_heights() const;
    godot::PackedFloat32Array get_temperatures() const;
    godot::PackedFloat32Array get_lights() const;       // at noon
    godot::PackedFloat32Array get_nutrients() const;    // free matter in the soil
    godot::PackedFloat32Array get_corpses() const;      // energy left in dead bodies
    godot::PackedFloat32Array get_soil() const;         // nutrient, corpse energy, waste, interleaved: one three-channel texture
    int get_height_revision() const;
    int get_climate_revision() const;

    // MultiMesh buffer for all creatures: 20 floats each, zero-padded up to
    // `capacity` instances. A sprite needs no rotation or scale, so the nine
    // basis slots of the transform carry body data instead (blob.gdshader
    // reads them back out of MODEL_MATRIX):
    //   basis x = (core radius, absorber, flagellum)
    //   basis y = (legs, sensor, armour)
    //   basis z = (storage, heading x, heading z)
    //   origin  = (x, species hue 0..1, z)   the shader ignores height, so y is free
    //   colour  = (pigment, photo cover, energy fullness, speed)
    //   custom  = (phase, shape gene 0, shape gene 1, poison)
    godot::PackedFloat32Array get_instance_buffer(int capacity) const;

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
    // The same facts for a creature already picked, by its "id". Empty once it has died.
    godot::Dictionary inspect_id(int64_t id) const;

    // Save every power cast so far; replay it with luca_headless --script.
    bool save_powers(const godot::String& path) const;

protected:
    static void _bind_methods();

private:
    godot::Dictionary describe(std::size_t who) const;

    std::unique_ptr<luca::Simulation> sim_;
};

VARIANT_ENUM_CAST(LucaWorld::Power);
