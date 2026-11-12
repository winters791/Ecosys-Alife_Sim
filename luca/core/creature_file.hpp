#pragma once
// Creature files (design doc section 5): the genome only, never derived
// stats, in a readable text format that is easy to edit by hand.
//
// One file holds one or more bodies. A designed creature is one body; a
// species banked from a world (the seed vault, 4.9) is a sample of up to
// ~20 of its living members, so it keeps its variation. Either can be
// dropped into a world as a meteor (the `seed` power, powers.hpp), where
// every body's stats are compiled under that world's physics.
//
//   luca-creature 1
//   name Spiky
//   origin designed
//   body
//   radius 1
//   pigment 0.3
//   hue 0.09                  (optional; skin colour on the wheel, colour.hpp)
//   thermal 20 10            optimum, tolerance (°C)
//   repro 600 0.5            threshold, offspring share
//   birth 0.5
//   ...                      every float gene, by name (see write_genome)
//   part absorber 0.5 0 0    type size theta stalk
//   enzyme A-D 0.015         reaction (poorer side first, enzyme_words) and rate
//   neuron 30 0              tau, bias (one line per hidden neuron, in order)
//   link hunger h0 1.5       from to weight: inputs and outputs by name, hidden neurons h0..h7
//   body
//   ...                      the next body
//
// Lines may also be separated by ';', so a whole file fits on one line (the
// power log keeps a meteor's cargo that way). '#' starts a comment. Unknown
// keys are an error, so a typo never silently drops a gene. Every genome
// read is passed through clamp_genome().

#include <string>
#include <vector>

#include "genome.hpp"

namespace luca {

constexpr int CREATURE_FILE_VERSION = 1;
constexpr int VAULT_SAMPLE = 20;   // bodies kept when a species is banked (section 5: ~20)

struct CreatureFile {
    std::string name;     // what the player called it, or the species' name
    std::string origin;   // free text: "designed", or where and when it was banked
    std::vector<Genome> bodies;
};

// Names and origins are one line of printable text without ';' or '#', at
// most 48 characters; anything else is dropped or cut.
std::string clean_text(const std::string& s, std::size_t max_len = 48);

// The brain's node names as files write them: inputs, outputs, h0..h7.
const char* brain_input_name(int input);
const char* brain_output_name(int output);

std::string write_creature_file(const CreatureFile& f);
// The same, with ';' between lines instead of newlines.
std::string creature_line(const CreatureFile& f);
// Returns false and fills `error` (with a line number) on the first bad line.
bool read_creature_file(const std::string& text, CreatureFile& out, std::string& error);

}  // namespace luca
