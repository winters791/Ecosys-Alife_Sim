# LUCA — ecosystem sim core

The headless simulation core for the ecosystem god game. It's pure C++20 with
no Godot dependency. Godot will later sit on top as a thin renderer.

## Build and run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

./build/luca_headless --ticks 20000 --out out
python3 tools/plot.py out        # writes out/run.png and out/map.png
```

Options: `--ticks --seed --size --pop --light uniform|gradient --sun --interval --out --no-snapshot --quiet`.
Run `--help` for defaults.

## Layout

| Path | What it is | Who writes it |
| --- | --- | --- |
| `core/genome.*` | Genome + `make_luca()` | Scaffolding (extend freely) |
| `core/phenotype.*` | Genome → derived stats | **Uday** (placeholder inside) |
| `core/energy.*` | Light demand, upkeep, thermal, movement, contact and corpse rules | First version by Claude (2026-11-11); Uday tunes |
| `core/mutation.*` | Parent genome → child genome | First version by Claude (2026-11-11); Uday tunes |
| `core/world.*` | Heightmap terrain, light and temperature fields | Scaffolding |
| `core/creatures.hpp` | Data-oriented creature storage (one array per field) | Scaffolding |
| `core/spatial_grid.hpp` | Per-cell buckets, rebuilt each tick in O(N) | Scaffolding |
| `core/sim.*` | Two-phase tick: decide, then apply; births and deaths | Scaffolding |
| `core/metrics.*` | CSV metrics, energy audit, end-of-run snapshot | Scaffolding |
| `core/rng.hpp` | Deterministic xoshiro256** RNG | Scaffolding |
| `core/config.hpp` | World settings and units | Scaffolding |
| `headless/main.cpp` | CLI runner | Scaffolding |
| `tools/plot.py` | Plots for a run | Scaffolding |

The creature rules are now filled in. `../CLAUDE.md` (sections 10 and 11) is the up-to-date reference, including which constants to tune; parts of this README describe the earlier placeholder stage. (`DESIGN.md`, an old copy of the design, was deleted on 2026-11-12.)

## Units (proposed: change now, not later)

- 1 tick = 1 minute of world time
- 1 energy unit = what one cell receives per tick under full sun
- 1 cell = `cell_size` world units (default 1)

## Rules the scaffolding already enforces

- **Light is finite.** Each cell gets `light` energy per tick. If the creatures
  in a cell ask for more than that in total, each gets the same fraction.
  Crowding creates competition, which creates a carrying capacity.
- **You can't spend what you don't have.** Costs are capped at a creature's
  energy. A creature at zero dies.
- **Energy is conserved.** Each metrics interval checks
  `stored(end) == stored(start) + in − out`. The `audit_rel_error` column
  should stay around 1e-6 (float rounding). If it jumps, something is
  creating or destroying energy, and evolution *will* find and exploit it.
- **Reproduction transfers energy.** At `repro_threshold`, the parent hands
  `offspring_share` of its energy to a mutated child.
- **Deterministic.** Same seed, same build, same machine gives identical runs.
  Only use the `Rng` passed into your code, never `std::rand`.

## Baseline (placeholder rules, default settings)

- Population plateaus around 8,700 on a 128×128 world. Life stops where light
  drops below upkeep (≈ y = 80 in gradient mode).
- About 0.2 ms per tick at 8.7k creatures, about 0.9 ms per tick at 33k
  (256×256 world). That's linear scaling.
- Audit error is about 1e-6.

## Your tasks, in order

1. **Phenotype compiler** (`phenotype.cpp`): clamping, Kleiber scaling, per-part
   costs, light capture limited by surface area.
2. **Energy rules** (`energy.cpp`): real upkeep, thermal cost outside the
   tolerance range, movement cost with slope.
3. **Mutation** (`mutation.cpp`): tweak, add, delete, duplicate, and retype parts.
4. **Emergence test 1**: `--light uniform`. Population should rise and
   plateau, neither explode nor die out.
5. **Emergence test 2**: `--light gradient`. The *photo size, sunny half* and
   *photo size, dim half* lines in `run.png` should drift apart. That's your
   first emergent variation.

## Next scaffolding (once your parts work)

- Genome distance function + species IDs (NEAT-style representatives)
- Contact interaction (energy transfer between touching creatures) using the
  spatial grid
- Corpses and matter
- Multithreading phase 1 by world tiles
- GDExtension + MultiMesh renderer
