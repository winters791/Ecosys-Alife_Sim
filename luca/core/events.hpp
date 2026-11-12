#pragma once
// A world that acts on its own (design doc 4.7): storms, eruptions, slow
// climate drift and the odd meteor, without the player.
//
// Each event is nothing but ordinary power commands (powers.hpp), cast by the
// world instead of the player, marked quiet so the chronicle gets one line per
// event written here instead of one per command. They go into the same power
// log, so a run's powers.txt replays them exactly (with events off, or they
// would happen twice). Events never touch a creature.
//
// The dice are keyed from a draw seed of their own (made from the world seed)
// and the tick (rng.hpp, draw_key), apart from the sim's: turning events off
// (rate 0) then leaves a world exactly as it was before events existed, and
// replaying a power log reproduces a run whether or not its events came from
// this class.
//
// Rate: `rate` scales a base table of how often each kind happens (events.cpp);
// 1 = about one event every 5,000 ticks, 0 = never.

#include <cstdint>
#include <string>
#include <vector>

#include "chronicle.hpp"
#include "powers.hpp"
#include "rng.hpp"
#include "world.hpp"

namespace luca {

enum class EventKind : uint8_t { Storm, Eruption, Meteor, ClimateDrift, Count };

class WorldEvents {
    friend struct SnapshotIO;   // snapshot.cpp: saves and restores the whole state
public:
    explicit WorldEvents(uint64_t seed, float rate = 0.0f);

    void  set_rate(float rate) { rate_ = rate > 0.0f ? rate : 0.0f; }
    float rate() const { return rate_; }

    // Called at the start of every tick, before the powers run: maybe start an
    // event (its commands are cast for this tick and later ones), and write any
    // chronicle lines that have come due ("the storm clears").
    void update(uint64_t tick, const World& world, PowerSystem& powers, Chronicle& chronicle);

    // Start one event of a given kind now, whatever the rate (tests, and later
    // scenario scripts). Uses the same dice.
    void trigger(EventKind kind, uint64_t tick, const World& world, PowerSystem& powers, Chronicle& chronicle);

    // How many of each kind have happened.
    uint32_t count(EventKind kind) const { return counts_[static_cast<int>(kind)]; }

private:
    struct Note {
        uint64_t    tick;
        std::string text;
    };
    void later(uint64_t tick, std::string text);
    void start(EventKind kind, uint64_t tick, Rng& rng, const World& world, PowerSystem& powers, Chronicle& chronicle);

    uint64_t seed_ = 0;   // every draw is keyed from this and the tick (rng.hpp, draw_key)
    float rate_ = 0.0f;
    std::vector<Note> notes_;   // chronicle lines still to come, in no particular order
    uint32_t counts_[static_cast<int>(EventKind::Count)] = {};
};

}  // namespace luca
