#include "scenario.hpp"

#include "sim.hpp"

#include <algorithm>

namespace luca {

namespace {
// The first movers detector in the chronicle counts them at 10 alive at once
// (Simulation::detect_events); the goal uses the same bar.
constexpr std::size_t MOVERS_GOAL = 10;
}  // namespace

const std::vector<ScenarioInfo>& scenario_list() {
    static const std::vector<ScenarioInfo> list = {
        {"none", "Sandbox", "No goal. Every power is a free brush.", 0, true},
        // On its own a default world first has 10 movers between tick 9,900
        // and 30,300 (30 seeds, median about 15,900): only 2 in 30 make it in
        // time, so the player has to shape the world to win.
        {"first-steps", "First steps",
         "Get life moving: have 10 creatures able to move alive at once within 10,000 ticks. "
         "Shape the land, the sun, the heat and the vents; you can't drop creatures in.",
         10000, false},
    };
    return list;
}

int find_scenario(const std::string& key) {
    const auto& list = scenario_list();
    for (std::size_t i = 0; i < list.size(); ++i)
        if (key == list[i].key) return static_cast<int>(i);
    return -1;
}

bool check_scenario(ScenarioState& s, uint64_t tick, std::size_t population, const Census& census) {
    if (!s.active() || s.outcome != Outcome::Running) return false;
    const ScenarioInfo& info = s.info();
    const uint64_t taken = tick - s.start_tick;
    bool won = false;
    if (info.key == std::string("first-steps")) won = census.movers >= MOVERS_GOAL;
    if (won) {
        s.outcome = Outcome::Won;
        s.verdict = "Goal met: " + std::string(info.title) + ", in " + with_commas(taken) + " ticks.";
    } else if (population == 0) {
        s.outcome = Outcome::Lost;
        s.verdict = "Lost: " + std::string(info.title) + ". Life died out.";
    } else if (info.limit > 0 && taken >= info.limit) {
        s.outcome = Outcome::Lost;
        s.verdict = "Time is up: " + std::string(info.title) + " was not met in " + with_commas(info.limit) + " ticks.";
    } else {
        return false;
    }
    s.end_tick = tick;
    return true;
}

std::string scenario_progress(const ScenarioState& s, const Census& census) {
    if (!s.active()) return "";
    if (s.info().key == std::string("first-steps"))
        return std::to_string(std::min<std::size_t>(census.movers, MOVERS_GOAL)) + " of " + std::to_string(MOVERS_GOAL) + " movers";
    return "";
}

}  // namespace luca
