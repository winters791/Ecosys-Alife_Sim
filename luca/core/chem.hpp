#pragma once
// The chemistry layer (design doc 2.1.1), stage 1.
//
// Matter comes in a few kinds of chemical, each with a fixed energy level per
// unit. Energy lives in them: a body's store, its body, a corpse are all
// amounts of chemicals, and what they are worth is amount x level. Matter is
// conserved unit for unit; energy is the difference in levels.
//
//   A  0    spent matter: what the soil holds (the old "free nutrient"),
//           like air and water to a plant. Never held in a store.
//   B  74
//   C  148
//   D  222  the richest: a body built of D is worth what a body was worth
//           before chemistry (222 a unit of mass)
//
// Enzymes (genome.hpp) turn one chemical into another, either way: downhill
// they release the difference in levels for the body to spend, uphill they
// need energy put in (light, a vent). A reaction that starts from A draws it
// from the cell; one that ends at A gives it back to the cell.

#include <algorithm>
#include <cstdint>

namespace luca {

constexpr int NCHEM = 4;
enum ChemId : uint8_t { CHEM_A = 0, CHEM_B = 1, CHEM_C = 2, CHEM_D = 3 };
constexpr float LEVEL[NCHEM] = {0.0f, 74.0f, 148.0f, 222.0f};
constexpr float TOP_LEVEL = LEVEL[CHEM_D];

inline const char* chem_name(int k) {
    static const char* names[NCHEM] = {"A", "B", "C", "D"};
    return k >= 0 && k < NCHEM ? names[k] : "?";
}

// An amount of each chemical.
struct Mix {
    float v[NCHEM] = {};

    float mass() const { return v[0] + v[1] + v[2] + v[3]; }
    float energy() const { return v[1] * LEVEL[1] + v[2] * LEVEL[2] + v[3] * LEVEL[3]; }
    // Energy per unit of mass (0 if empty).
    float density() const {
        const float m = mass();
        return m > 0.0f ? energy() / m : 0.0f;
    }
    void add(const Mix& o, float k = 1.0f) {
        for (int c = 0; c < NCHEM; ++c) v[c] += o.v[c] * k;
    }
    Mix scaled(float k) const {
        Mix m;
        for (int c = 0; c < NCHEM; ++c) m.v[c] = v[c] * k;
        return m;
    }
    void clear() { for (float& x : v) x = 0.0f; }
};

inline Mix pure(int chem, float amount) {
    Mix m;
    m.v[chem] = amount;
    return m;
}

// Takes `amount` of mass out of `from`, in proportion to what it holds, and
// returns what was taken. Never takes more than there is.
inline Mix take_share(Mix& from, float amount) {
    Mix out;
    const float m = from.mass();
    if (m <= 0.0f || amount <= 0.0f) return out;
    if (amount >= m) {
        out = from;
        from.clear();
        return out;
    }
    const float k = amount / m;
    for (int c = 0; c < NCHEM; ++c) {
        out.v[c] = from.v[c] * k;
        from.v[c] -= out.v[c];
    }
    return out;
}

}  // namespace luca
