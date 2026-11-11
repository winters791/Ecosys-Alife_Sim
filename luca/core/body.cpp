#include "body.hpp"

#include <algorithm>
#include <cmath>

namespace luca {

namespace {

constexpr float PI     = 3.14159265f;
constexpr float TWO_PI = 6.2831853f;
constexpr float BIN    = TWO_PI / BODY_DIRS;   // width of one direction

// Smallest angle between two directions, 0..pi.
float gap(float a, float b) {
    return std::abs(std::remainder(a - b, TWO_PI));
}

// Fraction of the direction centred on `centre` that an arc of half-width
// `half` around `theta` covers, 0..1.
// Sampled, so arcs that wrap round the back are handled without special cases;
// it runs once per birth.
float covers(float theta, float half, float centre) {
    if (half >= PI) return 1.0f;
    constexpr int SAMPLES = 8;
    int in = 0;
    for (int s = 0; s < SAMPLES; ++s)
        in += gap(theta, centre + BIN * ((static_cast<float>(s) + 0.5f) / SAMPLES - 0.5f)) < half;
    return static_cast<float>(in) / SAMPLES;
}

float arc_of(PartType t, float size) {
    return t == PartType::Absorber ? mouth_halfwidth(size) : cover_halfwidth(size);
}

}  // namespace

float mouth_halfwidth(float size) { return std::min(PI, MOUTH_ARC_BASE + MOUTH_ARC_PER_SIZE * std::max(0.0f, size)); }
float cover_halfwidth(float size) { return std::min(PI, COVER_ARC * std::max(0.0f, size)); }

int body_dir(float along, float across) {
    const float a = std::atan2(across, along);   // -pi..pi, 0 = front
    return static_cast<int>(std::lround(a / BIN)) & (BODY_DIRS - 1);
}

float armour_at(const Genome& g, float theta) {
    float sum = 0.0f;
    for (const Part& p : g.parts)
        if (p.type == PartType::Armour && p.size > 0.0f && gap(p.theta, theta) < cover_halfwidth(p.size)) sum += p.size;
    return sum;
}

float clogged(const Genome& g, const Part& part) {
    const float under = 1.0f - std::min(1.0f, std::max(0.0f, part.stalk) / STALK_CLEARS);
    return 1.0f / (1.0f + ARMOUR_CLOGS * armour_at(g, part.theta) * under);
}

float thrust_facing(PartType type, float theta) {
    if (type == PartType::Flagellum) return 0.25f + 0.75f * 0.5f * (1.0f - std::cos(theta));
    if (type == PartType::Legs)      return 0.5f + 0.5f * std::abs(std::sin(theta));
    return 0.0f;
}

float aspect(float elongation) { return 1.0f + 2.0f * std::clamp(elongation, 0.0f, 1.0f); }

float skin_factor(float elongation, float squareness) {
    // A prolate spheroid with the volume of a unit ball: axes k^(2/3) and k^(-1/3).
    const float k = aspect(elongation);
    float stretched = 1.0f;
    if (k > 1.0001f) {
        const float e = std::sqrt(1.0f - 1.0f / (k * k));
        stretched = 0.5f * std::pow(k, -2.0f / 3.0f) * (1.0f + k * std::asin(e) / e);
    }
    return stretched * (1.0f + BOX_SKIN * std::clamp(squareness, 0.0f, 1.0f));
}

float drag_factor(float elongation, float squareness) {
    return (1.0f + BOX_DRAG * std::clamp(squareness, 0.0f, 1.0f)) / std::cbrt(aspect(elongation));
}

BodyPlan compile_body(const Genome& g) {
    BodyPlan b;
    for (const Part& p : g.parts) {
        if (p.size <= 0.0f || (p.type != PartType::Absorber && p.type != PartType::Armour)) continue;
        const float half = arc_of(p.type, p.size);
        const float amount = p.type == PartType::Absorber ? p.size * clogged(g, p) : p.size;
        float* table = p.type == PartType::Absorber ? b.mouth : b.armour;
        const float stalk = p.type == PartType::Absorber ? std::clamp(p.stalk, 0.0f, limits::MAX_STALK) : 0.0f;
        for (int k = 0; k < BODY_DIRS; ++k) {
            const float c = covers(p.theta, half, BIN * static_cast<float>(k));
            table[k] += amount * c;
            if (stalk > 0.0f && c > 0.0f) {
                b.far_mouth[k] += amount * c;
                b.stalk[k] = std::max(b.stalk[k], stalk);
            }
        }
        b.longest_stalk = std::max(b.longest_stalk, stalk);
        if (p.type == PartType::Armour) b.any_armour = true;
    }
    const auto [lo, hi] = std::minmax_element(b.mouth, b.mouth + BODY_DIRS);
    b.even_mouth = b.longest_stalk == 0.0f && *hi - *lo <= 1e-4f * std::max(*hi, 1.0f);
    return b;
}

}  // namespace luca
