#pragma once
// Snapshots (design doc 14.6.1): the whole state of a world in one file, so a
// run can be saved, loaded, and forked.
//
// State only, never derived values, by the same rule as creature files: the
// config, the clocks and counters, both random streams, the world's layers,
// the soil, every creature (genome, how far it has grown, where it is and
// what it holds), the powers, the species registry, the chronicle and alerts,
// and the event detectors. Light, temperature, shade, drainage, phenotypes,
// brain and body plans are recompiled on loading, under the loading build's
// rules.
//
// File layout: a short text header (one "key value" line each, ended by a
// blank line) that a person can read with `head`, then binary sections, each
// a four-letter tag and a length. A reader skips sections it does not know,
// and inside a record (a creature, a genome, a species) fields are read in
// order until the record ends, so a field added at the end of a record reads
// as its default from an older file. Numbers are written as the machine holds
// them (little-endian on every machine this runs on).
//
// Two ways to load:
//   * exact: the same build. The run continues tick for tick as if it had
//     never stopped (tested in test_life: test_snapshot_round_trip).
//   * re-ruled: a build whose rules differ. Every body is recompiled under
//     the new rules; a store that no longer fits its body spills what doesn't
//     fit into its cell, so both books still close. The energy audit starts
//     again from the loaded state.

#include <memory>
#include <string>

#include "laws.hpp"
#include "sim.hpp"

namespace luca {

// 2 (2026-11-12): the random streams' states gave way to two draw seeds
// (counter-based draws, rng.hpp). Version 1 files are refused.
constexpr int SNAPSHOT_VERSION = 2;

// The file as bytes, and back. load returns null and fills `error` on a file
// it cannot read.
//
// Laws (laws.hpp): the header lists every law in force when it was saved.
// Loading sets the process's laws (g_laws) to those, then applies `changes`
// (from --set or --laws), before any body is recompiled. With no changes the
// world continues exactly; with changes it is a re-ruled world, and its
// chronicle says which laws changed.
std::string snapshot_bytes(const Simulation& sim);
std::unique_ptr<Simulation> snapshot_from_bytes(const std::string& bytes, std::string& error, const LawChanges& changes = {});

bool save_snapshot(const Simulation& sim, const std::string& path, std::string& error);
std::unique_ptr<Simulation> load_snapshot(const std::string& path, std::string& error, const LawChanges& changes = {});

}  // namespace luca
