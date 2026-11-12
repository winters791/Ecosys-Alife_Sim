#include "events.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace luca {

namespace {

// How many of each kind happen in 100,000 ticks at rate 1.
constexpr float PER_100K[static_cast<int>(EventKind::Count)] = {
    12.0f,  // storms: a cold spell over part of the map, gone within a few days
    2.0f,   // eruptions: a new hill with a hot vent in it, cooling over weeks
    2.0f,   // meteors: a crater and a burst of heat; now and then a dark winter
    4.0f,   // climate drift: the whole world a degree or two warmer or cooler
};

// Storms: a cold spot that rolls in, sits, and rolls out.
constexpr float STORM_RADIUS_MIN = 10.0f, STORM_RADIUS_MAX = 22.0f;
constexpr float STORM_COLD_MIN   = 6.0f,  STORM_COLD_MAX   = 14.0f;   // °C at the centre
constexpr uint32_t STORM_IN = 200, STORM_OUT = 400;
constexpr uint32_t STORM_HOLD_MIN = 600, STORM_HOLD_MAX = 2400;

// Eruptions: a cone of new ground, hot rock, and a vent that slowly closes.
constexpr float ERUPT_RADIUS_MIN = 4.0f,  ERUPT_RADIUS_MAX = 7.0f;
constexpr float ERUPT_HEIGHT_MIN = 8.0f,  ERUPT_HEIGHT_MAX = 18.0f;
constexpr float ERUPT_HEAT       = 15.0f;   // °C, over 1.5 x the cone
constexpr float ERUPT_VENT       = 2.0f;    // vent strength at the crater
constexpr uint32_t ERUPT_RISE = 300;
constexpr uint32_t ERUPT_COOL_MIN = 20000, ERUPT_COOL_MAX = 40000;

// Meteors: a crater with a rim, a flash of heat; sometimes dust that dims the sun.
constexpr float METEOR_RADIUS_MIN = 2.5f, METEOR_RADIUS_MAX = 6.0f;
constexpr float METEOR_DEPTH_PER_RADIUS = 2.5f;
constexpr float METEOR_HEAT = 30.0f;
constexpr uint32_t METEOR_COOL = 3000;
constexpr float GREAT_METEOR_CHANCE = 0.2f;
constexpr float WINTER_DIM = 0.25f;          // sun multiplier lost to the dust
constexpr uint32_t WINTER_FALL = 200;
constexpr uint32_t WINTER_CLEAR_MIN = 15000, WINTER_CLEAR_MAX = 40000;

// Climate drift: a slow step, pulled back towards where the world began.
constexpr float DRIFT_SD = 1.5f;             // °C
constexpr float DRIFT_PULL = 0.3f;           // share of the current offset undone per step
constexpr float DRIFT_MAX_STEP = 3.0f;       // °C, the most one drift moves the climate
constexpr uint32_t DRIFT_MIN = 10000, DRIFT_MAX = 30000;

std::string degrees(float v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.1f", static_cast<double>(std::abs(v)));
    return buf;
}

uint32_t between(Rng& rng, uint32_t lo, uint32_t hi) { return lo + rng.below(hi - lo + 1); }

PowerCommand brush(PowerKind kind, uint64_t tick, float x, float y, float radius, float amount, uint32_t duration) {
    PowerCommand c;
    c.tick = tick;
    c.kind = kind;
    c.x = x;
    c.y = y;
    c.radius = radius;
    c.amount = amount;
    c.duration = duration;
    c.quiet = true;
    return c;
}

PowerCommand global(PowerKind kind, uint64_t tick, float amount, uint32_t duration) {
    PowerCommand c;
    c.tick = tick;
    c.kind = kind;
    c.amount = amount;
    c.duration = duration;
    c.quiet = true;
    return c;
}

}  // namespace

WorldEvents::WorldEvents(uint64_t seed, float rate)
    : rng_(seed * 0x9E3779B97F4A7C15ull + 0x5EED), rate_(rate > 0.0f ? rate : 0.0f) {}

void WorldEvents::later(uint64_t tick, std::string text) { notes_.push_back({tick, std::move(text)}); }

void WorldEvents::update(uint64_t tick, const World& world, PowerSystem& powers, Chronicle& chronicle) {
    // Lines that have come due, in the order they were scheduled.
    if (!notes_.empty()) {
        std::size_t keep = 0;
        for (std::size_t i = 0; i < notes_.size(); ++i) {
            if (notes_[i].tick <= tick) chronicle.add(tick, std::move(notes_[i].text));
            else if (keep++ != i) notes_[keep - 1] = std::move(notes_[i]);
        }
        notes_.resize(keep);
    }
    if (rate_ <= 0.0f) return;

    // One draw a tick decides whether anything happens, and which.
    float total = 0.0f;
    for (float p : PER_100K) total += p;
    const float u = rng_.uniform() * 100000.0f / rate_;
    if (u >= total) return;
    float acc = 0.0f;
    for (int k = 0; k < static_cast<int>(EventKind::Count); ++k) {
        acc += PER_100K[k];
        if (u < acc) {
            trigger(static_cast<EventKind>(k), tick, world, powers, chronicle);
            return;
        }
    }
}

void WorldEvents::trigger(EventKind kind, uint64_t tick, const World& world, PowerSystem& powers, Chronicle& chronicle) {
    ++counts_[static_cast<int>(kind)];
    const float x = rng_.uniform(0.0f, world.extent());
    const float y = rng_.uniform(0.0f, world.extent());
    const std::string where = region_name(world, x, y);

    switch (kind) {
        case EventKind::Storm: {
            const float r = rng_.uniform(STORM_RADIUS_MIN, STORM_RADIUS_MAX);
            const float cold = rng_.uniform(STORM_COLD_MIN, STORM_COLD_MAX);
            const uint32_t hold = between(rng_, STORM_HOLD_MIN, STORM_HOLD_MAX);
            powers.cast(brush(PowerKind::Heat, tick, x, y, r, -cold, STORM_IN));
            powers.cast(brush(PowerKind::Heat, tick + STORM_IN + hold, x, y, r, cold, STORM_OUT));
            chronicle.add(tick, "A cold storm gathers over " + where + ".");
            later(tick + STORM_IN + hold + STORM_OUT, "The storm over " + where + " clears.");
            break;
        }
        case EventKind::Eruption: {
            const float r = rng_.uniform(ERUPT_RADIUS_MIN, ERUPT_RADIUS_MAX);
            const float h = rng_.uniform(ERUPT_HEIGHT_MIN, ERUPT_HEIGHT_MAX);
            const uint32_t cool = between(rng_, ERUPT_COOL_MIN, ERUPT_COOL_MAX);
            powers.cast(brush(PowerKind::Terrain, tick, x, y, r, h, ERUPT_RISE));
            powers.cast(brush(PowerKind::Heat, tick, x, y, 1.5f * r, ERUPT_HEAT, ERUPT_RISE));
            powers.cast(brush(PowerKind::Vent, tick, x, y, 0.6f * r, ERUPT_VENT, ERUPT_RISE));
            // The new hill stays; the heat and the vent fade together.
            powers.cast(brush(PowerKind::Heat, tick + ERUPT_RISE, x, y, 1.5f * r, -ERUPT_HEAT, cool));
            powers.cast(brush(PowerKind::Vent, tick + ERUPT_RISE, x, y, 0.6f * r, -ERUPT_VENT, cool));
            chronicle.add(tick, "A volcano erupts in " + where + ". A hot vent opens in its crater.");
            later(tick + ERUPT_RISE + cool, "The volcano in " + where + " falls quiet.");
            break;
        }
        case EventKind::Meteor: {
            const float r = rng_.uniform(METEOR_RADIUS_MIN, METEOR_RADIUS_MAX);
            const float depth = METEOR_DEPTH_PER_RADIUS * r;
            const bool great = rng_.uniform() < GREAT_METEOR_CHANCE;
            powers.cast(brush(PowerKind::Terrain, tick, x, y, r, -depth, 0));
            powers.cast(brush(PowerKind::Terrain, tick, x, y, 1.7f * r, 0.35f * depth, 0));   // the rim
            powers.cast(brush(PowerKind::Heat, tick, x, y, 2.0f * r, METEOR_HEAT, 0));
            powers.cast(brush(PowerKind::Heat, tick + 1, x, y, 2.0f * r, -METEOR_HEAT, METEOR_COOL));
            if (great) {
                const uint32_t clear = between(rng_, WINTER_CLEAR_MIN, WINTER_CLEAR_MAX);
                powers.cast(global(PowerKind::Sun, tick, -WINTER_DIM, WINTER_FALL));
                powers.cast(global(PowerKind::Sun, tick + WINTER_FALL, WINTER_DIM, clear));
                chronicle.add(tick, "A great meteor strikes " + where + ". Dust darkens the sky.");
                later(tick + WINTER_FALL + clear, "The dust of the great impact has settled; the sun shines as before.");
            } else {
                chronicle.add(tick, "A meteor strikes " + where + ", leaving a crater.");
            }
            break;
        }
        case EventKind::ClimateDrift: {
            const float step = std::clamp(DRIFT_SD * rng_.normal() - DRIFT_PULL * world.climate_offset(), -DRIFT_MAX_STEP, DRIFT_MAX_STEP);
            const uint32_t over = between(rng_, DRIFT_MIN, DRIFT_MAX);
            powers.cast(global(PowerKind::Climate, tick, step, over));
            const std::string way = step >= 0.0f ? "warm" : "cool";
            chronicle.add(tick, "The climate begins to " + way + ".");
            later(tick + over, "The climate has " + way + "ed by " + degrees(step) + " °C.");
            break;
        }
        case EventKind::Count:
            break;
    }
}

}  // namespace luca
