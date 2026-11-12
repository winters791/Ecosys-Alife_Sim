#pragma once
// Where parts sit on the body, and what that changes.
//
// Every part has an angle on the outline (Part::theta, radians from the
// front, + to the left). A body faces the way it last moved; a body that
// never moves keeps the facing it was born with. Placement is physics, not
// decoration:
//   * a mouth only pulls on bodies within its arc;
//   * armour only resists a pull that comes from within its arc;
//   * armour lying over a mouth or a tail clogs it, as it used to clog
//     every mouth whatever the plates covered;
//   * a tail pushes best from the back, legs best from the sides.
//
// The arcs are summed into a small table of directions once at birth
// (BodyPlan), so contact reads one number per direction. viewer/blob.gdshader
// draws the same arcs and must match the constants here.

#include "genome.hpp"
#include "laws.hpp"

namespace luca {

constexpr int   BODY_DIRS = 16;              // directions in a body plan's tables
inline const float& MOUTH_ARC_BASE     = g_laws.MOUTH_ARC_BASE;  // law, 0.5f: a mouth's half-width in radians, at size 0 ...
inline const float& MOUTH_ARC_PER_SIZE = g_laws.MOUTH_ARC_PER_SIZE;  // law, 0.8f: ... plus this per unit of size, up to all round
inline const float& COVER_ARC          = g_laws.COVER_ARC;  // law, 1.5707963f: armour, photo, poison: half-width per unit of size (size 2 covers everything)

// Contact resistance (moved here from phenotype.cpp).
inline const float& MEMBRANE_BASE = g_laws.MEMBRANE_BASE;  // law, 0.3f: every body resists a little, from every side
inline const float& ARMOUR_RESIST = g_laws.ARMOUR_RESIST;  // law, 2.0f: resistance per unit of armour, where it covers
inline const float& ARMOUR_CLOGS  = g_laws.ARMOUR_CLOGS;  // law, 0.5f: armour lying over a mouth or a tail slows it

float mouth_halfwidth(float size);
float cover_halfwidth(float size);

// The direction table entry for a direction given in the body's own frame
// (along = towards the front, across = towards its left).
int body_dir(float along, float across);

// Summed size of the armour lying over an angle on the outline.
float armour_at(const Genome& g, float theta);

// How much of a part's work survives the plates lying over it: 1 when bare,
// or when it stands clear of them on a stalk.
float clogged(const Genome& g, const Part& part);

// How well a motor part pushes from where it sits, 0..1.
//   tail: 1 straight behind, 1/4 at the front (it can still pull a little);
//   legs: 1 at the sides, 1/2 at the very front or back.
float thrust_facing(PartType type, float theta);

// --- Shape ----------------------------------------------------------------------
// Elongation (0..1) stretches a body to up to three times as long as wide,
// keeping its volume; squareness (0..1) squares it off towards a box. Both
// add skin: more room for sun patches, but faster heat loss, more fouling,
// and skin to keep up. A long body slips through the world more easily; a
// boxy one drags. The viewer draws the same outline (blob.gdshader).
inline const float& BOX_SKIN = g_laws.BOX_SKIN;  // law, 0.24f: a cube has 24% more skin than a ball of the same volume
inline const float& BOX_DRAG = g_laws.BOX_DRAG;  // law, 0.3f: and drags this much more

// Length over width: 1 round, 3 at elongation 1.
float aspect(float elongation);
// Skin of this shape over the skin of a ball of the same volume, >= 1.
float skin_factor(float elongation, float squareness);
// Drag on the move against a ball's: below 1 for long bodies, above 1 for boxy ones.
float drag_factor(float elongation, float squareness);

// A mouth on a stalk this long or longer clears any armour beside it.
inline const float& STALK_CLEARS = g_laws.STALK_CLEARS;  // law, 0.25f

struct BodyPlan {
    float mouth[BODY_DIRS]  = {};  // pull towards each direction (clogging included)
    float far_mouth[BODY_DIRS] = {};  // the part of that pull from mouths on stalks
    float stalk[BODY_DIRS]  = {};  // how much further than its skin it can reach, each way
    float longest_stalk = 0.0f;
    float armour[BODY_DIRS] = {};  // armour size covering each direction
    bool  any_armour = false;      // false: the membrane is MEMBRANE_BASE all round
    bool  even_mouth = true;       // true: pulls the same in every direction (no need to look)
};

BodyPlan compile_body(const Genome& g);

}  // namespace luca
