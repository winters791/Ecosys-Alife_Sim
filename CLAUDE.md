# Ecosystem God Game — Design & Project Reference

Owner: Uday · Last updated: 2026-11-06

An artificial-life ecosystem simulator played as a god game. It's like Dwarf Fortress, but more *fundamental*: there are no predefined classes of creature. Simple physical laws (energy, matter, inheritance, mutation) produce producers, predators, parasites, decomposers and symbionts through selection alone. The player shapes the world and its laws, never the creatures directly, and watches life respond.

This file holds everything decided so far. Section 13 has the working rules for Claude Code in this repo.

---

## 1. Core principles

1. **Energy is the universal currency.** Moving, reproducing, sensing, thinking, even existing costs energy. Entities that can't take in more than they spend die. Applied consistently, this one rule produces predator/prey dynamics, parasitism and symbiosis without anyone defining those categories.
2. **Emergence over prescription.** There is no `Animal` or `Plant` class. We write rules for how energy flows, how traits are encoded and inherited, and how entities interact with environmental gradients. Categories emerge from selection pressure.
3. **The genome as a program.** Behaviour and body are encoded in something evolvable. Mutation and recombination at reproduction are the source of novelty.
4. **Shape conditions, never creatures.** The player acts on the environment and the laws. Evolution decides what lives there.

---

## 2. Simulation laws

### 2.1 Two currencies: energy flows, matter cycles

- **Energy** enters from the sun (and later hydrothermal vents) and leaves as waste heat through metabolism. It never cycles.
- **Matter** (a few abstract nutrients, e.g. A, B, C) is finite. Building a body takes both energy and matter. When an entity dies, its matter stays locked in the corpse until something breaks it down.
- With energy alone you only get food chains. Matter is what gives **decomposers** a niche and makes nutrient-poor regions worth competing over.

**Artificial chemistry (later layer):** entities can secrete and absorb chemicals, and the genome specifies which reactions they catalyse (e.g. `A + energy → B`). One organism's waste becomes another's food. Cross-feeding, symbiosis and chemical warfare all fall out of the same rules.

### 2.2 Trade-offs: no free lunches

If any trait is free, one generalist takes over everything. Every trait must cost something:

| Trait | Benefit | Cost |
| --- | --- | --- |
| Size | More storage, harder to eat, retains heat | Higher maintenance, slower, costly to move uphill |
| Movement | Reach new resources | Cost ∝ mass × speed²; fast and big is very expensive |
| Sensing | Find food, avoid threats | Range and resolution cost energy every tick |
| Photosynthesis | Energy from light | Needs surface area; cheapest when stationary, which conflicts with moving |
| Armour / membrane | Protection | Slows absorption, adds weight |

**Heterogeneity is also required** for niches to form: light gradients, day/night, seasons, nutrient patches, terrain that blocks movement. A uniform world supports very few species.

### 2.3 One interaction primitive

There are no coded "eat", "attack" or "mate" actions. The single physical interaction is **contact plus transfer across membranes**. When two entities touch, energy and matter flow according to their traits (absorption strength vs. membrane resistance):

- Strong absorber vs. weak membrane looks like **predation**.
- Slow, small leakage from a large host looks like **parasitism**.
- Mutual secretion and absorption looks like **symbiosis**.

These are regions in trait-space, not code.

### 2.4 Genome design: body plus brain

| Option | Pros | Cons |
| --- | --- | --- |
| Number vector | Easy, stable | Bounded; only tunes what was anticipated |
| Tierra-style program | Truly open-ended | Hard to tie to bodies; brittle under mutation |
| Neural net | Great for behaviour | Awkward for body plans |

**Chosen hybrid:**
- **Body genome:** a list of modular parts (photosynthetic patch, absorber/mouth, flagellum, legs, sensor, armour, storage, later catalysts), each with parameters. Mutation can duplicate, delete or tweak parts. Duplication is how new structure appears.
- **Brain genome:** a small recurrent net (CTRNN). Inputs come only from sensors the body has, and outputs drive only effectors it has. A blind creature has no visual inputs.
- **Reproduction is a trait:** how much energy to invest per offspring and how many. r/K strategies emerge.

### 2.5 Prior art (study where they plateaued)

- **Tierra / Avida:** digital organisms as programs; the best source on open-endedness.
- **Polyworld** (Larry Yaeger): neural-net creatures with energy budgets.
- **The Bibites:** a modern, playable take on almost exactly this idea.
- **Lenia:** emergence from extremely simple continuous rules.
- **NEAT:** neuroevolution; its speciation method is reused here (section 9).

**The known open problem:** most ALife sims produce interesting dynamics, then **stagnate**. Mechanics aimed at sustained novelty (chemistry, gene duplication, environmental change via the god game) are this project's differentiator.

---

## 3. Milestone path and emergence tests

Each step either works or shows which law is too generous or too harsh.

1. **Light + LUCAs that photosynthesise and divide.** Test: population stabilises at carrying capacity, neither exploding nor dying out.
2. **Add the contact/transfer primitive.** Test: does something predator-like evolve without being coded?
3. **Add matter and corpses.** Test: do decomposers appear and nutrient cycles form?
4. **Add brains and sensors.** Test: does real behaviour emerge (chasing, fleeing, foraging)?
5. **Add the chemistry layer.** Test: cross-feeding or symbiosis.

The first concrete emergence check for step 1: under a **light gradient**, creatures in bright and dim regions should drift apart in photo-patch size and body size, with no code telling them to.

---

## 4. Player role: god game

### 4.1 Powers

Every power acts on the environment or the laws, never directly on organisms.

| Category | What the player controls | Effect on evolution |
| --- | --- | --- |
| Geography | Raise mountains, carve rivers, split or merge continents | Barriers isolate populations: the most reliable way to get two species from one (allopatric speciation) |
| Energy | Sun intensity, day length, season length/severity; place hydrothermal vents | Changes what's possible; vents allow ecosystems that never see the sun |
| Matter | Meteor impacts seeding new chemicals, volcanoes releasing nutrients, erosion moving matter downhill | Changes what bodies can be built from |
| Catastrophes | Ice ages, impact winters, toxic blooms | Mass extinction, then adaptive radiation into emptied niches |
| Laws (late-game, rare) | Mutation rate, metabolic cost multipliers, diffusion rates | "Divine" powers that affect everything; used sparingly |

### 4.2 Pacing

- **Changes are gradual.** A climate shift plays out over thousands of ticks so populations can track it. An ice age every 30 seconds would just cause extinction.
- **Powers cost influence.** Influence is earned from the health of the world (biodiversity, trophic levels, genome complexity), so the player is rewarded for richer ecosystems.
- **Time control is central:** fast-forward through deep time, then pause and zoom in on one creature.

### 4.3 Observation tools (core features, not polish)

- **Live tree of life:** phylogeny branching in real time, extinct lineages greyed out.
- **Automatic food web:** built from actual energy transfers; nothing is labelled "predator".
- **Creature inspector:** body parts, energy budget, brain firing as it moves.
- **Heatmap overlays:** light, each chemical, temperature, population density.
- **Natural history chronicle:** an auto-written log like Dwarf Fortress's Legends mode, e.g. *"Tick 41,200: lineage Velox splits in two across the Eastern Range."* Species get procedural names.

**Event detectors** power all of these and double as milestone tests: speciation, first predator (first sustained creature-to-creature energy flow), new trophic level, extinction.

### 4.4 Goals: sandbox plus scenarios

Scenarios are checked automatically by the event detectors:
- Evolve something that moves in under 10,000 ticks.
- Build a food chain five levels deep.
- Keep life alive through a snowball Earth.
- Get two lineages into a symbiosis.
- Start from a dead planet with vents only.

### 4.5 Prior art

SimEarth is the closest ancestor; Equilinox and The Bibites are also worth studying. None combines a deep emergent sim with god-level control.

---

## 5. Creature sharing

Players export creatures as files, share them online, and import them into their own worlds.

- **Imported creatures are invasive species.** The player introduces a creature but doesn't design it; evolution takes over (think cane toads in Australia). This keeps principle 4 intact. Examples: a vent-dweller tested in a friend's ice world; a friend's apex predator wrecking a food web, or starving because something it needs is missing.
- **Physics is part of the creature.** Each file carries a **physics fingerprint** (its home world's law parameters and chemistry). On import, the game warns about mismatches, e.g. *"Evolved under 1.4× sunlight with chemical C available. Your world has neither."*
- **The file contains:**
  - **Genome only, never derived stats.** Stats are recomputed under the destination's physics. This doubles as anti-cheat: a hand-edited "god creature" still pays its energy costs.
  - **A small population (~20 individuals)** to avoid a founder effect and keep genetic variation.
  - **Lineage metadata:** species name, origin world, author, tick count, a slice of its family tree.
  - **A format version** with migration code so old files keep loading.
- **Format:** follow Spore and embed the creature's data inside a **PNG portrait** of it; dropping the image into the game imports it. A readable JSON format underneath keeps it moddable.
- **Safety:** if genomes ever become Tierra-style executables, a downloaded creature is a stranger's program. It runs only in the game's sandboxed VM with a capped instruction budget, never as native code. Imports are validated and size-limited.
- This also connects to Uday's separate creature-design game idea: friends challenging each other to invade one another's worlds.

---

## 6. Creatures and world

Creatures are soft 3D blobs on a 3D heightmap world with cliffs, valleys and climates. **The simulation is 2.5D; only the rendering is full 3D.**

### 6.1 Why 2.5D

- **Rejected: full physics bodies.** Articulated rigid bodies evolving joint control (Karl Sims, *Evolved Virtual Creatures*, 1994) allow dozens of creatures, not thousands, and most evolution time goes into learning not to fall over.
- **Chosen: 2.5D simulation.** Creatures live on the terrain surface. The sim tracks position, height and slope, and movement is computed from traits. The renderer turns each creature into an animated 3D blob.

### 6.2 Body plan: core blob plus visible modules

| Body part | What it does in the sim | How it looks | Terrain and climate interaction |
| --- | --- | --- | --- |
| Core blob | Size, energy storage | Sphere deformed by shape genes | Bigger = more heat retained, heavier uphill |
| Photosynthetic patch | Light → energy | Translucent green spots | Needs open sky; useless in shaded valleys/caves |
| Absorber (mouth) | Pulls energy and matter on contact | Ring opening or cilia fringe | — |
| Flagellum | Movement | Whip-like wiggling tail | Efficient in water, poor on land |
| Leg stubs | Movement | Short tentacle-like limbs; crawl or hop | Efficient on land, can climb |
| Eyespot / sensor | Sensing range | Dark dots; bigger = longer range | Cliffs and ridges block line of sight |
| Armour | Resists absorption | Darker matte plates | Adds weight; climbing costs more |
| Storage | Fat reserves | Blob visibly swells | Survives winters and scarcity |
| Pigment | Heat absorption | Skin colour, light to dark | Dark: good in cold, bad in hot |

- **Colour is a real trait.** Dark lineages should emerge in cold highlands and pale ones in hot lowlands, so colours mean something and the world is readable from a distance.
- **Fat is visible.** Blobs swell before winter and shrink during it.

### 6.3 Terrain and climate as niche generators

- **Slopes:** climbing costs ∝ mass × height gained. Cliffs block heavy creatures but not light ones, isolating populations for free.
- **Valleys:** nutrients wash downhill and collect there; cold air settles. Rich but cold.
- **Altitude and latitude:** temperature drops with both. Each genome has a thermal tolerance range; outside it, metabolism costs more.
- **Water:** flagella work in water and legs on land, so colonising land becomes a visible evolutionary event.
- **Shade:** steep valley walls and overhangs block sunlight, pushing photosynthesisers onto ridges and plains.

### 6.4 Making blobs feel alive

- **Shared mesh:** one base blob mesh deformed per instance in the vertex shader (see 7.4). Unique high-detail meshes only for the inspected creature and for portraits.
- **Shader jiggle:** squash-and-stretch when moving, breathing tied to metabolic rate, wobble on contact. A soft-body look without soft-body physics.
- **Animation comes from parts:** flagellates wiggle, leg-stubbed creatures crawl or hop, immobile ones sit or roll downhill.
- **Distant creatures** become coloured dots.
- **Portraits:** the same mesh renders the PNG portrait used in creature files.

### 6.5 Architecture

- **Sim core in C++** (a GDExtension or separate library): heightmap grid, climate fields, creatures, energy accounting. It has its own tick rate and fast-forwards headless.
- **Godot world view:** terrain mesh from the heightmap, creatures interpolated from sim positions, overlays and inspector UI on top.
- **Starting scale:** a 512×512 heightmap, 10,000 creatures as the baseline benchmark, no population cap.

---

## 7. Performance targets

**10,000 creatures running with ease is the baseline benchmark, not a ceiling.**

### 7.1 No upper limit

- **There is no population cap.** The only limit on how many blobs exist is the **ecological equilibrium itself**: total energy input minus metabolic costs. Tuning the sun or costs moves the equilibrium; no counter removes or blocks creatures.
- **If the equilibrium exceeds what the machine can run in real time, the simulation slows down rather than capping.** Sim time is decoupled from real time.
- **Every system must scale roughly linearly** with population, so there's no size where performance collapses.

### 7.2 Budget

At 10k creatures, 60 ticks/s with 10× fast-forward is 600 ticks/s. That's about 0.15 µs per creature per tick on one core, or 1–2 µs spread over 8 threads.

### 7.3 Rules that make it possible

1. **Creatures are not Godot nodes.** They're plain data in the C++ core; Godot only draws them.
2. **Data-oriented layout:** arrays of fields, not arrays of objects (often a 5–10× speedup alone).
3. **Spatial hashing:** contact and sensing look only at nearby cells. Interaction cost stays linear (~100k checks per tick at 10k, not 100 million).
4. **No physics raycasts for sensing:** use a precomputed coarse visibility grid or a few heightmap samples.
5. **Small, costly brains:** ~20 inputs, ~10 hidden, ~8 outputs is a few hundred multiply-adds. Neurons cost energy, so brains grow only when intelligence pays.
6. **Two-phase, tile-parallel updates:** phase 1, every creature reads current state and decides; phase 2, apply changes. No race conditions, and the sim stays deterministic.
7. **Fields update every 5–10 ticks:** a 512×512 grid is 262k cells; diffusing every tick wastes work.

### 7.4 Rendering: one draw call

- **One base blob mesh**, deformed in the vertex shader from per-instance data (size, shape genes, pigment, fat, jiggle phase) via MultiMesh custom data. All blobs render in one draw call regardless of lineage count.
- **Body parts as separate instanced meshes**, one MultiMesh per part type.
- **Unique high-detail meshes** only for the inspected creature and portraits.

### 7.5 Stress test

The stress-test blobs *are* LUCAs plus a small random wander. They must run at a high tick rate headless and stay smooth on screen. Then push the population well past 10k to confirm linear scaling.

---

## 8. The creature model

### 8.1 Three layers

| Layer | What it is | Changes when | Examples |
| --- | --- | --- | --- |
| **Genome** | Inherited blueprint | Only at birth, via mutation | Core radius, pigment, parts list |
| **Phenotype** | Stats derived from the genome | Computed once at birth, then fixed | Mass, upkeep, photosynthesis rate, max speed |
| **State** | Current situation | Every tick | Position, energy, age, fat level |

Creature files export the genome only.

### 8.2 Genome parameters

| Parameter | Controls | Trade-off |
| --- | --- | --- |
| `core_radius` | Body size | Stores more, survives longer, but costs more to maintain and move |
| `shape[8]` | Lumps and elongation | Visual at first; later surface area or drag |
| `pigment` | Skin darkness | Absorbs heat: good in cold, bad in hot |
| `thermal_optimum` | Preferred temperature | Which climates are cheap to live in |
| `thermal_tolerance` | Width of comfort range | Wide costs extra upkeep; specialists are cheaper |
| `repro_threshold` | Energy needed before dividing | Low = many weak offspring; high = few strong ones |
| `offspring_share` | Energy fraction given to child | Same r/K trade-off from the parent's side |
| `parts` | Body modules | Each adds mass and upkeep |
| `brain` | Neural net weights | Empty for LUCA; neurons cost energy |

**Likely additions:** mutation rate as a gene (evolvable mutability), reproduction mode (asexual, or sexual with a mate-preference gene), chemistry genes (which reactions a creature catalyses).

**Rule for any new gene:** it must have a real cost in the phenotype compiler. A gene that is pure upside takes over every lineage and destroys diversity.

### 8.3 LUCA: the base creature

The last universal common ancestor: a core blob with one photosynthetic patch, no movement, no brain, dividing when it has enough energy. Every lineage descends from it. Legs, mouths, sensors and predation must come from mutation.

### 8.4 Phenotype compiler (genome → stats, once at birth)

- mass ∝ radius³, surface area ∝ radius²
- upkeep ∝ mass^0.75 (Kleiber's law)
- photosynthesis ∝ patch area × local light, limited by surface area
- every part adds mass and upkeep
- clamp the genome to sane ranges in exactly one place (compiler or mutation)

### 8.5 Tick (per creature, per tick)

1. **Energy in:** light, shared within the cell (finite light = carrying capacity).
2. **Energy out:** upkeep, plus thermal cost outside the tolerance range, plus movement and slope cost.
3. **Energy ≤ 0:** dies (later: becomes a corpse holding its matter).
4. **Energy ≥ threshold:** divides; the child gets a mutated genome and its energy share.

### 8.6 Mutation operators

- **Tweak** (most common): small Gaussian nudges to float genes.
- **Add / delete / duplicate a part** (less common). Duplication is how new structure appears.
- **Change a part's type** (rare).
- Rates are hard-coded at first; later they can become heritable genes or god-mode "law" knobs. Cap or price part counts so genomes can't bloat forever.

---

## 9. Speciation

### 9.1 Two modes

- **Asexual (start here):** no interbreeding, so species are **clusters in genome space** detected by the game. Labels drive the tree of life, chronicle and colours, but don't change sim behaviour.
- **Sexual (add later):** one rule makes species real: **two creatures can only produce viable offspring if their genomes are similar enough** (fertility falls with genetic distance, reaching zero past a threshold). Then a population split by a mountain diverges; when the mountain erodes, the halves either merge (with a hybrid zone) or stay as two true species. Neither outcome is scripted.

### 9.2 How divergence happens here

- **Allopatric:** barriers (cliffs, rivers, player-raised mountains).
- **Parapatric:** along gradients, like altitude getting colder.
- **Sympatric:** a niche split in the same place; with a mate-preference gene, populations can stop interbreeding without geography.
- **Founder effect:** a few colonists, or an imported creature file, diverge fast.
- **Adaptive radiation:** after mass extinction.

### 9.3 Detection

1. **Genome distance function:** weighted differences in float genes plus a parts-list comparison (counts per type, differences between matched parts). The same function drives fertility, species detection and colour clustering.
2. **Species IDs at birth (NEAT-style):** each species has a representative genome. A newborn inherits its parent's ID; if its distance to the representative passes the threshold, it founds a **new species** and the chronicle logs it. One distance check per birth.
3. **Periodic split checks:** every few thousand ticks, sample each species and check whether it's forming two separate clusters (e.g. either side of a mountain). If so, split it and give the new one a procedural name.

---

## 10. Implementation status

### 10.1 Repo layout (sim core scaffolding, built 2026-11-06)

| Path | What it is | Owner |
| --- | --- | --- |
| `core/genome.*` | Genome + `make_luca()` | Scaffolding (extend freely) |
| `core/phenotype.*` | Genome → derived stats | **Uday** (placeholder) |
| `core/energy.*` | Light demand, upkeep, thermal, movement costs | **Uday** (placeholder) |
| `core/mutation.*` | Parent genome → child genome | **Uday** (placeholder) |
| `core/world.*` | Heightmap (fBm value noise), light and temperature fields | Scaffolding |
| `core/creatures.hpp` | Data-oriented creature storage (one array per field) | Scaffolding |
| `core/spatial_grid.hpp` | Per-cell buckets, counting sort, O(N) per tick | Scaffolding |
| `core/sim.*` | Two-phase tick, births/deaths, light sharing | Scaffolding |
| `core/metrics.*` | CSV metrics, energy audit, end-of-run snapshot | Scaffolding |
| `core/rng.hpp` | Deterministic xoshiro256** | Scaffolding |
| `core/config.hpp` | World settings and units | Scaffolding |
| `headless/main.cpp` | CLI runner | Scaffolding |
| `tools/plot.py` | `run.png` + `map.png` | Scaffolding |

Stack: C++20, CMake, Python + matplotlib for plots. The core has **no Godot dependency**.

### 10.2 Build and run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/luca_headless --ticks 20000 --out out      # --help for all options
python3 tools/plot.py out
```

Options: `--ticks --seed --size --pop --light uniform|gradient --sun --interval --out --no-snapshot --quiet`.

### 10.3 Units (proposed; settle before tuning)

- 1 tick = 1 minute of world time
- 1 energy unit = what one cell receives per tick under full sun
- 1 cell = `cell_size` world units (default 1)

### 10.4 Rules the scaffolding enforces

- **Finite light:** each cell gets `light` energy per tick, shared equally among creatures if over-subscribed. Unabsorbed light is lost.
- **Costs capped at available energy;** a creature at zero dies.
- **Energy audit:** each metrics interval checks `stored(end) == stored(start) + in − out`. Expected error ≈ 1e-6 (float rounding). A jump means energy is being created or destroyed, and evolution will exploit it.
- **Reproduction transfers energy** (never creates it).
- **Determinism:** same seed + build + machine → identical runs (verified). Cross-machine determinism is not a goal yet; creature files don't need it.

### 10.5 Baseline numbers (placeholder rules, default settings)

- Population plateaus at about 8,700 on 128×128. Life stops where light falls below upkeep (≈ y = 80 in gradient mode).
- About 0.2 ms/tick at 8.7k creatures and 0.9 ms/tick at 33k (256×256): linear scaling.
- Audit error ≈ 1e-6.

---

## 11. Work split and next steps

**Agreed split:** Claude writes scaffolding (structure, storage, tick loop, metrics, tools). **Uday writes the interesting parts:** phenotype compiler, energy rules, mutation. These are marked `TODO(Uday)` in the code with notes.

### Uday's tasks, in order

1. **Phenotype compiler** (`phenotype.cpp`): clamping, Kleiber scaling, per-part costs, light capture limited by surface area.
2. **Energy rules** (`energy.cpp`): real upkeep, thermal cost outside tolerance, movement cost with slope.
3. **Mutation** (`mutation.cpp`): tweak, add, delete, duplicate, retype parts.
4. **Emergence test 1:** `--light uniform`. Population rises and plateaus.
5. **Emergence test 2:** `--light gradient`. The sunny-half and dim-half photo-size lines in `run.png` drift apart.

### Next scaffolding (after Uday's parts work)

- Genome distance function + NEAT-style species IDs + split checks
- Contact interaction using the spatial grid
- Corpses and matter
- Multithreading phase 1 by world tiles
- GDExtension + MultiMesh renderer (shared deformed blob mesh)

---

## 12. Open questions

- **Reproduction mode timing:** when to add sexual reproduction, and how mates find each other.
- **Death and decay:** how corpses hold matter and how fast they release it.
- **Tuning workflow:** headless parameter sweeps with logged metrics.
- **Save/load:** serialising full world state (follows from determinism).
- **Later:** camera and UX, water dynamics, the chemistry layer, influence formula details.

---

## 13. Working agreement for Claude Code in this repo

- **Do not implement the `TODO(Uday)` parts** (phenotype compiler, energy rules, mutation) unless Uday explicitly asks. He's writing those himself to learn. Help by reviewing, explaining, suggesting designs, and writing tests around them.
- **Keep the core free of Godot.** `core/` must compile with only the standard library.
- **Determinism:** use only the `Rng` passed in; never `std::rand`, `random_device`, time, or unordered-container iteration order in sim logic.
- **Never break the energy audit.** Any change that moves energy must be reflected in `TickStats`. Run a headless check after sim changes and confirm audit error stays ≈ 1e-6.
- **Data-oriented and linear:** no per-creature heap objects in the hot path; no O(N²) loops; neighbour queries go through the spatial grid.
- **No population caps,** ever. Limits come from ecology only.
- **Every new gene needs a cost** in the phenotype compiler.
- **Before finishing a change:** build with no warnings, run `./build/luca_headless --ticks 5000`, and check population behaviour, audit error and ms/tick against the baseline in 10.5.
