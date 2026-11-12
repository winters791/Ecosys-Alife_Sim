#include "creature_file.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <sstream>

namespace luca {

namespace {

const char* const INPUT_NAMES[NUM_INPUTS] = {"bias", "hunger", "light", "crowd", "cold", "corpse", "moved", "eating", "hurt"};
const char* const OUTPUT_NAMES[NUM_OUTPUTS] = {"light", "biomass", "corpse", "comfort", "nutrient", "kin",
                                               "bigger", "moving", "wander", "inertia", "throttle"};
const char* const PART_WORDS[static_cast<int>(PartType::Count)] = {"photo", "absorber", "flagellum", "legs",
                                                                   "sensor", "armour", "storage", "poison"};

// %.9g round-trips a float exactly, so a meteor in a replayed power log
// carries exactly the genome that was cast.
std::string num(float v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.9g", static_cast<double>(v));
    return buf;
}

std::string node_name(int node, bool as_source) {
    if (node >= FIRST_HIDDEN) return "h" + std::to_string(node - FIRST_HIDDEN);
    if (as_source) return node < NUM_INPUTS ? INPUT_NAMES[node] : "?";
    return node >= FIRST_OUTPUT ? OUTPUT_NAMES[node - FIRST_OUTPUT] : "?";
}

// A node by name; -1 if unknown. Sources are inputs or hidden neurons,
// targets outputs or hidden neurons, so "light" is never ambiguous.
int node_number(const std::string& w, bool as_source) {
    if (w.size() >= 2 && w[0] == 'h' && std::isdigit(static_cast<unsigned char>(w[1]))) {
        const int k = std::atoi(w.c_str() + 1);
        return k >= 0 && k < MAX_HIDDEN ? hidden_node(k) : -1;
    }
    if (as_source) {
        for (int i = 0; i < NUM_INPUTS; ++i)
            if (w == INPUT_NAMES[i]) return i;
    } else {
        for (int o = 0; o < NUM_OUTPUTS; ++o)
            if (w == OUTPUT_NAMES[o]) return FIRST_OUTPUT + o;
    }
    return -1;
}

int chem_number(char c) {
    for (int k = 0; k < NCHEM; ++k)
        if (c == chem_name(k)[0]) return k;
    return -1;
}

// "A-D" or "A+A-D+D" (enzyme_words).
bool parse_enzyme(const std::string& w, Enzyme& e) {
    const auto dash = w.find('-');
    if (dash == std::string::npos) return false;
    auto side = [](const std::string& s, uint8_t& a, uint8_t& b) {
        if (s.size() == 1) {
            const int k = chem_number(s[0]);
            if (k < 0) return false;
            a = static_cast<uint8_t>(k);
            b = NO_CHEM;
            return true;
        }
        if (s.size() == 3 && s[1] == '+') {
            const int k = chem_number(s[0]), j = chem_number(s[2]);
            if (k < 0 || j < 0) return false;
            a = static_cast<uint8_t>(k);
            b = static_cast<uint8_t>(j);
            return true;
        }
        return false;
    };
    if (!side(w.substr(0, dash), e.x, e.x2) || !side(w.substr(dash + 1), e.y, e.y2)) return false;
    return (e.x2 == NO_CHEM) == (e.y2 == NO_CHEM);
}

void write_genome(std::ostringstream& o, const Genome& g, const char* sep) {
    o << "body" << sep;
    o << "radius " << num(g.core_radius) << sep;
    o << "shape";
    for (float s : g.shape) o << ' ' << num(s);
    o << sep;
    o << "pigment " << num(g.pigment) << sep;
    o << "hue " << num(g.hue) << sep;
    o << "thermal " << num(g.thermal_optimum) << ' ' << num(g.thermal_tolerance) << sep;
    o << "repro " << num(g.repro_threshold) << ' ' << num(g.offspring_share) << sep;
    o << "birth " << num(g.birth_size) << sep;
    o << "elongation " << num(g.elongation) << sep;
    o << "squareness " << num(g.squareness) << sep;
    o << "bite " << num(g.bite) << sep;
    o << "dormancy " << num(g.dormancy) << sep;
    o << "throw " << num(g.seed_throw) << sep;
    o << "mutability " << num(g.mutability) << sep;
    o << "repair " << num(g.repair) << sep;
    for (const Part& p : g.parts)
        o << "part " << PART_WORDS[static_cast<int>(p.type)] << ' ' << num(p.size) << ' ' << num(p.theta) << ' '
          << num(p.stalk) << sep;
    for (const Enzyme& e : g.enzymes) o << "enzyme " << enzyme_words(e) << ' ' << num(e.rate) << sep;
    for (int k = 0; k < g.brain.hidden; ++k) o << "neuron " << num(g.brain.tau[k]) << ' ' << num(g.brain.bias[k]) << sep;
    for (const BrainLink& l : g.brain.links)
        o << "link " << node_name(l.from, true) << ' ' << node_name(l.to, false) << ' ' << num(l.weight) << sep;
}

std::string write_with(const CreatureFile& f, const char* sep) {
    std::ostringstream o;
    o << "luca-creature " << CREATURE_FILE_VERSION << sep;
    o << "name " << clean_text(f.name) << sep;
    if (!f.origin.empty()) o << "origin " << clean_text(f.origin, 120) << sep;
    for (const Genome& g : f.bodies) write_genome(o, g, sep);
    return o.str();
}

}  // namespace

const char* brain_input_name(int input) { return input >= 0 && input < NUM_INPUTS ? INPUT_NAMES[input] : "?"; }
const char* brain_output_name(int output) { return output >= 0 && output < NUM_OUTPUTS ? OUTPUT_NAMES[output] : "?"; }

std::string clean_text(const std::string& s, std::size_t max_len) {
    std::string out;
    for (char c : s) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (c == ';' || c == '#' || u < 32 || u == 127) continue;
        out += c;
    }
    const auto a = out.find_first_not_of(' ');
    if (a == std::string::npos) return {};
    out = out.substr(a, out.find_last_not_of(' ') - a + 1);
    if (out.size() > max_len) out.resize(max_len);
    return out;
}

std::string write_creature_file(const CreatureFile& f) { return write_with(f, "\n"); }
std::string creature_line(const CreatureFile& f) { return write_with(f, " ; "); }

bool read_creature_file(const std::string& text, CreatureFile& out, std::string& error) {
    out = CreatureFile{};
    // Split on newlines and ';' alike.
    std::vector<std::string> lines(1);
    for (char c : text) {
        if (c == '\n' || c == ';') lines.emplace_back();
        else if (c != '\r') lines.back() += c;
    }
    bool header = false;
    Genome* g = nullptr;
    for (std::size_t n = 0; n < lines.size(); ++n) {
        std::string line = lines[n];
        if (const auto hash = line.find('#'); hash != std::string::npos) line.erase(hash);
        std::istringstream ss(line);
        std::string key;
        if (!(ss >> key)) continue;
        auto fail = [&](const std::string& why) {
            error = "line " + std::to_string(n + 1) + ": " + why;
            return false;
        };
        if (!header) {
            int version = 0;
            if (key != "luca-creature" || !(ss >> version)) return fail("not a creature file (expected 'luca-creature 1')");
            if (version < 1 || version > CREATURE_FILE_VERSION) return fail("unknown version " + std::to_string(version));
            header = true;
            continue;
        }
        if (key == "name" || key == "origin") {
            std::string rest;
            std::getline(ss, rest);
            (key == "name" ? out.name : out.origin) = clean_text(rest, key == "name" ? 48 : 120);
            continue;
        }
        if (key == "body") {
            out.bodies.emplace_back();
            g = &out.bodies.back();
            g->parts.clear();
            g->enzymes.clear();
            g->brain.clear();
            continue;
        }
        if (g == nullptr) return fail("'" + key + "' before the first 'body'");
        auto floats = [&](std::initializer_list<float*> into) {
            for (float* f : into)
                if (!(ss >> *f) || !std::isfinite(*f)) return false;
            return true;
        };
        bool ok = true;
        if      (key == "radius")     ok = floats({&g->core_radius});
        else if (key == "pigment")    ok = floats({&g->pigment});
        else if (key == "hue")        ok = floats({&g->hue});
        else if (key == "thermal")    ok = floats({&g->thermal_optimum, &g->thermal_tolerance});
        else if (key == "repro")      ok = floats({&g->repro_threshold, &g->offspring_share});
        else if (key == "birth")      ok = floats({&g->birth_size});
        else if (key == "elongation") ok = floats({&g->elongation});
        else if (key == "squareness") ok = floats({&g->squareness});
        else if (key == "bite")       ok = floats({&g->bite});
        else if (key == "dormancy")   ok = floats({&g->dormancy});
        else if (key == "throw")      ok = floats({&g->seed_throw});
        else if (key == "mutability") ok = floats({&g->mutability});
        else if (key == "repair")     ok = floats({&g->repair});
        else if (key == "shape") {
            for (float& s : g->shape) ok = ok && floats({&s});
        } else if (key == "part") {
            std::string type;
            Part p;
            ss >> type;
            int t = -1;
            for (int k = 0; k < static_cast<int>(PartType::Count); ++k)
                if (type == PART_WORDS[k]) t = k;
            if (t < 0) return fail("unknown part '" + type + "'");
            p.type = static_cast<PartType>(t);
            ok = floats({&p.size, &p.theta, &p.stalk});
            if (ok) g->parts.push_back(p);
        } else if (key == "enzyme") {
            std::string w;
            Enzyme e;
            ss >> w;
            if (!parse_enzyme(w, e)) return fail("bad reaction '" + w + "' (e.g. A-D or A+A-D+D)");
            ok = floats({&e.rate});
            if (ok) g->enzymes.push_back(e);
        } else if (key == "neuron") {
            if (g->brain.hidden >= MAX_HIDDEN) return fail("more than " + std::to_string(MAX_HIDDEN) + " neurons");
            const int k = g->brain.hidden;
            ok = floats({&g->brain.tau[k], &g->brain.bias[k]});
            if (ok) ++g->brain.hidden;
        } else if (key == "link") {
            std::string from, to;
            float w = 0.0f;
            ss >> from >> to;
            const int a = node_number(from, true), b = node_number(to, false);
            if (a < 0) return fail("unknown link source '" + from + "'");
            if (b < 0) return fail("unknown link target '" + to + "'");
            ok = floats({&w});
            if (ok) g->brain.links.push_back({static_cast<uint8_t>(a), static_cast<uint8_t>(b), w});
        } else {
            return fail("unknown key '" + key + "'");
        }
        if (!ok) return fail("bad numbers after '" + key + "'");
    }
    if (!header) {
        error = "empty file";
        return false;
    }
    if (out.bodies.empty()) {
        error = "no 'body' in the file";
        return false;
    }
    for (Genome& b : out.bodies) clamp_genome(b);
    return true;
}

}  // namespace luca
