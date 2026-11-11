#include "mutation.hpp"

#include <cmath>

namespace luca {

// =============================================================================
// MUTATION
// Written by Claude on 2026-11-11 at Uday's request (it was a placeholder).
//
// Most children are near-copies: a few genes nudged. Now and then the parts
// list itself changes, and that is where new kinds of creature come from:
//   * add       a new small part of a random type
//   * delete    lose a part
//   * duplicate copy a part (two mouths, a second patch...)
//   * retype    a part turns into another kind (rare)
//
// No operator knows what is useful. A mouth on a creature that can't reach
// anything is pure cost and selection removes it.
//
// Rates are hard-coded for now; later they can become a heritable gene or a
// god-mode law. Ranges are enforced once, in clamp_genome().
// =============================================================================

namespace {

constexpr float P_GENE      = 0.20f;   // chance each ordinary gene is nudged
constexpr float P_ADD       = 0.030f;
constexpr float P_DELETE    = 0.020f;
constexpr float P_DUPLICATE = 0.020f;
constexpr float P_RETYPE    = 0.005f;
constexpr float P_WEIGHT    = 0.15f;   // chance each brain weight is nudged
constexpr float P_ADD_LINK   = 0.06f;  // a new connection between two nodes
constexpr float P_DEL_LINK   = 0.04f;  // a connection is lost
constexpr float P_ADD_NEURON = 0.015f; // a new hidden neuron grows in the middle of a link
constexpr float P_DEL_NEURON = 0.010f; // a hidden neuron dies, with its links
constexpr float P_NEW_TRAIT = 0.10f;   // of the times a missing optional trait is touched, how often it appears
constexpr float P_LOSE_BRAIN = 0.05f;  // a brain with nothing to drive withers

constexpr int NUM_TYPES = static_cast<int>(PartType::Count);

bool is_motor(PartType t) { return t == PartType::Flagellum || t == PartType::Legs; }

// Multiply by e^(noise): sizes stay positive and change in proportion.
void scale(float& v, float sigma, Rng& rng) { v *= std::exp(rng.normal() * sigma); }

// An optional trait: one a LUCA lacks (its gene is 0) and that costs upkeep
// to have at all. A plain nudge would bounce off zero and leave every lineage
// carrying a little of each, paying for traits it never uses. Instead a
// missing trait stays missing until a rarer mutation switches it on, and one
// that dwindles below a sliver is lost again.
void nudge_optional(float& v, float sigma, Rng& rng) {
    if (v <= 0.0f) {
        if (rng.chance(P_NEW_TRAIT)) v = sigma * (1.0f + std::abs(rng.normal()));
        return;
    }
    v += rng.normal() * sigma;
    if (v < 0.25f * sigma) v = 0.0f;
}

// The brain evolves the way the body does: mostly small nudges to what is
// there, and now and then a change of structure. None of these knows what is
// useful; each neuron and link costs upkeep, so what doesn't pay is lost.
void mutate_brain(Brain& b, Rng& rng) {
    for (BrainLink& l : b.links)
        if (rng.chance(P_WEIGHT)) l.weight += rng.normal() * 0.2f;

    // Lose a link.
    if (rng.chance(P_DEL_LINK) && b.links.size() > 1)
        b.links.erase(b.links.begin() + rng.below(static_cast<uint32_t>(b.links.size())));

    // Lose a neuron, and everything wired to it.
    if (rng.chance(P_DEL_NEURON) && b.hidden > 0) b.remove_hidden(static_cast<int>(rng.below(b.hidden)));

    // A new neuron grows in the middle of an existing link: a -> new -> b.
    // The old link stays, so behaviour changes only slightly at first.
    if (rng.chance(P_ADD_NEURON) && b.hidden < MAX_HIDDEN && !b.links.empty() &&
        b.links.size() + 2 <= static_cast<std::size_t>(MAX_LINKS)) {
        const BrainLink through = b.links[rng.below(static_cast<uint32_t>(b.links.size()))];
        const int fresh = hidden_node(b.hidden++);
        b.links.push_back({through.from, static_cast<uint8_t>(fresh), 1.0f});
        b.links.push_back({static_cast<uint8_t>(fresh), through.to, rng.normal() * 0.3f});
    }

    // A new link: from any input or hidden neuron, to any hidden neuron or
    // output. Hidden-to-hidden links (even a neuron to itself) are what give
    // a brain memory.
    if (rng.chance(P_ADD_LINK) && b.links.size() < static_cast<std::size_t>(MAX_LINKS)) {
        const uint32_t f = rng.below(static_cast<uint32_t>(NUM_INPUTS + b.hidden));
        const uint32_t t = rng.below(static_cast<uint32_t>(NUM_OUTPUTS + b.hidden));
        const int from = f < static_cast<uint32_t>(NUM_INPUTS) ? static_cast<int>(f) : hidden_node(static_cast<int>(f) - NUM_INPUTS);
        const int to = t < static_cast<uint32_t>(NUM_OUTPUTS) ? FIRST_OUTPUT + static_cast<int>(t) : hidden_node(static_cast<int>(t) - NUM_OUTPUTS);
        bool exists = false;
        for (const BrainLink& l : b.links) exists = exists || (l.from == from && l.to == to);
        if (!exists) b.links.push_back({static_cast<uint8_t>(from), static_cast<uint8_t>(to), rng.normal() * 0.3f});
    }
}

}  // namespace

Genome mutate(const Genome& parent, Rng& rng) {
    Genome g = parent;

    // --- Tweaks ---------------------------------------------------------------
    if (rng.chance(P_GENE)) scale(g.core_radius, 0.06f, rng);
    if (rng.chance(P_GENE)) g.pigment += rng.normal() * 0.05f;
    if (rng.chance(P_GENE)) g.thermal_optimum += rng.normal() * 1.0f;
    if (rng.chance(P_GENE)) g.thermal_tolerance += rng.normal() * 0.6f;
    if (rng.chance(P_GENE)) scale(g.repro_threshold, 0.08f, rng);
    if (rng.chance(P_GENE)) g.offspring_share += rng.normal() * 0.03f;
    if (rng.chance(P_GENE)) nudge_optional(g.seed_throw, 0.15f, rng);
    if (rng.chance(P_GENE)) nudge_optional(g.bite, 0.10f, rng);
    if (rng.chance(P_GENE)) nudge_optional(g.digestion, 0.05f, rng);
    if (rng.chance(P_GENE)) nudge_optional(g.dormancy, 0.08f, rng);
    for (float& s : g.shape)
        if (rng.chance(P_GENE * 0.5f)) s += rng.normal() * 0.15f;
    for (Part& p : g.parts)
        if (rng.chance(P_GENE)) {
            scale(p.size, 0.10f, rng);
            p.theta += rng.normal() * 0.2f;
            p.phi   += rng.normal() * 0.2f;
        }

    // --- Structure --------------------------------------------------------------
    if (rng.chance(P_DELETE) && !g.parts.empty())
        g.parts.erase(g.parts.begin() + rng.below(static_cast<uint32_t>(g.parts.size())));

    if (rng.chance(P_DUPLICATE) && !g.parts.empty() && g.parts.size() < static_cast<std::size_t>(limits::MAX_PARTS)) {
        Part copy = g.parts[rng.below(static_cast<uint32_t>(g.parts.size()))];
        copy.size *= 0.5f;                       // a new copy starts as a bud
        copy.theta += rng.normal() * 0.5f;
        g.parts.push_back(copy);
    }

    if (rng.chance(P_ADD) && g.parts.size() < static_cast<std::size_t>(limits::MAX_PARTS)) {
        Part p;
        p.type  = static_cast<PartType>(rng.below(NUM_TYPES));
        p.size  = rng.uniform(0.15f, 0.5f);
        p.theta = rng.uniform(0.0f, 6.2831853f);
        p.phi   = rng.uniform(0.0f, 3.1415927f);
        g.parts.push_back(p);
    }

    if (rng.chance(P_RETYPE) && !g.parts.empty())
        g.parts[rng.below(static_cast<uint32_t>(g.parts.size()))].type = static_cast<PartType>(rng.below(NUM_TYPES));

    // --- Brain ------------------------------------------------------------------
    bool can_move = false;
    for (const Part& p : g.parts) can_move = can_move || is_motor(p.type);

    if (can_move && g.brain.empty()) {
        // The first nervous system: random twitches. Mostly it just wanders.
        // No hidden neurons yet: an urge to wander and a few random reflexes.
        g.brain.connect(input_node(IN_BIAS), output_node(OUT_WANDER), 1.0f);
        for (int k = 0; k < 4; ++k)
            g.brain.connect(static_cast<int>(rng.below(NUM_INPUTS)), FIRST_OUTPUT + static_cast<int>(rng.below(NUM_OUTPUTS)),
                            rng.normal() * 0.3f);
    } else if (!g.brain.empty()) {
        if (!can_move && rng.chance(P_LOSE_BRAIN)) {
            g.brain.clear();
        } else {
            mutate_brain(g.brain, rng);
        }
    }

    clamp_genome(g);
    return g;
}

bool transfer_part(const Genome& from, Genome& to, Rng& rng, PartType& copied) {
    if (from.parts.empty() || to.parts.size() >= static_cast<std::size_t>(limits::MAX_PARTS)) return false;
    Part bud = from.parts[rng.below(static_cast<uint32_t>(from.parts.size()))];
    bud.size *= 0.5f;   // it arrives as a bud, like a duplicated part
    copied = bud.type;
    to.parts.push_back(bud);
    clamp_genome(to);
    return true;
}

}  // namespace luca
