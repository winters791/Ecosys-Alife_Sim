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
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "design.hpp"
#include "sim.hpp"

class LucaWorld : public godot::RefCounted {
    GDCLASS(LucaWorld, godot::RefCounted)

public:
    // Power ids for cast_power(); must match luca::PowerKind.
    enum Power { POWER_TERRAIN = 0, POWER_HEAT = 1, POWER_SUN = 2, POWER_CLIMATE = 3, POWER_VENT = 4 };

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
    // One species' profile for the species card (observation only): who it
    // is and where it came from; its record book (eldest, deadliest, most
    // young, each with a name, living members included); what its members
    // are like ("habits", from the last census, kept after extinction); its
    // body (the eldest living member's, or the founder's once extinct) with
    // every part; whom it kills and who kills it; and up to three living
    // members to draw ("specimens", with "specimen_names"). Empty for an unknown id.
    godot::Dictionary get_species_profile(int64_t species_id) const;
    // Sprite data for a few creatures by id, laid out like the two buffers
    // below ("instances": 20 floats each; "parts": PARTS_PER_CREATURE texels
    // each), each body at the origin facing the way it faces in the world.
    // Ids that have died are skipped; if none are left, the species'
    // founder is drawn instead and "fossil" is true. "speeds" holds each
    // drawn body's top speed, so the viewer can walk it about.
    godot::Dictionary get_sprites(const godot::PackedInt64Array& ids, int64_t species_id) const;

    // Fields, one float per cell, row-major (index = y * size + x).
    godot::PackedFloat32Array get_heights() const;
    godot::PackedFloat32Array get_temperatures() const;
    godot::PackedFloat32Array get_lights() const;       // at noon
    godot::PackedFloat32Array get_nutrients() const;    // free matter in the soil
    godot::PackedFloat32Array get_corpses() const;      // energy left in dead bodies
    godot::PackedFloat32Array get_soil() const;         // nutrient, corpse energy, waste, vent strength, interleaved: one four-channel texture
    int get_height_revision() const;
    int get_climate_revision() const;

    // MultiMesh buffer for all creatures: 20 floats each, zero-padded up to
    // `capacity` instances. A sprite needs no rotation or scale, so the nine
    // basis slots of the transform carry body data instead (blob.gdshader
    // reads them back out of MODEL_MATRIX):
    //   basis x = (core radius as grown so far, elongation, squareness)
    //   basis y = (storage, sleep, number of parts)
    //   basis z = (unused, heading x, heading z)
    //   origin  = (x, species hue 0..1, z)   the shader ignores height, so y is free
    //   colour  = (pigment, photo cover, energy fullness, speed)
    //   custom  = (phase, lump gene 0, lump gene 1, poison)
    godot::PackedFloat32Array get_instance_buffer(int capacity) const;

    // Every creature's parts, for the sprite shader to draw each one where it
    // sits: PARTS_PER_CREATURE texels per creature (in the same order as the
    // instance buffer), laid out in rows PART_TEX_WIDTH texels wide, RGBA float:
    //   (part type + 1, or 0 for no part; size; angle from the front; stalk length)
    static constexpr int PARTS_PER_CREATURE = 12;   // = luca::limits::MAX_PARTS
    static constexpr int PART_TEX_WIDTH = 1024;
    godot::PackedFloat32Array get_part_buffer(int capacity) const;
    int get_part_tex_width() const { return PART_TEX_WIDTH; }
    int get_parts_per_creature() const { return PARTS_PER_CREATURE; }

    // An empty patch of world filled with one hand-made creature of each body
    // plan, for checking the art. Returns how many it placed.
    int spawn_parade(double x, double z);

    // Terrain height under a ground position, smoothly interpolated.
    double height_at(double x, double z) const;

    // Where a ray hits the terrain. Returns Vector3(0, -1e9, 0) on a miss.
    godot::Vector3 raycast(godot::Vector3 origin, godot::Vector3 direction) const;

    // Cast a god power at the current tick. x, z, radius are ignored for the
    // global powers (sun, climate). duration 0 = instant.
    void cast_power(int power, double x, double z, double radius, double amount, int duration, bool quiet);

    // The world acting on its own (core/events.hpp): 1 = about one storm,
    // eruption, meteor or climate drift every 5,000 ticks; 0 = none.
    void set_event_rate(double rate);
    double get_event_rate() const;

    // Chronicle lines from entry `from` onwards, already formatted.
    int get_chronicle_size() const;
    godot::PackedStringArray get_chronicle(int from) const;

    // The creature nearest to a ground position (within `max_distance`), as a
    // dictionary of facts for the inspector. Empty if there is none.
    godot::Dictionary inspect(double x, double z, double max_distance) const;
    // The same facts for a creature already picked, by its "id". Empty once it has died.
    godot::Dictionary inspect_id(int64_t id) const;

    // Following a species (observation only; see Simulation::follow). The
    // inspector's "species_id" and "species_followed" say which and whether.
    void follow_species(int64_t species, bool on);
    bool is_following(int64_t species) const;
    // Followed species: id, name, population, hue, alive.
    godot::Array get_followed() const;
    // Alerts about followed species from entry `from` onwards, already formatted.
    int get_alerts_size() const;
    godot::PackedStringArray get_alerts(int from) const;

    // Save every power cast so far; replay it with luca_headless --script.
    bool save_powers(const godot::String& path) const;

    // --- The creature designer (viewer/designer.gd; core/design.hpp) ----------
    // Implemented in luca_design.cpp. A genome travels as a Dictionary:
    //   radius, pigment, thermal_optimum, thermal_tolerance, repro_threshold,
    //   offspring_share, birth_size, elongation, squareness, bite, dormancy,
    //   seed_throw, mutability, repair (floats); shape (8 floats);
    //   parts: [{type, size, theta, stalk}]; enzymes: [{x, y, x2, y2, rate}]
    //   (x2 = y2 = -1 for one-to-one); hidden (int), tau, bias (one float per
    //   hidden neuron); links: [{from, to, weight}] in node numbers (inputs,
    //   then outputs, then hidden neurons: see get_brain_layout).
    // Every genome coming in is clamped (clamp_genome) before it is used.

    // Node names and numbering for the brain editor, and the gene limits.
    godot::Dictionary get_brain_layout() const;
    // Starting points: [{name, genome}].
    godot::Array get_design_templates() const;
    // A genome through clamp_genome, as the world would hold it.
    godot::Dictionary clamp_design(const godot::Dictionary& genome) const;
    // The energy budget and stats of a design under `sun` (mean light a tick,
    // 1 = full) and `air` (°C): see luca::assess_design.
    godot::Dictionary assess_design(const godot::Dictionary& genome, double sun, double air) const;
    // One sprite of a design at the origin facing +x: {instances, parts}, laid
    // out like get_sprites. `fullness` 0..1 is how fed it looks.
    godot::Dictionary design_sprite(const godot::Dictionary& genome, double fullness) const;
    // What its brain answers to these inputs held for `ticks`: {outputs, hidden}.
    godot::Dictionary probe_brain(const godot::Dictionary& genome, const godot::PackedFloat32Array& inputs, int ticks) const;
    // Creature files (core/creature_file.hpp). write: genomes -> text.
    // read: text -> {ok, error, name, origin, bodies: [genome]}.
    godot::String write_creature_file(const godot::Array& genomes, const godot::String& name, const godot::String& origin) const;
    godot::Dictionary read_creature_file(const godot::String& text) const;
    // The seed vault: a sample of a species as a creature file ("" if unknown).
    godot::String bank_species(int64_t species_id) const;
    // A living creature's genome, by id (empty once it has died).
    godot::Dictionary genome_of(int64_t id) const;
    // Drop `count` bodies of a creature file as a meteor (the seed power,
    // logged like any other). Returns false if the file doesn't read.
    bool release(const godot::String& text, double x, double z, double radius, int count);

    // The test tank: a small world of its own (luca::Trial), stepped by the
    // designer while it is open. Drawn with the same buffers as the world;
    // in the instance buffer the hue is 0.08 for the design's lineage and
    // 0.42 for everything else.
    bool trial_start(const godot::String& text, int count, bool with_luca, double sun, double air, int64_t seed);
    void trial_stop();
    int trial_step(int max_ticks, double budget_ms);
    // tick, guests, natives, generations, births, extent, and the guests'
    // diet: fed_light, fed_living, fed_dead, fed_loose (sums), mean_energy.
    godot::Dictionary trial_info() const;
    godot::PackedFloat32Array trial_instance_buffer(int capacity) const;
    godot::PackedFloat32Array trial_part_buffer(int capacity) const;

protected:
    static void _bind_methods();

private:
    godot::Dictionary describe(std::size_t who) const;

    std::unique_ptr<luca::Simulation> sim_;
    std::unique_ptr<luca::Trial> trial_;
};

VARIANT_ENUM_CAST(LucaWorld::Power);
