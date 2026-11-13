#pragma once
// Scenarios (design doc 4.4, 4.6; roadmap step 0b): a goal with a clock,
// checked by the sim from what it already measures. Observation only: a
// scenario changes no rule and no creature, and draws nothing from the dice.
// What it may forbid (dropping creatures by meteor) is enforced by whoever
// casts powers (the viewer), not by the sim.
//
// No influence yet: powers stay free brushes, as in Sandbox (4.6).

#include <cstdint>
#include <string>
#include <vector>

namespace luca {

struct Census;

enum class Outcome : uint32_t { Running = 0, Won = 1, Lost = 2 };

struct ScenarioInfo {
    const char* key;        // stable name, used in saves and on the command line
    const char* title;
    const char* brief;      // what to do, for the player
    uint64_t    limit;      // ticks allowed from the start; 0 = none
    bool        meteors;    // may the player drop creatures (the seed power)?
};

// Every scenario there is. Index 0 is "none" (Sandbox).
const std::vector<ScenarioInfo>& scenario_list();
int find_scenario(const std::string& key);   // -1 if unknown

// One world's scenario: which, since when, and how it stands.
struct ScenarioState {
    uint32_t id = 0;              // index into scenario_list(); 0 = none
    uint64_t start_tick = 0;
    Outcome  outcome = Outcome::Running;
    uint64_t end_tick = 0;        // when it was won or lost
    std::string verdict;          // one line for the player once it is decided

    bool active() const { return id != 0; }
    const ScenarioInfo& info() const { return scenario_list()[id]; }
};

// Checks the goal against this tick's census (run with the census, every 50
// ticks). Returns true when the outcome has just been decided.
bool check_scenario(ScenarioState& s, uint64_t tick, std::size_t population, const Census& census);

// How far along the goal is, for a progress line: e.g. "3 of 10 movers".
std::string scenario_progress(const ScenarioState& s, const Census& census);

}  // namespace luca
