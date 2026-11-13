#pragma once
// Laws (design doc 14.6.2): the tunable numbers of this world's physics, in
// one struct, so a run can change one without a rebuild (`--set NAME=VALUE`,
// `--laws FILE`) and a snapshot can say which laws it was made under.
//
// Each law keeps the name it had as a constant, and the file that uses it
// still reads it by that name (`inline const float& METABOLIC_RATE =
// g_laws.METABOLIC_RATE;`), next to the comment that explains it. The
// defaults below are the values those constants had, so nothing changes
// until a law is set.
//
// There is one set of laws per process (g_laws), not one per Simulation:
// the phenotype compiler and the energy rules are free functions called
// from the sim, the designer, the test tank and the tests, and all of them
// should see the same physics. Set the laws before building or loading a
// world; changing them under a running world is a re-ruled world (bodies
// keep the stats they were compiled with until they are recompiled).
//
// Adding a law: add a line to LUCA_LAWS and turn the constant where it is
// used into a reference to g_laws. Per-part tables (MASS_PER_SIZE,
// COST_PER_SIZE) and integer constants are not laws yet.

#include <string>
#include <utility>
#include <vector>

namespace luca {

// X(name, default, where it is used)
#define LUCA_LAWS(X) \
    X(METABOLIC_RATE,       0.75f,         "energy") \
    X(PHOTO_EFFICIENCY,     1.0f,          "energy") \
    X(MOVING_SHADE,         0.8f,          "energy") \
    X(FULL_STRIDE,          0.005f,        "energy") \
    X(DORMANT_SAVING,       0.9f,          "energy") \
    X(FALL_ASLEEP,          1.0f / 20.0f,  "energy") \
    X(WAKE_UP,              1.0f / 90.0f,  "energy") \
    X(AGE_SCALE,            10000.0f,      "energy") \
    X(PIGMENT_WARMTH,       8.0f,          "energy") \
    X(THERMAL_SCALE,        8.0f,          "energy") \
    X(TRANSPORT_COST,       2.0f,          "energy") \
    X(MOVE_COST,            3.0f,          "energy") \
    X(CLIMB_COST,           1.0f,          "energy") \
    X(THROW_COST,           2.0f,          "energy") \
    X(FOUL_HARM,            0.5f,          "energy") \
    X(INHIBIT_K,            0.0f,          "energy") \
    X(INHIBIT_SCALE,        74.0f,         "energy") \
    X(CONTACT_RATE,         1.5f,          "energy") \
    X(CORPSE_RATE,          2.0f,          "energy") \
    X(SIZE_POWER,           2.0f,          "energy") \
    X(SIZE_EDGE_MAX,        2.0f,          "energy") \
    X(FOLD_STRIDE,          0.001f,        "energy") \
    X(UNFOLD_TICKS,         600.0f,        "energy") \
    X(TRANSFER_EFFICIENCY,  0.7f,          "energy") \
    X(UPTAKE_RATE,          0.05f,         "energy") \
    X(WASTE_SPREAD,         0.2f,          "energy") \
    X(WASTE_DECAY,          0.04f,         "energy") \
    X(CORPSE_WEATHERING,    0.004f,        "energy") \
    X(BASE_UPKEEP,          0.3f,          "phenotype") \
    X(PHOTO_PER_AREA,       1.0f,          "phenotype") \
    X(FLAGELLUM_THRUST,     0.125f,        "phenotype") \
    X(LEGS_THRUST,          0.10f,         "phenotype") \
    X(MAX_SPEED,            0.5f,          "phenotype") \
    X(ARMOUR_SHADES,        0.5f,          "phenotype") \
    X(REACH_PER_RADIUS,     0.5f,          "phenotype") \
    X(TOXICITY_PER_SIZE,    1.0f,          "phenotype") \
    X(MAX_TOXICITY,         2.0f,          "phenotype") \
    X(CAPACITY_PER_VOLUME,  650.0f,        "phenotype") \
    X(CAPACITY_PER_STORE,   600.0f,        "phenotype") \
    X(SENSE_CELLS_PER_SIZE, 2.0f,          "phenotype") \
    X(BRAIN_COST,           0.0001f,       "phenotype") \
    X(NEURON_COST,          0.00012f,      "phenotype") \
    X(LINK_COST,            0.00002f,      "phenotype") \
    X(MEMORY_COST,          0.00002f,      "phenotype") \
    X(TOLERANCE_COST,       0.004f,        "phenotype") \
    X(THROW_UPKEEP,         0.015f,        "phenotype") \
    X(SKIN_UPKEEP,          0.1f,          "phenotype") \
    X(STALK_MASS,           0.1f,          "phenotype") \
    X(STALK_UPKEEP,         0.06f,         "phenotype") \
    X(BITE_UPKEEP,          0.06f,         "phenotype") \
    X(DORMANCY_UPKEEP,      0.02f,         "phenotype") \
    X(HEAT_LAG,             120.0f,        "phenotype") \
    X(ENZYME_UPKEEP,        1.0f,          "phenotype") \
    X(GEAR_STANDING,        1.0f,          "phenotype") \
    X(SENSOR_BRAIN,         0.0f,          "sim") \
    X(TURN_RATE,            0.3f,          "sim") \
    X(TURN_COST,            1.0f,          "sim") \
    X(THRUST_BASE,          0.0f,          "sim") \
    X(ARC_FOLDS,            1.0f,          "sim") \
    X(P_CHANNEL,            0.05f,         "mutation") \
    X(MOUTH_ARC_BASE,       0.5f,          "body") \
    X(MOUTH_ARC_PER_SIZE,   0.8f,          "body") \
    X(COVER_ARC,            1.5707963f,    "body") \
    X(MEMBRANE_BASE,        0.3f,          "body") \
    X(ARMOUR_RESIST,        2.0f,          "body") \
    X(ARMOUR_CLOGS,         0.5f,          "body") \
    X(BOX_SKIN,             0.24f,         "body") \
    X(BOX_DRAG,             0.3f,          "body") \
    X(STALK_CLEARS,         0.25f,         "body") \
    X(P_GENE,               0.20f,         "mutation") \
    X(HUE_STEP,             0.08f,         "mutation") \
    X(P_ADD,                0.030f,        "mutation") \
    X(P_DELETE,             0.020f,        "mutation") \
    X(P_DUPLICATE,          0.020f,        "mutation") \
    X(P_RETYPE,             0.005f,        "mutation") \
    X(P_MOUTH_FRONT,        0.3f,          "mutation") \
    X(P_WEIGHT,             0.15f,         "mutation") \
    X(P_ADD_LINK,           0.06f,         "mutation") \
    X(P_DEL_LINK,           0.04f,         "mutation") \
    X(P_ADD_NEURON,         0.015f,        "mutation") \
    X(P_DEL_NEURON,         0.010f,        "mutation") \
    X(P_TAU,                0.15f,         "mutation") \
    X(TAU_STEP,             0.5f,          "mutation") \
    X(P_BIAS,               0.15f,         "mutation") \
    X(P_ENZ_ADD,            0.010f,        "mutation") \
    X(P_ENZ_DELETE,         0.010f,        "mutation") \
    X(P_ENZ_COPY,           0.010f,        "mutation") \
    X(P_ENZ_SWAP,           0.010f,        "mutation") \
    X(P_ENZ_PAIR,           0.010f,        "mutation") \
    X(P_NEW_TRAIT,          0.10f,         "mutation") \
    X(P_LOSE_BRAIN,         0.05f,         "mutation") \
    X(WANDER_PATH,          1.0f,          "sim") \
    X(GROW_STEP,            0.05f,         "sim") \
    X(GROW_MIN,             0.01f,         "sim") \
    X(GROW_RESERVE,         0.3f,          "sim") \
    X(SOCIAL_FAST,          0.25f,         "sim") \
    X(W_RADIUS,             2.0f,          "species") \
    X(W_PIGMENT,            2.0f,          "species") \
    X(W_HUE,                1.0f,          "species") \
    X(W_OPTIMUM,            0.1f,          "species") \
    X(W_TOLERANCE,          0.05f,         "species") \
    X(W_THRESHOLD,          1.0f,          "species") \
    X(W_SHARE,              1.0f,          "species") \
    X(W_THROW,              0.4f,          "species") \
    X(W_BITE,               0.8f,          "species") \
    X(W_DORMANCY,           1.5f,          "species") \
    X(W_ELONGATE,           1.5f,          "species") \
    X(W_SQUARE,             1.0f,          "species") \
    X(W_BIRTH,              1.5f,          "species") \
    X(W_STALK,              0.8f,          "species") \
    X(W_ENZ_KIND,           0.6f,          "species") \
    X(W_ENZ_RATE,           0.4f,          "species") \
    X(W_PART_KIND,          0.6f,          "species") \
    X(W_PART_SIZE,          0.5f,          "species") \
    X(W_BRAIN,              0.4f,          "species") \
    X(W_WEIGHTS,            0.06f,         "species") \
    X(W_NEURON,             0.3f,          "species") \
    X(W_TAU,                0.05f,         "species") \
    /* Switches for the ablation pass (14.10.8): 1 = the rule as built, 0 = off. */ \
    X(USE_BITE,             1.0f,          "genome") \
    X(GENE_TRANSFER,        1.0f,          "sim") \
    X(SPILL_LOST,           1.0f,          "sim") \
    X(DEAD_IN_REACH,        1.0f,          "sim") \
    X(LIGHT_ACROSS,         0.0f,          "world") \
    X(LIGHT_FOOTPRINT,      1.0f,          "sim") \
    X(P_BASE,               0.0f,          "phenotype") \
    X(P_PART_SCALE,         1.0f,          "phenotype") \
    X(P_STORE,              2.0f,          "phenotype") \
    X(P_UPTAKE,             0.01f,         "sim") \
    X(P_WEATHER_SHARE,      0.25f,         "sim") \
    X(P_MEAN,               0.3f,          "sim") \
    X(VENT_P,               0.02f,         "sim")

struct Laws {
#define LUCA_LAW_FIELD(name, value, where) float name = value;
    LUCA_LAWS(LUCA_LAW_FIELD)
#undef LUCA_LAW_FIELD
};

inline Laws g_laws{};

struct LawInfo {
    const char* name;
    const char* where;   // the file that uses it, without its extension
    float Laws::*field;
};

// Every law, in the order of LUCA_LAWS.
const std::vector<LawInfo>& law_table();
// The law of this name (case does not matter), or null.
const LawInfo* find_law(const std::string& name);

// A list of changes to the laws, (name, value), in the order given.
using LawChanges = std::vector<std::pair<std::string, float>>;

// Laws that differ from their defaults.
LawChanges changed_laws(const Laws& l);
// Laws that differ between two sets, with the values in `to`.
LawChanges laws_diff(const Laws& from, const Laws& to);

// "NAME=VALUE" appended to `out` (the name as the table spells it). False,
// with `error` filled, on an unknown name or a bad number.
bool parse_law(const std::string& assignment, LawChanges& out, std::string& error);
// A laws file: one "NAME VALUE" or "NAME=VALUE" a line; '#' starts a comment.
bool read_laws_file(const std::string& path, LawChanges& out, std::string& error);
// Apply changes from parse_law or read_laws_file (unknown names are skipped).
void apply_laws(Laws& l, const LawChanges& changes);

}  // namespace luca
