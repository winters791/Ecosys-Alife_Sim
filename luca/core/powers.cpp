#include "powers.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <istream>
#include <ostream>
#include <sstream>

#include "creature_file.hpp"

namespace luca {

namespace {

// Ramps are applied in steps this many ticks apart, not every tick: a field
// refresh touches many cells, and nothing alive can tell the difference.
constexpr uint64_t RAMP_STEP = 8;

bool is_brush(PowerKind k) { return k == PowerKind::Terrain || k == PowerKind::Heat || k == PowerKind::Vent; }
// More bodies than this in one meteor is a typo, not a power.
constexpr float MAX_SEED_COUNT = 500.0f;

const char* kind_name(PowerKind k) {
    switch (k) {
        case PowerKind::Terrain: return "terrain";
        case PowerKind::Heat:    return "heat";
        case PowerKind::Sun:     return "sun";
        case PowerKind::Climate: return "climate";
        case PowerKind::Vent:    return "vent";
        case PowerKind::Seed:    return "seed";
    }
    return "?";
}

std::string num(float v, int decimals = 1) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.*f", decimals, static_cast<double>(std::abs(v)));
    return buf;
}

}  // namespace

// Which ninth of the map a point is in. y = 0 is the sunny edge, so "south".
std::string region_name(const World& w, float x, float y) {
    const float third = w.extent() / 3.0f;
    const int col = std::clamp(static_cast<int>(x / third), 0, 2);
    const int row = std::clamp(static_cast<int>(y / third), 0, 2);
    static const char* const names[3][3] = {
        {"the south-west", "the south", "the south-east"},
        {"the west", "the heart of the world", "the east"},
        {"the north-west", "the north", "the north-east"},
    };
    return names[row][col];
}

namespace {

std::string describe_start(const PowerCommand& c, const World& w) {
    const bool up = c.amount >= 0.0f;
    const std::string over =
        c.duration > 0 ? " over " + with_commas(c.duration) + " ticks" : std::string();
    switch (c.kind) {
        case PowerKind::Terrain:
            if (c.duration == 0)  // a brush stroke: its size is whatever the player paints
                return std::string(up ? "The land is raised in " : "The land is lowered in ") +
                       region_name(w, c.x, c.y) + ".";
            return std::string(up ? "The land begins to rise in " : "The land begins to sink in ") +
                   region_name(w, c.x, c.y) + " (" + num(c.amount) + " high, " + num(c.radius, 0) +
                   " wide)" + over + ".";
        case PowerKind::Heat:
            if (c.duration == 0)
                return std::string(up ? "A warm spot forms in " : "A cold spot forms in ") +
                       region_name(w, c.x, c.y) + ".";
            return std::string(up ? "A warm spot begins to form in " : "A cold spot begins to form in ") +
                   region_name(w, c.x, c.y) + " (" + num(c.amount) + " °C)" + over + ".";
        case PowerKind::Vent:
            if (c.duration == 0)
                return std::string(up ? "A vent opens in " : "A vent closes in ") + region_name(w, c.x, c.y) + ".";
            return std::string(up ? "A vent begins to open in " : "A vent begins to close in ") +
                   region_name(w, c.x, c.y) + over + ".";
        case PowerKind::Sun:
            if (c.duration == 0)
                return std::string(up ? "The sun brightens" : "The sun dims") + " by " + num(c.amount, 2) + "x.";
            return std::string(up ? "The sun begins to brighten" : "The sun begins to dim") +
                   " by " + num(c.amount, 2) + "x" + over + ".";
        case PowerKind::Climate:
            if (c.amount <= -10.0f)
                return "An ice age begins: the world cools by " + num(c.amount) + " °C" + over + ".";
            if (c.duration == 0)
                return std::string(up ? "The world warms" : "The world cools") + " by " + num(c.amount) + " °C.";
            return std::string(up ? "The world begins to warm" : "The world begins to cool") +
                   " by " + num(c.amount) + " °C" + over + ".";
        case PowerKind::Seed:
            return {};   // the Simulation says where it lands and what came with it
    }
    return {};
}

// Only ramps get an "it's over" entry; an instant power needs no epilogue.
std::string describe_end(const PowerCommand& c, const World& w) {
    switch (c.kind) {
        case PowerKind::Terrain:
            return "The land in " + region_name(w, c.x, c.y) + " comes to rest.";
        case PowerKind::Heat:
            return "The " + std::string(c.amount >= 0.0f ? "warm" : "cold") + " spot in " +
                   region_name(w, c.x, c.y) + " stops changing.";
        case PowerKind::Vent:
            return "The vent in " + region_name(w, c.x, c.y) + std::string(c.amount >= 0.0f ? " is fully open." : " has closed.");
        case PowerKind::Sun:
            if (std::abs(w.sun_scale() - 1.0f) < 0.005f) return "The sun is back to its original strength.";
            return "The sun settles at " + num(w.sun_scale(), 2) + "x its original strength.";
        case PowerKind::Climate: {
            const float o = w.climate_offset();
            if (std::abs(o) < 0.05f) return "The climate is back to where it began.";
            return "The climate settles " + num(o) + " °C " + (o >= 0.0f ? "above" : "below") +
                   " where it began.";
        }
        case PowerKind::Seed:
            return {};
    }
    return {};
}

}  // namespace

void PowerSystem::cast(const PowerCommand& cmd) {
    log_.push_back(cmd);
    // Keep pending_ sorted by tick; equal ticks stay in cast order.
    const auto pos = std::upper_bound(
        pending_.begin(), pending_.end(), cmd,
        [](const PowerCommand& a, const PowerCommand& b) { return a.tick < b.tick; });
    pending_.insert(pos, cmd);
}

void PowerSystem::apply(const PowerCommand& cmd, float amount, World& world) {
    switch (cmd.kind) {
        case PowerKind::Terrain: world.brush(BrushField::Height, cmd.x, cmd.y, cmd.radius, amount); break;
        case PowerKind::Heat:    world.brush(BrushField::Heat, cmd.x, cmd.y, cmd.radius, amount); break;
        case PowerKind::Vent:    world.brush(BrushField::Vent, cmd.x, cmd.y, cmd.radius, amount); break;
        case PowerKind::Sun:     world.add_sun(amount); break;
        case PowerKind::Climate: world.add_climate(amount); break;
        case PowerKind::Seed:    break;   // landed by the Simulation (take_arrivals)
    }
}

void PowerSystem::update(uint64_t tick, World& world, Chronicle& chronicle) {
    if (idle()) return;  // the common case: nothing to do

    // Start everything that is due.
    std::size_t due = 0;
    while (due < pending_.size() && pending_[due].tick <= tick) {
        const PowerCommand& cmd = pending_[due++];
        if (cmd.kind == PowerKind::Seed) {
            arrivals_.push_back(cmd);
            continue;
        }
        if (!cmd.quiet) chronicle.add(tick, describe_start(cmd, world));
        active_.push_back({cmd, tick, 0.0f});
    }
    pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(due));

    // Advance ramps, in the order they started.
    std::size_t keep = 0;
    for (std::size_t i = 0; i < active_.size(); ++i) {
        Active a = active_[i];
        const uint64_t elapsed = tick - a.start + 1;  // ticks covered, counting this one
        const bool done = elapsed >= a.cmd.duration;
        if (done || (tick - a.start) % RAMP_STEP == 0) {
            const float target = done ? 1.0f
                                      : static_cast<float>(elapsed) / static_cast<float>(a.cmd.duration);
            apply(a.cmd, a.cmd.amount * (target - a.applied), world);
            a.applied = target;
        }
        if (done) {
            if (a.cmd.duration > 0 && !a.cmd.quiet) chronicle.add(tick, describe_end(a.cmd, world));
        } else {
            active_[keep++] = a;
        }
    }
    active_.resize(keep);
}

bool parse_power_script(std::istream& in, std::vector<PowerCommand>& out, std::string& error) {
    std::string line;
    int line_no = 0;
    while (std::getline(in, line)) {
        ++line_no;
        if (const auto hash = line.find('#'); hash != std::string::npos) line.erase(hash);
        std::istringstream ss(line);
        std::string first;
        if (!(ss >> first)) continue;  // blank or comment-only line

        auto fail = [&](const std::string& why) {
            error = "line " + std::to_string(line_no) + ": " + why;
            return false;
        };

        PowerCommand cmd;
        std::string kind;
        try {
            std::size_t used = 0;
            cmd.tick = std::stoull(first, &used);
            if (used != first.size() || first[0] == '-') return fail("bad tick '" + first + "'");
        } catch (...) {
            return fail("bad tick '" + first + "'");
        }
        if (!(ss >> kind)) return fail("missing power name");

        if      (kind == "terrain") cmd.kind = PowerKind::Terrain;
        else if (kind == "heat")    cmd.kind = PowerKind::Heat;
        else if (kind == "sun")     cmd.kind = PowerKind::Sun;
        else if (kind == "climate") cmd.kind = PowerKind::Climate;
        else if (kind == "vent")    cmd.kind = PowerKind::Vent;
        else if (kind == "seed")    cmd.kind = PowerKind::Seed;
        else return fail("unknown power '" + kind + "' (terrain, heat, vent, sun, climate, seed)");

        if (cmd.kind == PowerKind::Seed) {
            // The cargo runs to the end of the line, so read it from the raw
            // line (a creature file never holds '#': clean_text drops it).
            if (!(ss >> cmd.x >> cmd.y >> cmd.radius >> cmd.amount))
                return fail("seed needs: x y radius count creature");
            if (!std::isfinite(cmd.x) || !std::isfinite(cmd.y) || !std::isfinite(cmd.radius) || cmd.radius < 0.0f)
                return fail("bad seed position or radius");
            if (!(cmd.amount >= 1.0f && cmd.amount <= MAX_SEED_COUNT)) return fail("seed count must be 1 to 500");
            std::getline(ss, cmd.creature);
            cmd.creature.erase(0, cmd.creature.find_first_not_of(" \t"));
            CreatureFile f;
            std::string why;
            if (!read_creature_file(cmd.creature, f, why)) return fail("bad creature: " + why);
            out.push_back(cmd);
            continue;
        }

        if (is_brush(cmd.kind)) {
            if (!(ss >> cmd.x >> cmd.y >> cmd.radius >> cmd.amount))
                return fail(kind + " needs: x y radius amount [duration]");
            if (cmd.radius <= 0.0f) return fail("radius must be positive");
        } else {
            if (!(ss >> cmd.amount)) return fail(kind + " needs: amount [duration]");
        }
        if (!std::isfinite(cmd.x) || !std::isfinite(cmd.y) || !std::isfinite(cmd.radius) ||
            !std::isfinite(cmd.amount))
            return fail("numbers must be finite");

        // Optional tail: [duration] [quiet]
        std::string word;
        if (ss >> word && word != "quiet") {
            try {
                std::size_t used = 0;
                const long long duration = std::stoll(word, &used);
                if (used != word.size() || duration < 0 || duration > 0xFFFFFFFFll)
                    return fail("bad duration '" + word + "'");
                cmd.duration = static_cast<uint32_t>(duration);
            } catch (...) {
                return fail("bad duration '" + word + "'");
            }
            word.clear();
            ss >> word;
        }
        if (word == "quiet") {
            cmd.quiet = true;
            word.clear();
            ss >> word;
        }
        if (!word.empty()) return fail("unexpected '" + word + "'");

        out.push_back(cmd);
    }
    return true;
}

void write_power_script(std::ostream& out, const std::vector<PowerCommand>& commands) {
    out << "# tick power args... [duration]   (see core/powers.hpp)\n";
    char buf[160];
    for (const PowerCommand& c : commands) {
        // %.9g round-trips a float exactly, so a replay sees the same numbers.
        if (c.kind == PowerKind::Seed) {
            std::snprintf(buf, sizeof buf, "%llu seed %.9g %.9g %.9g %.9g ", static_cast<unsigned long long>(c.tick),
                          static_cast<double>(c.x), static_cast<double>(c.y), static_cast<double>(c.radius),
                          static_cast<double>(c.amount));
            out << buf << c.creature << '\n';
            continue;
        }
        if (is_brush(c.kind))
            std::snprintf(buf, sizeof buf, "%llu %s %.9g %.9g %.9g %.9g %u",
                          static_cast<unsigned long long>(c.tick), kind_name(c.kind),
                          static_cast<double>(c.x), static_cast<double>(c.y),
                          static_cast<double>(c.radius), static_cast<double>(c.amount), c.duration);
        else
            std::snprintf(buf, sizeof buf, "%llu %s %.9g %u",
                          static_cast<unsigned long long>(c.tick), kind_name(c.kind),
                          static_cast<double>(c.amount), c.duration);
        out << buf << (c.quiet ? " quiet\n" : "\n");
    }
}

}  // namespace luca
