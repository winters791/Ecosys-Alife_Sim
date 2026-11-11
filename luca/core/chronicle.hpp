#pragma once
// The natural history chronicle: a plain, append-only log of things that
// happened in this world ("Tick 4,000: the sun begins to dim...").
//
// It is written by the god powers and by event detectors, and read by the
// headless runner now and the in-game history view later. It never feeds
// back into the simulation, so it cannot affect determinism.

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace luca {

struct ChronicleEntry {
    uint64_t    tick = 0;
    std::string text;
};

class Chronicle {
public:
    void add(uint64_t tick, std::string text) { entries_.push_back({tick, std::move(text)}); }
    const std::vector<ChronicleEntry>& entries() const { return entries_; }

private:
    std::vector<ChronicleEntry> entries_;
};

// 41200 -> "41,200". Used for ticks and populations in chronicle text.
inline std::string with_commas(uint64_t v) {
    std::string s = std::to_string(v);
    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<std::size_t>(i), ",");
    return s;
}

// "Tick 41,200: <text>"
inline std::string format_entry(const ChronicleEntry& e) {
    return "Tick " + with_commas(e.tick) + ": " + e.text;
}

}  // namespace luca
