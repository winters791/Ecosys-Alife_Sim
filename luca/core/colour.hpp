#pragma once
// Skin colour (2026-11-12). A body's skin has a colour, from two genes: `hue`
// (where on the colour wheel, free to drift) and `pigment` (how dark, which
// also sets how much sun it soaks up as heat). Hue is drawing only: no sense
// reads it. (Camouflage, where others sensed a body by how it showed against
// its ground, was removed 2026-11-12; CLAUDE.md 14.8.)
//
// The formula here is the one blob.gdshader draws with. Keep them in step.

#include <algorithm>
#include <cmath>

namespace luca {

struct Rgb {
    float r = 0.0f, g = 0.0f, b = 0.0f;
};

inline Rgb mix(Rgb a, Rgb b, float t) {
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

// A pure colour on the wheel, 0..1 (0 red, 1/3 green, 2/3 blue).
inline Rgb hue_rgb(float h) {
    h -= std::floor(h);
    auto ch = [h](float o) {
        float x = h + o;
        x -= std::floor(x);
        return std::clamp(std::fabs(x * 6.0f - 3.0f) - 1.0f, 0.0f, 1.0f);
    };
    return {ch(0.0f), ch(2.0f / 3.0f), ch(1.0f / 3.0f)};
}

// Skin stays earthy (Uday, 2026-11-12: no magenta animal standing out like a
// sore thumb): the hue shows only faintly, and is pulled towards a tan base,
// so every hue comes out as a muted natural tone (sage, olive, slate,
// dusty mauve, rust) rather than a bright one.
constexpr float SKIN_CHROMA = 0.28f;   // how strongly the hue shows
constexpr float SKIN_EARTH  = 0.40f;   // how far it is pulled towards EARTH
constexpr Rgb   EARTH       = {1.0f, 0.82f, 0.60f};
constexpr float SKIN_PALE   = 0.92f;   // brightness at pigment 0
constexpr float SKIN_DARK   = 0.20f;   // brightness at pigment 1

// Skin colour. LUCA (hue 0.09, pigment 0.3) is the familiar tan.
inline Rgb skin_colour(float hue, float pigment) {
    const float value = SKIN_PALE + (SKIN_DARK - SKIN_PALE) * std::clamp(pigment, 0.0f, 1.0f);
    const Rgb c = mix(mix(Rgb{1.0f, 1.0f, 1.0f}, hue_rgb(hue), SKIN_CHROMA), EARTH, SKIN_EARTH);
    return {c.r * value, c.g * value, c.b * value};
}

}  // namespace luca
