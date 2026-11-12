#pragma once
// Colour as a physical fact (2026-11-12). A body's skin has a colour, from
// two genes: `hue` (where on the colour wheel, free to drift) and `pigment`
// (how dark, which also sets how much sun it soaks up as heat). The ground has
// a colour too, from its height, soil, snow and vents. The one rule colour
// obeys: a body is easier to sense the more it stands out from the ground it
// is on (camouflage). Nothing says which colour is good; that depends on
// where a body lives and who is looking.
//
// The formulas here are the same ones blob.gdshader and terrain.gdshader draw
// with, so what a hunter "sees" is what the player sees. Keep them in step.

#include <algorithm>
#include <cmath>

namespace luca {

struct Rgb {
    float r = 0.0f, g = 0.0f, b = 0.0f;
};

inline Rgb mix(Rgb a, Rgb b, float t) {
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

inline float distance(Rgb a, Rgb b) {
    const float dr = a.r - b.r, dg = a.g - b.g, db = a.b - b.b;
    return std::sqrt(dr * dr + dg * dg + db * db);
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

constexpr float GROUND_SCALE = 40.0f;   // height of the top band: terrain.gdshader's max_height

// The ground's colour in the natural view, without its pixel-art detail
// (dither, hill shading, specks): height band, darker where the soil is rich,
// whitened by frost, basalt over a vent.
inline Rgb ground_colour(float height, float temperature, float nutrient, float vent) {
    static constexpr Rgb BANDS[6] = {{0.80f, 0.69f, 0.44f}, {0.67f, 0.54f, 0.34f}, {0.52f, 0.41f, 0.28f},
                                     {0.40f, 0.35f, 0.31f}, {0.50f, 0.49f, 0.50f}, {0.68f, 0.69f, 0.72f}};
    const float k = std::clamp(height / GROUND_SCALE, 0.0f, 1.2f) * 5.0f;
    Rgb c = BANDS[std::clamp(static_cast<int>(k), 0, 5)];
    const float rich = 1.08f + (0.80f - 1.08f) * std::clamp(std::floor(nutrient * 1.5f) / 6.0f, 0.0f, 1.0f);
    c = {c.r * rich, c.g * rich, c.b * rich};
    c = mix(c, Rgb{0.88f, 0.92f, 0.97f}, std::clamp(-temperature / 8.0f, 0.0f, 1.0f));
    if (vent > 0.0f) c = mix(c, Rgb{0.20f, 0.17f, 0.17f}, std::clamp(0.35f + vent * 0.25f, 0.0f, 0.85f));
    return c;
}

// How plainly a body shows against its ground, VISIBLE_MIN (a perfect match)
// to 1 (a contrast of CONTRAST_FULL or more). It scales how strongly the
// senses of others feel the body: the lean towards the living, and the
// danger / kin sense.
constexpr float VISIBLE_MIN   = 0.25f;
constexpr float CONTRAST_FULL = 0.35f;

inline float visibility(Rgb skin, Rgb ground) {
    const float k = std::min(distance(skin, ground) / CONTRAST_FULL, 1.0f);
    return VISIBLE_MIN + (1.0f - VISIBLE_MIN) * k;
}

}  // namespace luca
