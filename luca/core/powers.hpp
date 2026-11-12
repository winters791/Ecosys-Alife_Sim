#pragma once
// God powers: the player's hands. Every power edits the ENVIRONMENT (terrain,
// heat, vents, sun, climate) through World's edit functions. There is no power that
// touches a creature, and there never should be.
//
// A power is a command: "at tick T, change X by `amount`, spread over
// `duration` ticks".
//   duration = 0  -> instant. This is Sandbox mode: paint and watch.
//   duration > 0  -> a linear ramp, so populations get time to track the
//                    change. This is what Scenario mode will enforce.
//
// Amounts are DELTAS, not targets, so overlapping powers simply add up and
// the result never depends on how they interleave.
//
// Every command that is cast is kept in a log. Same seed + same log = the
// same world, so a session can be replayed or shared as a tiny text file.

#include <cstdint>
#include <iosfwd>
#include <string>
#include <utility>
#include <vector>

#include "chronicle.hpp"
#include "world.hpp"

namespace luca {

enum class PowerKind : uint8_t {
    Terrain,  // brush: raise (+) or lower (-) the land, in height units
    Heat,     // brush: local warm (+) or cold (-) spot, in °C
    Sun,      // global: change the sun multiplier (0.2 = 20% brighter)
    Climate,  // global: warm (+) or cool (-) the whole world, in °C
    Vent,     // brush: open (+) or close (-) a vent, in units of strength (0..vent_max)
    // A life-bearing meteor (design doc 4.9): `amount` bodies from a creature
    // file (creature_file.hpp, kept in `creature`) land within `radius` of
    // (x, y). The one power that brings creatures: it never touches any that
    // are already there. Instant; the Simulation lands them (take_arrivals).
    Seed,
};

struct PowerCommand {
    uint64_t  tick = 0;       // when it starts
    PowerKind kind = PowerKind::Sun;
    float     x = 0.0f, y = 0.0f, radius = 0.0f;  // brush powers only (world units)
    float     amount = 0.0f;  // total change once the ramp has finished
    uint32_t  duration = 0;   // ticks to spread the change over; 0 = instant
    bool      quiet = false;  // no chronicle entry (later dabs of one brush stroke)
    std::string creature = {};   // Seed only: the cargo, as one line (creature_line)
};

class PowerSystem {
public:
    // Queue a command. A tick in the past means "start now".
    void cast(const PowerCommand& cmd);

    // Called once at the start of every tick, before creatures act.
    void update(uint64_t tick, World& world, Chronicle& chronicle);

    // Every command ever cast, in the order it was cast.
    const std::vector<PowerCommand>& log() const { return log_; }

    bool idle() const { return pending_.empty() && active_.empty(); }

    // Meteors that came due in update(), for the Simulation to land. Taking
    // them empties the list.
    std::vector<PowerCommand> take_arrivals() { return std::exchange(arrivals_, {}); }

private:
    struct Active {
        PowerCommand cmd;
        uint64_t start = 0;
        float    applied = 0.0f;  // fraction of cmd.amount already put into the world
    };

    static void apply(const PowerCommand& cmd, float amount, World& world);

    std::vector<PowerCommand> pending_;  // sorted by tick, ties in cast order
    std::vector<Active>       active_;
    std::vector<PowerCommand> log_;
    std::vector<PowerCommand> arrivals_;
};

// --- Script format -----------------------------------------------------------
// One command per line; '#' starts a comment. The last number (duration, in
// ticks) is optional and defaults to 0 = instant. A final word `quiet` keeps
// the command out of the chronicle (used for the dabs of a brush stroke).
//
//   <tick> terrain <x> <y> <radius> <amount> [duration]
//   <tick> heat    <x> <y> <radius> <amount> [duration]
//   <tick> vent    <x> <y> <radius> <amount> [duration]
//   <tick> sun     <amount> [duration]
//   <tick> climate <amount> [duration]
//   <tick> seed    <x> <y> <radius> <count> <creature file on one line>
//
// A seed line ends with its cargo (creature_line: the file with ';' between
// lines); it takes no duration and no `quiet`.
//
// Returns false and fills `error` (with a line number) on the first bad line.
bool parse_power_script(std::istream& in, std::vector<PowerCommand>& out, std::string& error);

// Writes commands back out in the same format (round-trips with the parser).
void write_power_script(std::ostream& out, const std::vector<PowerCommand>& commands);

// Which ninth of the map a point is in, in words: "the north-west".
// Also used by the species split check for where each half lives.
std::string region_name(const World& w, float x, float y);

}  // namespace luca
