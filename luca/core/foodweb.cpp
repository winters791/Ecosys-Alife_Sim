#include "foodweb.hpp"

#include <algorithm>

#include "sim.hpp"

namespace luca {

FoodWeb food_web(const Simulation& sim, uint32_t min_alive) {
    FoodWeb web;
    const auto& all = sim.species().all();
    std::vector<int> node_of(all.size(), -1);
    for (const Species& s : all)
        if (s.alive() && s.population >= std::max(min_alive, 1u)) {
            node_of[s.id] = static_cast<int>(web.nodes.size());
            web.nodes.push_back({s.id, s.population, 1.0, 0.0});
        }

    // The ledger holds faded sums; FADE every 50 ticks makes a sum of about
    // 50 / (1 - FADE) ticks' worth, so this gives energy a tick.
    const double per_tick = (1.0 - FoodLedger::FADE) / 50.0;
    sim.food_ledger().each([&](uint64_t key, double e) {
        const uint32_t to = static_cast<uint32_t>(key >> 32);
        const auto kind = static_cast<FoodKind>((key >> 30) & 3u);
        const uint32_t from = static_cast<uint32_t>(key & 0x3FFFFFFFu);
        if (to >= node_of.size() || node_of[to] < 0) return;
        if (kind != FoodKind::Source && (from >= node_of.size() || node_of[from] < 0)) return;
        web.edges.push_back({kind, from, to, e * per_tick});
        web.nodes[static_cast<std::size_t>(node_of[to])].intake += e * per_tick;
    });
    const auto& sources = sim.food_ledger().source;
    for (std::size_t to = 0; to < sources.size() && to < node_of.size(); ++to) {
        if (node_of[to] < 0) continue;
        for (uint32_t from = 0; from < 4; ++from) {
            const double e = sources[to][from] * per_tick;
            if (e <= 0.0) continue;
            web.edges.push_back({FoodKind::Source, from, static_cast<uint32_t>(to), e});
            web.nodes[static_cast<std::size_t>(node_of[to])].intake += e;
        }
    }

    // Trophic levels: one above the energy-weighted mean of what it eats. The
    // sun and vents are 0; the dead, leavings and loose chemicals 1 (detritus).
    // Iterated to settle; loops (cannibals, two species eating each other)
    // converge because every source has weight under 1.
    for (int round = 0; round < 60; ++round) {
        std::vector<double> sum(web.nodes.size(), 0.0);
        for (const FoodWebEdge& e : web.edges) {
            double from_level = 0.0;
            if (e.kind == FoodKind::Living) from_level = web.nodes[static_cast<std::size_t>(node_of[e.from])].level;
            else if (e.kind == FoodKind::Leavings) from_level = 1.0;
            else from_level = (e.from == static_cast<uint32_t>(FoodSource::Sun) || e.from == static_cast<uint32_t>(FoodSource::Vent)) ? 0.0 : 1.0;
            sum[static_cast<std::size_t>(node_of[e.to])] += e.energy * from_level;
        }
        for (std::size_t k = 0; k < web.nodes.size(); ++k)
            web.nodes[k].level = web.nodes[k].intake > 0.0 ? 1.0 + sum[k] / web.nodes[k].intake : 1.0;
    }

    std::sort(web.nodes.begin(), web.nodes.end(), [](const FoodWebNode& a, const FoodWebNode& b) {
        return a.intake != b.intake ? a.intake > b.intake : a.species < b.species;
    });
    std::sort(web.edges.begin(), web.edges.end(), [](const FoodWebEdge& a, const FoodWebEdge& b) {
        if (a.energy != b.energy) return a.energy > b.energy;
        if (a.to != b.to) return a.to < b.to;
        if (a.kind != b.kind) return a.kind < b.kind;
        return a.from < b.from;
    });
    return web;
}

}  // namespace luca
