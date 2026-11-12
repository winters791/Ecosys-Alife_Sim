# Ecosystem God Game — Design & Project Reference

Owner: Uday · Last updated: 2026-11-12

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
| Movement | Reach new resources | Cost ∝ mass × (distance + speed²): getting anywhere costs, and fast and big costs most |
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
- **Brain genome:** a small recurrent net (planned as a CTRNN; built 2026-11-11 as an evolvable discrete-time net, see 10.4). Inputs come only from sensors the body has, and outputs drive only effectors it has. A blind creature has no visual inputs.
- **Reproduction is a trait:** how much energy to invest per offspring and how many. r/K strategies emerge.

### 2.5 Prior art (study where they plateaued)

- **Tierra / Avida:** digital organisms as programs; the best source on open-endedness.
- **Polyworld** (Larry Yaeger): neural-net creatures with energy budgets.
- **The Bibites:** a modern, playable take on almost exactly this idea.
- **Lenia:** emergence from extremely simple continuous rules.
- **NEAT:** neuroevolution; its speciation method is reused here (section 9).

**The known open problem:** most ALife sims produce interesting dynamics, then **stagnate**. Mechanics aimed at sustained novelty (chemistry, gene duplication, environmental change via the god game) are this project's differentiator.

---

### 2.6 Open-endedness mechanics (decided 2026-11-11, build after milestones 2–4)

Four additions aimed at the stagnation problem. Each is one physical rule with a cost, not a scripted feature; none tells a creature what to do with it.

| Mechanic | The rule | Its cost | What might emerge |
| --- | --- | --- | --- |
| **Niche construction** | Living things change the fields they live in: photo patches shade the cell beneath them, corpses add matter to the soil, armoured dead pile up and raise the terrain | Nothing extra; it's a side effect of living and dying | Forests with shaded understoreys, fertile graveyards, reefs. The map itself keeps evolving |
| **Adhesion** | A part that keeps two touching blobs attached; a little energy and matter leaks across the join | Mass, upkeep, and a joined pair moves as one heavier body | Chains, mats, colonies, and cells that specialise: multicellularity |
| **Signals** | An emitter part writes a pulse into a scent field (a few channels) that diffuses and fades; sensors can read it | Energy per pulse; anyone can read it, including things that eat you | Herding, alarm calls, lures, mimicry. No signal has a built-in meaning |
| **Carry and deposit** | A part that picks matter up from a cell and puts it down in another | Carried matter is extra mass; pick-up and drop cost energy | Mounds, dams, nests, hoards: evolved architecture |

Order of dependency: niche construction and carrying need matter (milestone 3); signals need brains and sensors (milestone 4); adhesion needs the contact primitive (milestone 2).

---

## 3. Milestone path and emergence tests

Each step either works or shows which law is too generous or too harsh.

1. **Light + LUCAs that photosynthesise and divide.** Test: population stabilises at carrying capacity, neither exploding nor dying out. *Built; passes.*
2. **Add the contact/transfer primitive.** Test: does something predator-like evolve without being coded? *Built 2026-11-11. Yes: bodies with mouths that drain their neighbours appear within ~15,000 ticks, and mobile ones with little or no photo patch follow.*
3. **Add matter and corpses.** Test: do decomposers appear and nutrient cycles form? *Built 2026-11-11 (one nutrient, corpses as per-cell fields). Creatures living mostly on the dead do appear, but stay rare (a handful at a time); most corpse-eating is done by mixed feeders.*
4. **Add brains and sensors.** Test: does real behaviour emerge (chasing, fleeing, foraging)? *First version built 2026-11-11: a one-layer brain, not yet a CTRNN. Movers evolve and persist; whether they chase or flee has not been measured.*
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

SimEarth is the closest ancestor; Equilinox and The Bibites are also worth studying. None combines a deep emergent sim with god-level control. **WorldBox** is the model for *feel*: pick a brush, poke the world, watch what happens, no menus in the way.

### 4.6 Two modes: Sandbox and Scenario (decided 2026-11-11)

- **Sandbox:** every power is a free, instant brush. No influence, no cooldowns. This is the WorldBox toy and the default way to play.
- **Scenario:** the same powers, but they cost influence and play out as slow ramps (4.2), and a goal from 4.4 is being checked.
- Both modes run on one mechanism: a power is a command with a `duration` (0 = instant). Sandbox just sets it to 0.
- Brushes: height, heat, and later water, nutrients and vents.

### 4.7 A world that acts on its own

Storms, eruptions, slow climate drift and the odd meteor happen without the player, at a rate set by a slider (off is allowed). They are ordinary power commands cast by the world from the sim's `Rng`, so they are deterministic, logged and replayable like the player's. The point: you come back to a world with a history you didn't write.

### 4.8 Favourites and the fossil museum

- **Favourites:** pin a lineage to follow it through deep time, with alerts when something happens to it (*"Your Velox just lost its eyes"*). Observation only; a favourite gets no help.
- **Fossils:** an extinct species leaves fossils where it lived. Digging one up adds its portrait and a short life story to a museum collection.

### 4.9 Seed vault and life-bearing meteors

Bank any living lineage (a ~20-individual sample, same as a creature file) in a vault, and drop it back into this world or a friend's as a meteor. A reintroduction is an invasive species under exactly the rules of section 5: genome only, stats recomputed, evolution takes over. This is also how imported creature files arrive in the world.

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

> **Look changed 2026-11-11 (Uday):** the game is **top-down pixel art, like WorldBox**, not 3D. (A soft-3D view was tried later the same day and reverted at Uday's request; it is in git history at commits 676c363 and 8506a75.) Creatures are small pixel sprites and the ground is pixel tiles. The simulation is unchanged (still 2.5D: a heightmap, slopes, climates). Read "3D blob", "mesh" and "shader jiggle" below as history; 6.6 is how it works now.

Creatures are soft blobs on a heightmap world with cliffs, valleys and climates. **The simulation is 2.5D.**

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
| Poison | Costs whatever drains the body energy | Violet warts on the skin | — |
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

### 6.6 Pixel bodies (decided and built 2026-11-11)

- **The sprite is drawn from the body, not stored.** Each creature is one quad; `blob.gdshader` paints its pixels from the creature's numbers (radius, shape genes, pigment, part sizes, heading, fullness). No per-creature texture or mesh, so ten thousand creatures are still one draw call and a body that evolves a new part simply starts being drawn with it.
- **One pixel grid for everything:** 8 pixels to a cell (`PX_SIZE`), shared by the ground and the sprites, and sprites snap to it.
- **The outline is drawn from the shape genes (2026-11-12):** a superellipse with the volume of the round body, up to three times as long as wide (`elongation`) and up to a box with rounded corners (`squareness`). Two lump genes add small bumps. It faces the way the creature faces (`hx`, `hy`).
- **Every part is drawn where it sits:** at its own angle on the outline (`Part::theta`, from the front), and out on its stalk if it has one. Photo = green pixels over the arc of skin it covers, or a leaf on a stalk (a frond); absorber = a dark red mouth on the rim (teeth when large), or a toothed ring at the end of a tentacle; flagellum = a wiggling tail; legs = a mirrored pair of stubs that step; sensor = an eye towards its side (two when large); armour = grey plates over the arc of rim it covers; poison = violet warts over its arc; storage = a pale fat spot. Pigment is skin colour, fullness is body size. Arcs match `core/body.hpp`.
- **Parts reach the shader through a data texture:** 12 texels per creature in instance order, each (type + 1, size, angle, stalk), from `LucaWorld::get_part_buffer`. Still one draw call.
- `./view.sh --parade` opens an empty world with seven hand-made bodies (a LUCA, a long swimmer, an armoured box, a tentacle monster, a frond, a walker, a fat poisonous one) to check the art. `--inspect=x,z` opens the inspector on the creature nearest a spot.
- **Zoom is in whole screen pixels per sprite pixel** where it can be, so the art stays crisp.

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
| `bite` | Drains the living faster: pull × (1 + bite), up to ×4 (LUCA: 0) | Upkeep 0.06 per unit, paid whether or not there is anything to bite |
| `digestion` | Keeps more of a meal, living or dead: 70% up to 95% (LUCA: 0) | Upkeep up to 0.10 |
| `detox` | Takes up the waste in its cell as food, 0 to 1 (LUCA: 0) | Upkeep up to 0.08, and it is only worth it where others foul the cell |
| `dormancy` | How deeply it sleeps through dark and cold, 0 to 1 (LUCA: 0) | A little upkeep always; asleep it is helpless, and it wakes slowly, so it misses the dawn |
| `seed_throw` | How far a child can be flung, in cells (LUCA: 0) | Upkeep per cell of range, plus energy ∝ child mass × distance² for every throw |
| `elongation` | Up to 3× as long as wide, same volume (0..1, LUCA: 0) | More skin (sun room, but heat loss, fouling, `SKIN_UPKEEP`); a long body moves more cheaply and faster (2026-11-12) |
| `mutability` | How sloppily it is copied: every mutation chance × 2^mutability, from −3 (an eighth) to +2 (four times) (LUCA: 0) | Below 0, `FIDELITY_UPKEEP` 0.02 per halving (proofreading); above 0 free, paid for in broken children (2026-11-12) |
| `repair` | Slows wear: the age clock runs 1 + 2 × repair times slower, 0 to 1 (LUCA: 0) | Up to 15% more upkeep (`REPAIR_SHARE`), so a bigger body pays more |
| `squareness` | Round to box (0..1, LUCA: 0) | Up to 24% more skin, and 30% more drag on the move |
| `parts` | Body modules, each with a size, an angle on the outline (`theta`) and, for mouths and sun patches only, a stalk | Each adds mass and upkeep; a stalk adds mass and upkeep per unit of length. Where a part sits matters (10.4) |
| `brain` | Hidden neuron count and a list of weighted links, both evolved | Empty for LUCA; every neuron, link and unit of weight costs upkeep |

**Likely additions:** reproduction mode (asexual, or sexual with a mate-preference gene), chemistry genes (which reactions a creature catalyses).

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
- Rates are scaled per lineage by the heritable `mutability` gene (2026-11-12); a god-mode "law" knob could scale them for the whole world later. Cap or price part counts so genomes can't bloat forever.

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
| `core/genome.*` | Genome, brain weight layout, gene limits + `clamp_genome()`, `make_luca()` | Scaffolding (extend freely) |
| `core/body.*` | Where parts sit: per-direction mouth and armour tables (`BodyPlan`), thrust by placement, skin and drag of a shape | Claude, at Uday's request (2026-11-12); Uday tunes |
| `core/phenotype.*` | Genome → derived stats | **Uday** wrote steps 1–6; Claude added contact, storage, senses, brain (2026-11-11) |
| `core/energy.*` | Light demand, upkeep + wear, thermal, movement, contact flow, corpse rules | Claude, at Uday's request (2026-11-11); Uday tunes |
| `core/mutation.*` | Parent genome → child genome | Claude, at Uday's request (2026-11-11); Uday tunes |
| `core/world.*` | Heightmap (fBm value noise), light and temperature fields, time of day | Scaffolding |
| `core/soil.hpp` | Per-cell free nutrient, corpse matter, corpse energy and waste | Scaffolding (built 2026-11-11) |
| `core/creatures.hpp` | Data-oriented creature storage (one array per field) | Scaffolding |
| `core/spatial_grid.hpp` | Per-cell buckets, counting sort, O(N) per tick | Scaffolding |
| `core/sim.*` | Two-phase tick, births/deaths, light sharing | Scaffolding |
| `core/metrics.*` | CSV metrics, energy audit, end-of-run snapshot | Scaffolding |
| `core/powers.*` | God powers: commands, ramps, command log, script format | Scaffolding (built 2026-11-11) |
| `core/chronicle.hpp` | Append-only natural history log | Scaffolding (built 2026-11-11) |
| `core/species.*` | Genome distance, species registry (ids, names, parents, populations) | Scaffolding (built 2026-11-11); Uday tunes the weights |
| `core/rng.hpp` | Deterministic xoshiro256** | Scaffolding |
| `core/config.hpp` | World settings and units | Scaffolding |
| `headless/main.cpp` | CLI runner | Scaffolding |
| `tools/plot.py` | `run.png` + `map.png` | Scaffolding |
| `godot/` | GDExtension glue: `LucaWorld` wraps the sim and hands Godot packed arrays | Scaffolding (built 2026-11-11) |
| `viewer/` | Godot 4.7 project: `main.gd` (camera, brushes, UI), `tree_view.gd` (tree of life), `species_card.gd` (species card), terrain and blob shaders | Scaffolding (built 2026-11-11) |
| `view.sh` | Build the extension and open the viewer | Scaffolding |
| `tests/test_powers.cpp` | Brush, ramp, script, determinism and audit tests (`./build/test_powers`) | Scaffolding |
| `tests/test_life.cpp` | Day/night, mutation, costs, movement, contact, scavenging, species, energy and matter books (`./build/test_life`) | Scaffolding (built 2026-11-11) |
| `scripts/long_winter.txt` | Demo god-power script | Scaffolding |

All paths are inside `luca/`.

Stack: C++20, CMake, Python + matplotlib for plots, Godot 4.7 + godot-cpp for the viewer. The core has **no Godot dependency**; `godot/` depends on the core, never the reverse.

### 10.2 Build and run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/luca_headless --ticks 20000 --out out      # --help for all options
python3 tools/plot.py out
```

Options: `--ticks --seed --size --pop --light uniform|gradient --sun --day --day-swing --windfall --nutrients --interval --script --out --no-snapshot --quiet`.

God powers, headless: `./build/luca_headless --ticks 16000 --script scripts/long_winter.txt`. Each run also writes `chronicle.txt`, `powers.txt` (every command cast, replayable with `--script`) and `species.csv` (every species there has ever been, with its parent: the tree of life as a table).

### 10.3 Units (proposed; settle before tuning)

- 1 tick = 1 minute of world time
- 1 energy unit = what one cell receives per tick under full sun, **averaged over a day**. With the day/night cycle noon is twice that and night is zero.
- 1 day = `day_length` ticks (default 600, so the clock shown in the viewer is not literal minutes; `--day 0` turns the cycle off)
- 1 cell = `cell_size` world units (default 1)
- For scale: a LUCA burns about 0.35 a tick (0.46 before `METABOLIC_RATE` went to 0.75), holds up to 650, divides at 600, and its body is worth 244 (since 2026-11-11; before that 450, 400 and 44).

### 10.4 Rules the scaffolding enforces

- **Finite light, canopy rule (2026-11-11):** each cell gets `light` energy per tick. If the bodies in it ask for more, the tallest (`Phenotype::canopy`: core radius, plus the stalks its sun patches stand on, since 2026-11-12) takes its fill first, then the next, and smaller ones get what is left; bodies of exactly the same size share alike. A full-patch LUCA asks for the whole cell, so a smaller body under it gets nothing unless the taller one's patch is sparse. Unabsorbed light is lost. (`Simulation::share_light_by_canopy`)
- **Costs capped at available energy;** a creature at zero dies.
- **Energy audit:** each metrics interval checks `stored(end) == stored(start) + in − out`. Expected error ≈ 1e-6 (float rounding). A jump means energy is being created or destroyed, and evolution will exploit it. "Stored" is energy in living creatures + energy locked in their bodies + energy left in corpses + energy in waste. "In" is light plus windfalls (`in_windfall`). "Out" is upkeep (less the part given off as waste), thermal, movement, transfer loss, corpse decay, waste breaking down, fouling, poison and overflow (`out_waste`: a full body can't hold more; not the waste chemical).
- **Matter audit:** soil nutrient + corpse matter + the mass of everything alive is constant (`Simulation::total_matter()`, column `matter_total`).
- **Reproduction transfers energy and matter** (never creates them): the parent pays the child's energy share plus the energy to build its body, and the body's mass comes out of the soil in the parent's cell. No free matter there, no child. A body tries once every 8 ticks.
- **Dispersal is paid for (2026-11-11):** a child lands touching its parent (their two reaches apart, about one cell for LUCAs). The `seed_throw` gene flings it up to that many cells further, at a cost (`throw_cost`, booked under movement). `offspring_spread` is gone. A child's path stops at any step in the ground taller than `birth_max_step` (4 height units between neighbouring cells; natural terrain tops out near 3.1), so a tall raised mountain blocks births as well as walkers.
- **Death leaves a corpse** in the cell: its mass, and the energy that was locked in the body (222 per unit of mass, so 244 for a LUCA). Corpses weather away slowly on their own (0.4% every 8 ticks); anything with an absorber breaks them down faster, and matter returns to the soil in proportion to the energy eaten.
- **Windfalls (2026-11-12, Uday's request):** at the start of every tick, after powers, food falls on random cells: `windfall_rate` (0.001, `config.hpp`) energy per cell per tick on average, in lumps of `windfall_size` (250, about one LUCA corpse), so one lump every ~16 ticks on a 128×128 map. The number of lumps per tick is the whole part of the mean plus one more by chance, and each lands on a uniformly random cell; all from the sim `Rng` (`Simulation::drop_windfalls`). A lump goes into the cell's corpse energy **with no matter**, so it is eaten, sensed (the "corpse here" input and the lean towards the dead) and weathered exactly like a corpse, and the matter books don't change. It is the second way energy enters the world: booked in `energy_in` and broken out as `in_windfall` (metrics columns `in_windfall`, `windfalls`). Because it sits in the corpse field, food from it counts as `fed_dead`: a body living on windfalls shows in the census as a scavenger. `--windfall 0` turns it off.
- **Slope shade (2026-11-11):** the sun stands over the y = 0 edge. A cell loses light for dropping away from the sun (15% per unit of height per cell, up to 50%) and for lying below its rim, the ground three cells out (6% per unit of depth, up to 40%). `World::shade()` is a static field, recomputed around a terrain brush stroke only; `slope_shade` in the config scales it (0 = off). On the default map the mean is 0.94 and the darkest cell 0.53.
- **Matter washes downhill (2026-11-11):** every 64 ticks a cell sends `wash_rate` (0.001) × steepness of its free nutrient to its lowest neighbour (`World::downhill()`, eight neighbours; steepness is the drop over 2 height units, capped at 1). Corpse matter stays put until it is freed. All taking is worked out before any giving, and `total_matter()` is unchanged.
- **Day and night:** `World::set_time(tick)` runs at the start of every tick, after powers. Daylight is a pure function of the tick (no randomness), 0 at night to 2 at noon.
- **Cold nights (2026-11-11):** the air everywhere swings by `temp_day_swing` (10 °C, peak to peak, in `config.hpp`; 0 = off) with the day, a sine an eighth of a day behind the sun: warmest mid-afternoon, coldest just before dawn, daily mean unchanged (`World::air_swing()`; `temperature()` and `env()` include it, `temperatures()` and `mean_temperature()` are daily means). No creature feels the air directly: each carries a `body_temp` that closes `heat_follow` of the gap to what it would feel (air plus sun on dark skin) each tick, where `heat_follow = 1 / (1 + HEAT_LAG × mass / skin)` (`HEAT_LAG` 120 in `phenotype.cpp`). A LUCA lags by ~130 ticks and feels about 60% of the night's chill; a radius-2 body about 30%. Thermal cost, the dormancy cold trigger and the brain's cold input all read body temperature. A child starts as warm as its parent. Not a new gene: size and fat already cost upkeep.
- **Contact:** a body with an absorber looks at the 3×3 cells around it. For each body within reach, energy flows at `edge × pull² / (pull + membrane)`, where `edge` is (taker radius / giver radius)², at most 2 (the size rule, 2026-11-11: equal bodies are unchanged, a body half its victim's size pulls a quarter as hard, and nothing is ever immune); 70% arrives, 30% is lost as heat. If several bodies pull on one, each gets the same fraction. Reach is capped at 1.5 cells, so very large bodies don't yet touch further.
- **Where parts sit matters (2026-11-12, `core/body.*`).** Every part has an angle on the outline. A body faces the way it last moved; one that never moves keeps the facing it was born with (from its id, like its first wander angle).
  - **A mouth pulls only within its arc:** half-width `MOUTH_ARC_BASE` (0.5 rad) + `MOUTH_ARC_PER_SIZE` (0.8) × size, all round from size 3.3. Contact looks up the taker's pull towards the giver and the giver's armour facing the taker in two 16-direction tables built at birth (`BodyPlan`, kept in `Creatures::body`, off the hot path). Corpses lie all round, so feeding on the dead still uses the whole pull.
  - **Armour guards only the arc it covers:** half-width `COVER_ARC` (π/2) × size, so size 1 covers half the body and size 2 all of it, at `ARMOUR_RESIST` × size where it covers (the old rule, everywhere, is the size-2 case). `Phenotype::membrane` is now the average over directions, for display.
  - **Armour clogs only what lies under it:** a mouth or a tail under a plate works at 1 / (1 + `ARMOUR_CLOGS` × armour there). Before, every mouth was clogged by all armour.
  - **Motors push by where they sit:** a tail gives full thrust straight behind, falling to a quarter at the front; legs full at the sides, half at front or back. New parts land at a random angle, so the first tails are badly placed and evolution moves them. **Except mouths:** a new mouth buds within `P_MOUTH_FRONT` (0.3 rad, `mutation.cpp`) of the front, so a mover's first mouth faces where it goes (added 2026-11-12; see the hunters numbers in 10.7). Copies and nudges still move mouths anywhere.
- **Shape matters (2026-11-12):** `elongation` and `squareness` keep the volume (and so the mass) and add skin (`skin_factor`: a 3:1 spheroid has 18% more, a box 24% more). Skin is `surface_area`, so it gives more room for sun patches, faster heat loss (`heat_follow`), more sun on dark skin, and more fouling; extra skin costs `SKIN_UPKEEP` (0.1) per unit. Moving cost (not climbing) and top speed scale with `drag_factor`: 1 / cube root of the aspect, × 1.3 for a full box. So a 3:1 body moves 0.69× as dear and 1.44× as fast.
- **Stalks (2026-11-12):** mouths and sun patches can grow on a stalk (up to `MAX_STALK` 1.0 world unit; `can_have_stalk`), at `STALK_MASS` 0.1 and `STALK_UPKEEP` 0.06 per unit of length. A mouth on a stalk reaches bodies further away in its arc (reach + stalk, still capped at 1.5) and, from `STALK_CLEARS` (0.25) up, is never clogged by armour. Sun patches on stalks raise the body's canopy height by their size-weighted stalk, so a frond overtops a plain body of the same size.
- **Movement:** only a body with a motor part *and* a brain moves. A motor part with no brain gets a small random one at the next mutation.
- **The brain is evolved, structure and all (2026-11-11, Uday's request).** The genome holds a number of hidden neurons (0 to `MAX_HIDDEN` = 8) and a list of links (up to `MAX_LINKS` = 48), each from an input or hidden neuron to a hidden neuron or an output, with a weight. Inputs (7): a constant, hunger, light, crowding, cold, corpse here, own speed. Outputs (11): how hard to lean towards light, the living, the dead, comfort, soil matter, kin, bigger bodies, moving bodies; wander; inertia; throttle. Hidden neurons are tanh and all update together from the inputs and last tick's hidden values, so a neuron wired to itself or to another is memory (a small discrete-time recurrent net, in place of the CTRNN of 2.4). With no hidden neurons it is a set of reflexes, which is where every brain starts.
- **Wander drifts (2026-11-11):** the wander output pushes towards each creature's own wander angle (`Creatures::wander`), which drifts **by distance travelled, not by time**: each tick it takes a normal step of sqrt(2 × stride / `WANDER_PATH`) radians, where stride is how far the body is about to go (`WANDER_PATH` 1.0 world unit, in `sim.cpp`). So a wanderer's path stays fairly straight for about a world unit whatever its speed. A newborn's angle comes from its id, so births draw nothing from the `Rng`.
  - **History of the "spinning in circles" (2026-11-11):** first the angle was a fresh random one every tick (jitter on the spot). Then it drifted by a fixed 0.5 rad every tick (`WANDER_TURN`), which looked fine for fast bodies but not for real movers: nearly all of them creep at 0.01 to 0.03 a tick, so they turned ~23° a tick while their loops were a tenth of a cell across, and the sprite rotated in place. Measured with a probe over 300 ticks: mean turn 23° a tick, path straightness 0.19. With turning by distance: 5 to 7° a tick, straightness 0.72 to 0.78.
  - **The spin was holding the world together.** With movers able to wander straight, slow sun-fed drifters (plants with a tiny motor) roamed onto fresh, unshaded, unfouled ground and took over: seed 7 fell to 800 creatures, 600 of them movers, 7 species. Lowering `FULL_STRIDE` from 0.02 to 0.005 (a creeper now loses most of its sunlight) and `WANDER_PATH` from 3 to 1 brought the world back (numbers in 10.7). `test_wander_turns_by_distance` checks that a slow wanderer does not spin.
  - **Mutation:** nudge weights; add a link; lose a link; grow a neuron in the middle of a link (the old link stays); lose a neuron with its links. Rates are the `P_*_LINK` / `P_*_NEURON` constants in `mutation.cpp`.
  - **Cost:** upkeep per hidden neuron (`NEURON_COST` 0.012), per link (`LINK_COST` 0.002) and per unit of weight (`BRAIN_COST` 0.01), all in `phenotype.cpp`. A brain on a body that cannot move costs nothing and does nothing.
  - **Layout:** `compile_brain()` turns the genome into a fixed-size `BrainPlan`; `think()` runs it. Plans and hidden state live in their own arrays in `Creatures`, off the hot path; only movers read them.
- **Danger / kin sense (2026-11-11):** three brain outputs, felt only with a sensor. Each body within two cells pulls or pushes the creature by `OUT_KIN` if it is the same species, `OUT_BIGGER` × how much bigger it is (negative when smaller), and `OUT_MOVING` × how fast it moved last tick; nearer and heavier bodies count for more. Nothing says which bodies are enemies: the weights start as noise and evolve. It walks the spatial grid, so the cost is bounded by local crowding. **This is the one rule that reads species ids**, so `SPECIES_THRESHOLD` and the distance weights now touch behaviour.
- **Moving is paid per distance as well as per speed (2026-11-11):** `movement_cost` is mass × (`TRANSPORT_COST` 2.0 × distance + `MOVE_COST` 3.0 × distance²) a tick, plus climbing. Before, only the squared term existed, so covering a distance at speed v cost mass × 3 × v × distance: creeping was nearly free and the world's movers were slow drifting plants. Now creeping a cell costs about two-thirds as much as walking it.
- **Senses are cheap (2026-11-11):** sensor upkeep is 0.05 per unit of size (it was 0.3, the dearest part there was; seeing one cell cost about 40% of a LUCA's upkeep, and nearly every mover was blind).
- **Optional traits (`seed_throw`, `bite`, `digestion`)** stay at exactly 0 until a rarer mutation switches them on (`P_NEW_TRAIT`), and are lost again below a sliver. A plain nudge bounced off zero and left every lineage paying upkeep for a little of each.
- **Sunbathing and moving conflict (2026-11-11):** a body's light demand is multiplied by `light_while_moving(moved)`: 1 if it sat still last tick, falling in proportion to 0.2 at a stride of 0.005 world units or more (`MOVING_SHADE` 0.8, `FULL_STRIDE`, in `energy.cpp`; 0.08 until wander began to drift, 0.02 until wander turned by distance, see 10.7). It applies before the canopy shares light out.
- **Dormancy (2026-11-11):** each creature has a `sleep` level, 0 awake to 1. With the `dormancy` gene it climbs by 1/20 a tick, up to the gene's value, whenever its cell is dark or colder than its comfort range, and drops by 1/90 a tick otherwise (quick to sleep, slow to wake). At sleep `s` a body pays (1 − 0.9 s) of its upkeep and thermal cost, catches (1 − s) of its light and pulls (1 − s) as hard on the living and the dead. Above 0.1 it cannot move or breed; at 0.5 or more it ages at an eighth of the usual pace. Its membrane is unchanged, so it can be eaten in its sleep. Rules are `next_sleep` and `cost_while_asleep` in `energy.cpp`; `dormancy` is an optional trait like `bite`.
- **Poison (2026-11-11):** a new part type. A body's `toxicity` is its total poison size, capped at 2. Whatever drains it loses `toxicity` × the energy drained, as heat (`out_poison` in `TickStats`), paid out of what it holds, so a poisonous meal can cost more than it gives (a meal returns 70%). Poison is light (0.1 mass per unit) but dear to keep (0.12 upkeep per unit), and unlike armour does not slow the bite. It works on living bodies only: a poisonous body's corpse is safe to eat. Nothing can sense it yet, so eaters cannot learn to avoid it; they are only selected against.
- **Gene transfer (2026-11-11):** each tick that a body is draining another, there is a `gene_transfer_chance` (0.0001, in `config.hpp`; 0 = off) that it takes up the blueprint of one of that body's parts, picked at random, as a half-size bud (`transfer_part` in `mutation.cpp`, drawn from the sim `Rng` in contact order). "Touch" here means the existing contact rule, so only bodies with a mouth take anything up. The blueprint goes into the taker's **genome only**: its own body and stats do not change, and its children are built from the new plan and pay for the part. So a living body's genome can now hold parts its body lacks (the inspector's parts list reads the genome). `TickStats` counts transfers, cross-species ones and ones that bring a kind of part the taker lacked; the chronicle logs the first cross-species one.
- **Waste (2026-11-11), the first piece of the chemistry layer:** `WASTE_SHARE` (20%) of every body's upkeep leaves it not as heat but as waste in its cell (`Soil::waste`), still holding that energy. Every body in the cell, its maker included, loses `WASTE_HARM` (0.0025) × waste × skin area a tick to it, as heat (`out_fouling`), asleep or not. Every 8 ticks each cell hands `WASTE_SPREAD` (20%) of its waste to its four neighbours and `WASTE_DECAY` (4%) of it breaks down to heat (`out_waste_decay`). A body with the `detox` gene (an optional trait like `bite`) takes up to 0.3 × detox a tick of its cell's waste, shared like corpses, and keeps its usual `efficiency` of it. Waste is energy only: the matter books do not change. "Stored" energy now includes waste. The census counts **cleaners** (over 60% of food from waste), the chronicle logs the first ten, and `fed_waste` is tracked per creature. Rules: `fouling_cost`, `waste_demand` (`energy.cpp`), `Simulation::spread_waste`. One look at the cell in phase 2 does all three things: take up, pay for fouling, excrete.
- **Wear:** upkeep is multiplied by `1 + wear × (age / AGE_SCALE)²` (`AGE_SCALE` 10000; `wear` 1 unless repaired). Nothing dies of old age as such; an old body costs more than it earns.
- **Repair and copying fidelity (2026-11-12):** the `repair` gene (an optional trait like `bite`) makes the wear clock run up to three times slower (`Phenotype::wear` = 1 / (1 + `REPAIR_GAIN` × repair)²) and multiplies upkeep by 1 + `REPAIR_SHARE` (0.15) × repair. The `mutability` gene scales every chance in `mutate()` (and in the brain's mutation) by 2^mutability; step sizes are unchanged. Copying better than the usual rates costs `FIDELITY_UPKEEP` (0.02) per halving; copying worse is free, and its only price is mutation load. Both are in the species distance (`W_MUTABILITY` 0.3 per doubling, `W_REPAIR` 1.5), the metrics (`mean_mutability`, `mean_repair`) and the creature snapshot.
- **Observation only:** `fed_light / fed_living / fed_dead` per creature and `Simulation::census()` count ways of life from real energy flows. Sim logic never reads them.
- **Species record book (2026-11-12, observation only):** each creature counts its kills and children (`Creatures::kills`, `children`). A kill goes to the body that drained the most from the victim in the tick it died (`top_taker_` in `sim.cpp`). At each death the registry keeps per-species tallies (born in all, died eaten or otherwise, kills, a kill web of killer and victim species) and records (eldest, most kills, most young, with the holder's id and tick). `take_habits()` runs with the census and stores each species' means (diet shares, movers, sighted, sleepers, armoured, poisonous, size, age, speed, light, temperature, height, where it lives and how spread out). Sim logic never reads any of it, and it draws nothing from the `Rng`: a 20,000-tick run is identical with and without it. Individuals get syllable names from their id (`creature_name`).
- **Species** (`core/species.*`, section 9.3 steps 1 and 2) are labels the game works out; only the kin sense reads them. Every creature carries a species id. A newborn keeps its parent's species unless its genome is further than `SPECIES_THRESHOLD` (1.0) from that species' founder; then it joins a living daughter species that is close enough, or founds a new one. The distance is Euclidean over weighted gene differences (a plain sum made every small drift a species: 164 at once instead of ~27). Shape genes are left out. No randomness is used.
- **Determinism:** same seed + build + machine → identical runs (verified). Cross-machine determinism is not a goal yet; creature files don't need it.

### 10.5 God powers (built 2026-11-11)

- **Four powers so far:** `terrain` and `heat` (soft round brushes), `sun` and `climate` (global). Script format is documented in `core/powers.hpp`.
- **World layers vs. derived fields:** powers edit layers (height, local heat, sun multiplier, climate offset); light and temperature are recomputed from them. `World::brush / add_sun / add_climate` are the only ways the world changes after generation.
- **Amounts are deltas,** so overlapping powers add up in any order. A ramp ends in the same world as the same power cast instantly (tested).
- **Ramps apply every 8 ticks,** not every tick.
- **Powers run at the start of a tick,** before any creature acts, and use no randomness. Same seed + same command log = identical world (tested by replaying `powers.txt`).
- **Chronicle detectors so far:** population milestones (1,000, 10,000, ...), a great dying (population halves from its peak), recovery (back to 90%), total extinction.
- **Felt now (2026-11-11):** `thermal_cost` and `movement_cost` are real, so terrain, heat and climate all reach life: cold or hot ground costs upkeep, and climbing costs energy.
- **Species in the chronicle:** a new species is logged when 10 are alive at once (*"A new species: Vuman branches from Luca. It is smaller."*), and its extinction if it ever got that far. Names are procedural and unique per id.
- **Chronicle detectors added 2026-11-11:** first movers, senses, armour, poison, hunters and scavengers (each needs 10 alive at once). A great dying is now judged on each day's peak population, because numbers breathe with day and night.

### 10.6 Viewer (built 2026-11-11, rebuilt as pixel art the same day)

Run with `./view.sh` (needs `thirdparty/godot-cpp`, which is git-ignored; the clone command is in the script). Only macOS is tested.

- **One class crosses the boundary:** `LucaWorld` (a `RefCounted`). Godot asks it for fields and for one MultiMesh buffer per frame; it tells it which power was cast.
- **Top-down, orthographic, flat.** The ground is one quad; `terrain.gdshader` draws tiles from the height, temperature, light and soil textures (nearest-filtered): height bands with ordered dither, three-step hill shading, dark cliff edges, snow where it is cold, darker rich soil, pale specks where corpses lie.
- **Creatures** are one MultiMesh of quads, one draw call; see 6.6. A sprite needs no rotation or scale, so the nine basis slots of each instance transform carry body data, and the shader reads them back from `MODEL_MATRIX`; the parts come from a float texture (`parts_tex`). Layout is documented in `godot/luca_world.h`.
- **Inspector** lists each part with its size, where it sits (front, left, back-right, ...) and its stalk, and says whether the body is round, oval, long or boxy.
- **Day and night** tint the whole picture. When days pass faster than about one a second, the tint holds a steady dusk instead of strobing.
- **Stats panel:** day and clock, population, the census (sun-fed, hunters, scavengers, cleaners, mixed diet, able to move), and the six most numerous species with their colours.
- **Colour by species** (checkbox or `C`, `--species` on the command line): skin is painted in the species colour instead of the pigment. The hue rides in the unused height slot of each instance's origin.
- **Overlays:** natural, temperature, light, nutrients, the dead, waste. In the natural view, heavily fouled ground turns murky olive.
- **Tree of life** (`T`, or the button; `viewer/tree_view.gd`; rebuilt 2026-11-12, the pixel tree with crowns was unreadable past a few dozen species): a scrollable cladogram over nearly the whole window. One row per species that ever had 10 alive; time runs left to right, a line runs from the tick a species branched off to now or to its extinction, and a stem drops from the parent's line. Children are listed newest first under their parent, so no stem crosses another line. A right-hand column gives each name and how many are alive (or when it died). Hover a row for its story in the footer; click to select it (its line of descent lights up, the rest dims); double-click or Enter to open its branch on its own, with breadcrumbs back; click the triangle to fold a clade; wheel, trackpad or drag to scroll; arrows move and fold; Backspace or right-click steps out; `E` hides dead ends; Esc closes. It opens on the followed creature's species. Data comes from `LucaWorld.get_species_tree()`. `--tree` or `--tree=Name` opens it at start.
- **Species card** (`viewer/species_card.gd`, 2026-11-12): beside the tree, it shows the selected species. At the top is a living portrait: a few real members drawn by the same blob shader in a small SubViewport over ground tinted by their habitat. Movers walk along their real heading. An extinct species is drawn from its founder's blueprint in stone (`stone` uniform in `blob.gdshader`). Below that:
  - a niche, a classification worked out from diet, parts and kill rate: Flytrap, Canopy frond, Roaming hunter, Ambush predator, Scavenger, Waste recycler, Mixotroph, Parasite and so on;
  - tags such as Armoured, Sleeps at night, Cold-hardy, Many small young and Cannibal, judged against where life actually lives;
  - a written natural history;
  - a diet bar;
  - the body of its eldest living member, part by part;
  - the record book (eldest ever, most kills, most young, with holders' names);
  - a food web of who it kills and who kills it (click a name to jump to that species);
  - its range.
  `P` or the inspector's "Its species" button opens the card on the followed creature's species, and species in the stats panel are clickable. Data comes from `LucaWorld.get_species_profile(id)` and `get_sprites(ids, species)`. The rules for niches and tags are in `_niche` and `_tags`; they are first guesses to tune.
- **Inspector** follows the picked creature by id as it moves, and shows its species, the species it branched from, and its diet as percentages of sun / the living / the dead.
- **Sandbox brushes:** raise/lower land, warm/cold spot, sun and climate buttons, an instant/gradual toggle. A stroke is many small instant commands; only the first is written to the chronicle (`quiet` flag).
- **Painting works while paused:** `Simulation::flush_powers()` applies due powers immediately, with the same result as waiting for the next tick.
- **Time:** pause, 1x, 4x, 16x, max. The sim gets a 10 ms budget per frame; past that the world runs slower (7.1).
- **Not done yet:** position interpolation between ticks, the food web view, sim on its own thread, a pixel font for the UI.
- **Checking the viewer:** `./view.sh --warmup=40000 --zoom=8 --at=100,24 --frames=40 --shot=/tmp/x.png` skips ahead 40,000 ticks, zooms in, saves a screenshot and quits. Keep `--frames` small: macOS throttles a window that is behind other windows to a few frames a second, so a long run looks like a hang.

### 10.7 Baseline numbers (default settings, 128×128, seed 42, measured 2026-11-11)

**After windfalls (2026-11-12, current; measured on a build that also held uncommitted work on mutability and repair genes, so compare the rows below with each other, not with older sections):**
- `--ticks 5000`: 787 creatures, audit error ≈ 1.5e-7.
- Six seeds (42 / 7 / 99 / 1 / 2 / 3), 100,000 ticks, mean of the last fifth, at `--windfall` 0 / 0.001 (default) / 0.003: population 4,000 / 3,900 / 4,460; established species 75 / 72 / 80; hunters 30 / 32 / 22; scavengers 7 / 8 / 10; mixed feeders 105 / 107 / 115; movers 99 / 44 / 52 (the 99 is mostly one seed with 320); corpse energy eaten 49 / 52 / 57 a tick. Audit error under 2e-7 throughout.
- At the default, windfalls are 16 energy a tick against about 2,070 of light (0.8%), and only about a fifth of it is eaten: corpse weathering rose by 13 a tick. Every difference at 0.001 is within the spread between seeds. At 0.003 (49 a tick, 2%) the world is a little fuller and has a few more scavengers. Whether movers actually seek windfalls out has not been measured.

**After evolvable mutation rate and repair (2026-11-12; measured without windfalls, against the front-mouth numbers below):**
- `--ticks 5000`: 730 creatures, 0.031 ms/tick, audit error ≈ 1e-7.
- Six seeds (42 / 7 / 99 / 1 / 2 / 3), 100,000 ticks, mean of the last fifth: 4,000 creatures (4,240 before), 75 established species (63; part of the rise is the two genes joining the species distance), 30 hunters (29), 105 mixed feeders, 99 movers (155; one seed holds 320 of its own, the rest 16 to 104). Audit error under 2e-7.
- At 200,000 ticks (same seeds): 5,760 creatures, 180 species, 70 hunters (more than the 56 under round mouths at 100,000), 169 movers. Seed 3 reached 9,220 creatures.
- **Repair spreads slowly:** about half of all bodies carry some by tick 100,000; the mean gene rises steadily (about 0.01 at tick 20,000, 0.06 to 0.13 at 100,000, 0.12 to 0.18 at 200,000). Bodies live a little longer: median age about 3,500 against 3,200, the oldest tenth past 9,700 against 8,700.
- **Mutability varies between lineages:** at tick 100,000 a third or more of bodies in every seed copy more faithfully than usual (and pay for it), while 4 to 28% copy at more than 1.4× the usual rates, some at the ×4 ceiling. The mean drifts upward slowly (+0.06 to +0.23 at 100,000, up to +0.46 at 200,000, rates about 1.4× the usual in the sloppiest world), except in seed 99, where it fell to −0.10.

**After new mouths bud at the front (`P_MOUTH_FRONT` 0.3, 2026-11-12):**
- `--ticks 5000`: 701 creatures, 0.032 ms/tick, audit error ≈ 2e-7.
- Six seeds (42 / 7 / 99 / 1 / 2 / 3), 100,000 ticks, mean of the last fifth: 4,240 creatures, 63 established species, hunters 17 / 20 / 24 / 39 / 31 / 45 (mean 29, still rising at the end), 122 mixed feeders, 155 movers; 96 moving hunters at tick 100,000 across the six (61 without it, 153 under the old round mouths).
- **Why hunters fell, and the fixes tried** (same six seeds, means): a small new mouth covers about a fifth of the circle, so the first step towards hunting pays far less than when every mouth touched every neighbour, and hunting starts later (about 5 hunters at ticks 20,000–40,000, against 32 under round mouths). Old rules: 56 hunters, 32 species. Arcs with random new mouths: 24 hunters, 56 species. Half-circle mouths (`MOUTH_ARC_BASE` π/2): 24 hunters, 45 species, hunting starts sooner but levels off. Front mouths plus half-circle: 34 hunters, 48 species. Random new mouths run to 200,000 ticks: 39 hunters, 136 moving hunters, 141 species, so most of the drop is a delay.

**After body plans: placement, shape and stalks (2026-11-12):**
- `--ticks 5000`: 701 creatures, 0.033 ms/tick, audit error ≈ 2e-7.
- `--light uniform --seed 7 --ticks 60000`: 6,209 creatures (3,832 before), 0.61 ms/tick.
- Six seeds (42 / 7 / 99 / 1 / 2 / 3), 100,000 ticks, mean of the last fifth: 4,980 / 3,670 / 4,190 / 3,530 / 2,030 / 4,240 creatures (mean 3,770); 72 / 58 / 64 / 56 / 25 / 60 established species (mean 56, about twice before); hunters 24 / 39 / 28 / 11 / 16 / 24 (mean 24, against 50 to 75 before); mixed feeders mean 91; movers 3 / 55 / 519 / 3 / 28 / 44. Audit error under 2e-7 throughout.
- **Fronds everywhere:** by tick 100,000, 85 to 98% of bodies hold their sun patches up on stalks (mean lift 0.32 to 0.47). The canopy gives a cell's light to the tallest first, so any lift wins the cell; it is an arms race, as the canopy rule predicted ("expect tall things"), paid for at about 8% of a LUCA's upkeep.
- **Tentacles evolve:** 4 to 31% of bodies with a mouth carry it on a stalk (17 to 31% in five seeds of six).
- **Tails move to the back:** a new tail lands at a random angle (mean thrust 0.625 of full). In the seeds with most tailed movers they sit at 0.94 (90 movers, seed 3), 0.70 (21, seed 99) and 0.80 (3, seed 7).
- Movers are a little longer than the rest in two seeds (elongation 0.21 and 0.30 against a world mean of 0.09 and 0.10); everywhere else shape stays near round (mean elongation and squareness about 0.1).
- **Wider mouths tried** (`MOUTH_ARC_BASE` 1.0 against 0.5, same six seeds): 3,370 creatures, 52 species, 32 hunters, 130 mixed feeders. Hunters recover only part of the way. Kept at 0.5; Uday's call.
- Stage 2 alone (arcs and placement, no shape or stalk physics; seeds 42 / 7 / 99): 4,270 / 4,570 / 3,930 creatures, 33 / 33 / 51 species, hunters 13 / 18 / 20.
- **Cost per creature grows with crowding, in the old build too.** The previous commit's build at `--light uniform --seed 7` holds 3,840 creatures at 60 ns each a tick, but with `--sun 1.6` 4,940 at 165 ns; the new build holds 6,900 at 160 ns. So the body plans are not slower, but the world is denser and something in the tick scales worse than linearly with density (7.1). Not yet profiled.

**After moving costs per distance (`TRANSPORT_COST` 2) and cheap senses (sensor upkeep 0.05):**
- `--ticks 5000`: 750 creatures, 0.031 ms/tick, audit error ≈ 1e-7.
- `--light uniform --seed 7 --ticks 60000`: 3,832 creatures, 0.19 ms/tick.
- Seeds 42 / 7 / 99, 100,000 ticks, last fifth: 2,950 / 3,380 / 3,000 creatures; 29 / 35 / 30 established species; movers 67 / 38 / 8; hunters 75 / 71 / 50; bodies with a sensor 168 / 294 / 316. Seeds 1 to 4: 3,700 / 3,150 / 2,300 / 2,090 creatures, 46 / 26 / 25 / 31 species.
- Movers at tick 100,000 over all seven seeds (467): a third of their food from the living (against about a fifth to a third with no distance cost), half from the sun; a third carry a sensor (none to 9% at sensor upkeep 0.3). Top speed 0.03 to 0.06. Probe of movers over 300 ticks: seed 7, 62% sighted, turning 8° a tick; seed 42, blind, mostly 8–9° a tick, with a few whose evolved wiring flips their direction (one reverses 73% of ticks over corpses: its "corpse here" input switches its wander urge off).
- **Grid tried** (seeds 42 / 7 / 99; distance cost 0 / 1 / 2 against sensor upkeep 0.3 / 0.1 / 0.05): every cell held 2,500 to 3,500 creatures except a crash of seed 99 at 0 / 0.1 (1,230) and seed 1 at 1 / 0.1 (885). Movers' share of food from the living rises with distance cost; cheaper senses mostly spread sensors onto plants (up to 430 bodies, where they do nothing) and onto movers only at 0.05. Mover diet varies more between seeds than between settings: at 2 / 0.3 it was 63% from the living in three seeds and 14% in four others, because one mover lineage per world sets the number. Judge any mover tuning on at least six seeds.

**After wander turns by distance, `FULL_STRIDE` 0.005, `WANDER_PATH` 1:**
- `--ticks 5000`: 682 creatures, 0.030 ms/tick, audit error ≈ 1e-7.
- `--light uniform --seed 7 --ticks 60000`: 2,933 creatures (4,161 before), 0.20 ms/tick.
- Sweep, seeds 42 / 7 / 99, 100,000 ticks, mean of the last fifth: 3,020 / 2,500 / 2,560 creatures (3,200 / 3,910 / 2,910 before); 33 / 39 / 25 established species (28 / 38 / 36); movers 19 / 57 / 11; hunters 49 / 64 / 35; cleaners 7 / 5 / 8.
- Other settings tried (same sweep, all with turning by distance): `WANDER_PATH` 3 and stride 0.02: 2,510 / 800 / 1,650 creatures, 22 / 7 / 20 species, movers 371 / 608 / 15. Stride 0.01: 2,380 / 1,960 / 1,490. Stride 0.005 with path 3: 2,760 / 3,010 / 2,740, species 32 / 33 / 19. Path 0.5 with stride 0.02: 1,680 / 2,210 / 1,870.
- Movers are still slow creepers (top speed 0.02 in the probe) with no senses and no inertia: a fixed wander urge and nothing else. Real chasing or fleeing has not evolved.

**After waste:**
- `--ticks 5000`: 742 to 752 creatures, about 0.03 ms/tick, audit error ≈ 1e-7.
- `--light uniform --seed 7 --ticks 60000`: 4,161 creatures, 0.24 to 0.26 ms/tick: 57 ns per creature per tick against 45 before. Switching fouling off did not change it; that world evolved more mouths and senses. In the three-seed sweep below the cost per creature did not rise (64 / 62 / 66 ns against 85 / 68 / 78).
- Sweep, seeds 42 / 7 / 99, 100,000 ticks, mean of the last fifth: 3,200 / 3,910 / 2,910 creatures (2,890 / 3,370 / 3,150 before waste); 28 / 38 / 36 established species (25 / 30 / 19 before); cleaners 8 / 15 / 5, first logged at ticks 55,800 / 25,800 / 25,250; mixed feeders 133 / 240 / 202 (99 / 149 / 138 before). Waste eaten is a quarter to a third of waste given off; fouling burns about 5% as much as upkeep. Audit error under 1e-7.
- **Detox is widespread, not a specialist's trait:** 49 to 63% of bodies carry some, and they get about 10% of their food from waste; they are plants that also recycle. (Measured before fouling moved into phase 2, at the same settings: at `DETOX_UPKEEP` 0.16, 41–57% carry it and there are 7–9 cleaners; at 0.24, 39–49% still carry it, cleaners fall to 1–5 and one seed dropped to 1,670 creatures.) Kept at 0.08.

**After cold nights, `temp_day_swing` 10 (before waste):**
- `--ticks 5000`: 745 creatures, 0.024 to 0.038 ms/tick (noisy), audit error ≈ 1e-7.
- `--light uniform --seed 7 --ticks 60000`: 4,998 creatures, 0.165 ms/tick.
- Sweep, seeds 42 / 7 / 99, 100,000 ticks, mean of the last fifth, at swing 0 / 10 / 20 °C: population 3,300 / 3,140 / 2,980 (mean of seeds); established species 28.5 / 24.5 / 22.6; mean radius 0.895 / 0.907 / 0.920; heat burned (`out_thermal`) 490 / 1,130 / 3,240 a row. Audit error under 1e-7 throughout. Bigger swings mean bigger bodies, a little, and fewer species.
- Seed 42 at tick 100,000, mean radius by band of latitude from the warm edge: no swing 0.82 / 0.90 / 0.91; swing 10 0.85 / 0.93 / 0.94; swing 20 0.89 / 0.92 / 0.94. Bodies were already bigger away from the warm, bright edge (light falls that way too, so the two are mixed up); the swing makes every band bigger. The coldest quarter of the map holds no life in any of them. One seed only.

**After wander drifts, with `FULL_STRIDE` 0.02 and `WANDER_TURN` 0.5 (before cold nights):**
- `--ticks 5000`: 796 creatures, 0.031 ms/tick, audit error ≈ 2e-7.
- `--light uniform --seed 7 --ticks 60000`: 4,247 creatures, 0.178 ms/tick.
- Sweep, seeds 42 / 7 / 99, 100,000 ticks, mean of the last fifth: 3,669 / 3,746 / 3,181 creatures; 26 / 21 / 27 established species; movers 59 / 27 / 33; hunters 48 / 42 / 61; mixed feeders 179 / 126 / 213. The old per-tick random wander gave 4,228 / 3,446 / 2,387, 26 / 28 / 29 species, 51 / 58 / 160 movers, about 40 hunters and 130 mixed.
- Movers now turn about 23° a step (90° before) and get somewhere.
- Other settings tried (same sweep): drifting wander at turn 0.15 with stride 0.08 overgrazed the world to 680–1,240 creatures with movers the majority; `MOVE_COST` ×10 or ×100 did not help (moving is so slow that cost ∝ speed² is near zero); stride 0.01 nearly wiped out movers; turn 1.0 restored numbers but brought most of the jitter back.

**After gene transfer, before wander drifted:**
- `--ticks 5000`: 749 creatures, 0.021 ms/tick, audit error ≈ 2e-7.
- `--ticks 200000`, seeds 42 / 7 / 99: 6,666 / 1,311 / 3,926 creatures; 123 / 24 / 66 established species; movers 60 / 464 / 129; 0.11 to 0.22 ms/tick; audit error ≈ 3e-7. The spread between seeds is as wide as ever.
- First borrowed blueprint in the chronicle between ticks 4,500 and 15,600.
- `--light uniform --seed 7 --ticks 60000`: 4,585 creatures, 0.187 ms/tick.

**After poison, before gene transfer:**
- `--ticks 5000`: 807 creatures, 0.022 ms/tick, audit error ≈ 2e-7.
- `--ticks 200000`, seeds 42 / 7 / 99: 3,880 / 3,664 / 2,287 creatures; 83 / 88 / 57 established species; 289 / 157 / 74 poisonous; 62 to 92 hunters and 280 to 410 mixed feeders; movers 121 / 150 / 262; mean dormancy 0.35 to 0.41; about 0.13 ms/tick; audit error ≈ 3e-7.
- First poison in the chronicle at about tick 7,450 (seed 42).
- `--light uniform --seed 7 --ticks 60000`: 3,826 creatures, 0.186 ms/tick.

**After dormancy, before poison:**
- `--ticks 5000`: 735 creatures, 0.023 ms/tick, audit error ≈ 2e-7.
- `--ticks 200000`, seeds 42 / 7 / 99: 1,962 / 2,784 / 3,201 creatures, 14 / 67 / 76 established species, mean dormancy 0.82 / 0.26 / 0.37; about 0.13 ms/tick; audit error ≈ 3e-7.
- Seed bank check (seed 7, sun −0.35 and climate −12 °C from tick 150,000 to 160,000): 3,080 creatures fell to 43 and 10 species came through; mean dormancy among the living rose from 0.17 before the winter to 0.38 at its end and 0.51 as they recovered.
- `--light uniform --seed 7 --ticks 60000`: 4,513 creatures, 0.168 ms/tick.

**After the sunbathing-versus-moving rule, before dormancy:**
- `--ticks 5000`: 876 creatures, 0.020 ms/tick, audit error ≈ 2e-7.
- `--ticks 200000` (seed 42): holds near 3,000 from tick 40,000 on and ends at 3,191 with 47 established species, about 200 mixed feeders, 20 to 90 hunters, 30 to 70 movers; 0.10 ms/tick; audit error ≈ 3e-7. Mean radius 0.77, mean bite 0.12.
- Three seeds, last fifth of 200,000 ticks: 3,077 / 2,553 / 1,795 creatures, 45 / 37 / 31 species, 12% of cells occupied. With the rule switched off: 405 / 480 / 1,560 and 9 / 6 / 24. At `MOVING_SHADE` 0.5: 698 / 2,456 / 574. At 1.0: about the same as 0.8, with fewer movers (14 to 20).
- `--light uniform --seed 7 --ticks 60000`: 4,713 creatures, 0.135 ms/tick.

**After retuning (`METABOLIC_RATE` 0.75, `AGE_SCALE` 10000) and the evolvable brain, before that rule:**
- `--ticks 5000`: 846 creatures, 0.022 ms/tick, audit error ≈ 2e-7.
- `--ticks 200000`, seeds 42 / 7 / 99: peaks of 3,000 to 3,500 near tick 40,000; ends at about 400 / 400 / 2,000 creatures with 8 / 4 / 29 established species; 370 to 410 running brains in each; 0.04 to 0.15 ms/tick; audit error ≈ 4e-7.
- `--light uniform --seed 7 --ticks 60000`: 3,853 creatures, 0.157 ms/tick.
- Firsts (seed 42, before the brain change): movement ~6,300, 1,000 strong ~6,900, senses ~9,150, hunters ~9,550, armour ~13,000.
- The tuning sweep script used for this is not in the repo; the method was three seeds per setting, 150,000 ticks, averaging the last fifth. Single levers tried: washing off or slower, more soil matter, a brighter dim edge, a cheaper body, shade off, longer lives, cheaper metabolism. Only the last two moved the world clearly.

**After bite, digestion and the kin sense, before the retune:**
- `--ticks 5000`: 361 creatures; `--ticks 60000`: 304; audit error ≈ 3e-7; about 0.015 ms/tick.
- `--light uniform --seed 7 --ticks 60000`: 1,684 creatures, 0.042 ms/tick.
- `--ticks 300000`, seeds 42 / 7 / 99: 310 / 451 / 527 creatures, 7 / 13 / 12 established species.

**After slope shade and matter washing downhill, before those:**
- `--ticks 5000`: 389 creatures, 0.017 ms/tick, audit error ≈ 2e-7.
- `--ticks 60000`: 641 creatures, 0.018 ms/tick; 4 established species.
- `--ticks 300000`: 658 creatures, 0.021 ms/tick; 15 established species; mean radius 0.73.
- `--light uniform --seed 7 --ticks 60000`: 2,255 creatures, 0.045 ms/tick.

**After corpses worth 244 and the size rule at the mouth, before the terrain rules:**
- `--ticks 5000`: 449 creatures, 0.016 ms/tick, audit error ≈ 2e-7.
- `--ticks 60000`: 842 creatures, 0.022 ms/tick; 1 established species; hunters ~43,600, no scavengers yet.
- `--ticks 300000`: 2,315 creatures, 0.042 ms/tick; 49 established species alive; first scavengers at tick 165,500, then a steady 10 or so; about 120 mixed feeders; mean radius 0.71; mean `seed_throw` 0.63.
- `--light uniform --seed 7 --ticks 60000`: 2,523 creatures, 0.083 ms/tick.

**After canopy and paid dispersal, before the two rules above:**
- `--ticks 5000`: 976 creatures, 0.026 ms/tick, audit error ≈ 2e-7.
- `--ticks 60000`: 1,685 creatures, 0.048 ms/tick; 19 established species alive; mean `seed_throw` has risen from 0 to about 0.5 cells and is still rising; mean radius 0.89 at tick 55,000.
- `--light uniform --seed 7 --ticks 60000`: 3,304 creatures, 0.12 ms/tick.
- Firsts: senses ~2,600, 1,000 strong ~4,500, armour ~5,000, movement ~5,050, hunters ~7,000.

**Before those two rules (kept for comparison; the numbers below no longer reproduce):**

- `--ticks 5000`: about 800 creatures, 0.02 ms/tick, audit error ≈ 2e-7. Life starts slowly; 200 LUCAs take ~9,000 ticks to pass 1,000.
- `--ticks 60000`: population breathes between roughly 2,500 and 4,500 with day and night; about 0.08 ms/tick averaged over the run.
- Species: about 550 founded in 60,000 ticks, 36 of them ever reaching 10 alive, 27 such alive at the end; the original Luca still holds 1,842 of 4,159 creatures. Species detection costs nothing measurable.
- Typical firsts in the chronicle: movement ~3,000 ticks, armour and senses ~6,000–8,000, hunters ~12,000, scavengers later and not in every run.
- Light gradient check (emergence test 2): by tick 53,000 mean photo-patch size is 0.65 in the sunny half and 1.02 in the dim half; bodies are smaller in the sun (0.67 vs 0.78 radius).
- **Cost per creature is about 120 ns a tick** at 8,000 creatures (1.0 ms/tick, `--light uniform --seed 7`, tick 60,000). It was about 25 ns before contact, soil and movement existed. That is inside the 150 ns single-core budget of 7.2 but only just; the time goes on scattered memory access in phases 1c and 2b of `Simulation::step()`, not on contact.
- The old numbers (plateau of 8,700, 0.2 ms/tick) were for a world with no night, no matter and nothing that could eat anything; they no longer apply.

---

## 11. Work split and next steps

**Split, as changed on 2026-11-11:** Uday asked Claude to finish the energy rules and mutation and to extend the phenotype compiler so that creatures could move, eat and evolve. Those files are no longer off limits. They are first guesses that give a living world; **Uday owns the tuning** and may rewrite any of them.

### Good places for Uday to tune (all are single constants)

- `energy.cpp`: `METABOLIC_RATE` (0.75 since the retune), `AGE_SCALE` (10000), `MOVING_SHADE`, `FULL_STRIDE` (0.005), `TRANSPORT_COST` (2.0), `MOVE_COST`, `CLIMB_COST`, `CONTACT_RATE`, `CORPSE_RATE`, `THERMAL_SCALE`; `energy.hpp`: `TRANSFER_EFFICIENCY`, `CORPSE_WEATHERING`.
- `phenotype.cpp`: `COST_PER_SIZE` per part (sensor 0.05), thrust, membrane and armour numbers, capacity, `BODY_ENERGY_PER_MASS`, `BRAIN_COST`, `TOLERANCE_COST`.
- `energy.cpp`: `SIZE_POWER`, `SIZE_EDGE_MAX` (the size rule at the mouth).
- `mutation.cpp`: the `P_*` rates, including `P_NEW_TRAIT`, `P_MOUTH_FRONT` and the brain's `P_ADD_LINK`, `P_DEL_LINK`, `P_ADD_NEURON`, `P_DEL_NEURON`.
- Brain: `NEURON_COST`, `LINK_COST`, `BRAIN_COST` (`phenotype.cpp`); `MAX_HIDDEN`, `MAX_LINKS` (`genome.hpp`).
- `phenotype.cpp`: `BITE_UPKEEP`, `DIGEST_GAIN`, `DIGEST_UPKEEP`; `sim.cpp`: `SOCIAL_CELLS`, `SOCIAL_FAST` (the kin sense), `WANDER_PATH` (how far a wanderer goes before its direction is lost).
- Cold nights: `temp_day_swing` (`config.hpp`), `HEAT_LAG` (`phenotype.cpp`).
- Waste: `WASTE_SHARE`, `WASTE_SPREAD`, `WASTE_DECAY` (`energy.hpp`); `WASTE_HARM`, `WASTE_EAT_RATE` (`energy.cpp`); `DETOX_UPKEEP` (`phenotype.cpp`).
- Windfalls: `windfall_rate`, `windfall_size` (`config.hpp`). Bigger, rarer lumps make finding one matter more; a higher rate feeds the world more.
- `config.hpp`: `day_length`, `nutrient_mean`, `nutrient_patchiness`, `gradient_min`, `birth_max_step`, `slope_shade`, `wash_rate`, `gene_transfer_chance`.
- `world.cpp`: `FACING_LOSS`, `VALLEY_LOSS`, `HORIZON`, `STEEP_DROP` (shade and drainage).
- Dispersal: `THROW_COST` (`energy.cpp`), `THROW_UPKEEP` (`phenotype.cpp`), `limits::MAX_THROW`.
- Body plans (`body.hpp`): `MOUTH_ARC_BASE`, `MOUTH_ARC_PER_SIZE`, `COVER_ARC`, `MEMBRANE_BASE`, `ARMOUR_RESIST`, `ARMOUR_CLOGS` (moved here from `phenotype.cpp`), `STALK_CLEARS`, `BOX_SKIN`, `BOX_DRAG`; `thrust_facing` in `body.cpp`. Keep the arc constants in step with `viewer/blob.gdshader`.
- Shape and stalks (`phenotype.cpp`): `SKIN_UPKEEP`, `STALK_MASS`, `STALK_UPKEEP`; `limits::MAX_STALK`.
- Copying and repair (`phenotype.cpp`): `FIDELITY_UPKEEP`, `REPAIR_GAIN`, `REPAIR_SHARE`; `limits::MIN_MUTABILITY`, `MAX_MUTABILITY`; `AGE_SCALE` (`energy.cpp`).

### Known weak spots

- Scavengers stay rare and movers are a small minority; hunters are mostly sessile mixed feeders. The decomposer and chase/flee niches exist but are thin.
- Bodies still shrink, but far more slowly since the canopy rule (mean radius 0.89 at tick 55,000, against 0.71 before). Most cells hold one body, so there is rarely anyone to overtop.
- The world is much emptier since dispersal has to be paid for: about 1,700 creatures at tick 60,000 where there were 4,100, growing in clumps around each founder. A sessile parent also drains its own cell of matter after one child (the body is built from the parent's cell even when the child is flung).
- **Everything is several times slower since bodies cost 244.** A child costs its parent about twice the sunlight it used to, so generations are slower, the map fills more slowly and evolution takes longer: one established species at tick 60,000, 49 by tick 300,000. This makes "the first ten minutes are slow" worse. Levers, all Uday's: `BODY_ENERGY_PER_MASS`, `METABOLIC_RATE`, `initial_population`.
- **Washing makes the world poorer, not just patchier.** A parent needs a whole body's worth of free matter in its own cell, so soil that drains off a slope leaves it barren while the hollows hold more than one body per cell can use. At `wash_rate` 0.005 life fell to a quarter; at the current 0.001 a 300,000-tick run holds about 650 creatures and 15 species where it held 2,300 and 49 without the terrain rules. Nothing yet carries matter back uphill (a volcano power, or creatures that carry, would).
- **Gene transfer happens but its effect is not shown.** A 200,000-tick run sees 6,000 to 12,000 transfers against 84,000 to 211,000 births; about 30% cross species and 130 to 200 bring a kind of part the taker lacked. Nearly all are a hunter taking up a sun patch from the plant it eats. Whether any borrowed part went on to spread has not been traced.
- **Poison holds as a minority defence**: 74 to 289 poisonous bodies (3 to 8% of the world) at tick 200,000 across three seeds, more than armour in two of them. About 1 to 2% of what hunters drain is burned by it.
- A shape gene's slot in the sprite data now carries poison, so the five-lobed lump from `shape[2]` is no longer drawn.
- **Dormancy may be too good.** It is selected in every run: by tick 200,000 the mean gene is 0.26 to 0.82 across three seeds, and in one of them nearly every creature sleeps deeply. Little hunts at night yet, so being helpless costs almost nothing. Levers: `DORMANT_SAVING`, `WAKE_UP`, `FALL_ASLEEP` (`energy.cpp`), `DORMANCY_UPKEEP` (`phenotype.cpp`).
- **Movers took over again in one seed of three** once they could sleep (1,915 of 1,962 creatures at tick 200,000, species down from 26 to 14); the other two seeds reached 67 and 76 species, the most so far.
- Sleeping bodies look the same as waking ones in the viewer; only the inspector says.
- **Brains stay small.** Across three 200,000-tick runs hundreds of creatures carry a brain, with about 5 links each, but hidden neurons are rare: usually under 1 brain in 10 has one, the most seen in any brain is 2, and they come and go (one run reached about half of brains with a neuron, then lost them). Thinking costs upkeep and so far a few reflexes are enough. The costs and the structural mutation rates are the levers; so is giving brains more to think about (prey that flees, a day worth planning around).
- **Movers no longer take over** (they did before moving cost sunlight: two of three seeds ended near 400 creatures, almost all movers). With the rule, three 200,000-tick runs end at 1,800 to 3,100 creatures and 31 to 45 established species, with movers a steady minority of 30 to 80. At the end of one run the 27 movers carried far more bite (0.42 against a world mean of 0.12) and digestion (0.18 against 0.04) than the rest; a small sample, but the first sign those genes are used. Movers still carry large sun patches, so the plant/animal split is not clean yet.
- **Bite, digestion and the kin sense have not visibly paid off yet.** (Measured before the retune and the new brain; not re-measured.) After 300,000 ticks mean bite is under 0.1 and digestion under 0.07, hunters number 0 to 5 and movers 0 to 2, so there is almost nobody to use them. Whether anything actually flees or herds has not been measured.
- **Runs differ a lot by seed**: 310, 451 and 527 creatures at tick 300,000 for seeds 42, 7 and 99. Judge a tuning change on several seeds, not one.
- Most corpse energy still weathers away uneaten (about 70% in the last 50,000 ticks of a 300,000-tick run), because eaters must be in the same cell and few bodies move.
- The `seed_throw` gene has no drawing in `blob.gdshader` (it is a gene, not a part); the inspector shows it.
- Kin are eaten like anyone else; there is no self-recognition.
- **Movers are drifting plants, not animals.** A probe of every mover over 300 ticks (seeds 7, 42, 99) finds top speeds of 0.01 to 0.03 a tick, almost none with a sensor, and brains that are a single fixed wander urge. Moving pays only as slow drift to new ground. Cost per distance and cheap senses (built since) moved this a little: movers now get about a third of their food from the living, a third carry a sensor, and they are still slow (0.03 to 0.06 a tick). Still open: prey worth chasing, and anything that makes speed itself pay.
- **Waste can't be sensed.** Movers have no brain input for it, so nothing can learn to leave a fouled cell; they are only selected against it. Detox is mostly carried by plants as a side income (see 10.7) rather than making a separate cleaner niche: 5 to 15 true cleaners at a time.
- **Hunters are thinner since mouths have arcs (2026-11-12):** a sessile mouth now reaches only the neighbours in front of it, so hunters fell from 50 to 75 to about 24 across six seeds; with new mouths budding at the front, 29 and rising (10.7). Levers: `MOUTH_ARC_BASE`, `MOUTH_ARC_PER_SIZE` (`body.hpp`, and the shader); evolution's own answers are more mouths, bigger ones, and tentacles.
- **Every plant is a frond:** stalked sun patches win the canopy so reliably that 85 to 98% of bodies grow them. Levers: `STALK_UPKEEP`, `STALK_MASS`. A cell still goes to the single tallest body first; nothing yet makes being under the canopy livable (shade-tolerant photosynthesis would).
- **Mutation rates creep upward.** Copying worse than usual is free, so mutability random-walks between a floor that costs and a ceiling that doesn't; selection against mutation load holds it only loosely (mean +0.46, rates ×1.4, in one seed by tick 200,000). If it keeps climbing, a small upkeep for sloppiness or a lower `MAX_MUTABILITY` are the levers. Not run past 200,000 ticks.
- Shape barely evolves (mean elongation and squareness about 0.1). Extra skin only pays where sun patches outgrow the skin, and streamlining only for the few fast movers.
- The creature snapshot (`creatures.csv`) has five new columns: elongation, squareness, mouth_stalk, canopy_lift, tail_facing.
- `tools/plot.py` gained two panels (ways of life, energy flows) that have not been run: matplotlib isn't installed on this machine.

### What the game is lacking (assessed 2026-11-11, in order of importance)

The headline: **you can't see evolution happening.** The sim produces real variety, but the game gives no way to notice it.

1. **Species: mostly built 2026-11-11.** Genome distance, species ids at birth, names, chronicle entries, a species list, a colour-by-species view and the tree of life now exist. Still missing: split checks for a species forming two clusters (9.3 step 3), and following one species (favourites, 4.8).
2. **The ecology is one niche deep.** Almost everything is a sessile plant; "hunters" are plants with a mouth.
   - Nothing rewards being big. Bodies shrink steadily. The design says size should make a body harder to eat and better at holding heat; neither rule exists.
   - Moving rarely pays. Light is everywhere, prey can't flee, and children already land up to 2.5 cells away for free.
   - The dead are barely worth eating. A corpse holds ~44 energy against up to 400 in a living body, and weathers away by itself.
3. **The world is too uniform.** Light is a smooth gradient; there is no water, no shade, and no barrier that really isolates a population. A uniform world supports few species (2.2).
4. **The player has little to do.** Four powers and no goal. Nothing interesting can be caused on purpose yet (e.g. split a population with a mountain and watch two species form), and that needs point 1 to be visible.
5. **The first ten minutes are slow.** 600 LUCAs take thousands of ticks to fill the map; nothing visibly new appears before about tick 3,000.
6. **No save/load.** Creature files and the seed vault depend on it.

Smaller: there is one nutrient, not several.

### TODO: deeper ecology and a less uniform world (picked by Uday, 2026-11-11)

These answer points 2 and 3 above. None is built yet. Each is one physical rule with a cost; none tells a creature what to do. Where a line says "Claude's reading", that detail is an interpretation of Uday's pick, to confirm before building.

**Size**

- [x] **Canopy.** *(built 2026-11-11)* Light in a cell goes to the biggest body first; smaller ones get what is left, instead of an equal share. Size buys sunlight and costs upkeep and matter. Expect tall things and shade-tolerant things beneath them.
- [x] **Mouths only work on smaller bodies.** *(built 2026-11-11; scaled, not cut off, so a small thing can still sip from a big host)* Scale the pull in `contact_flow` by the size ratio of taker to giver, so being bigger than a neighbour is real protection.
- [x] **Cold nights.** *(built 2026-11-11; body temperature lags the air by mass / skin, so fat counts as well as size)* Temperature swings with day and night, and a big body feels a smoothed version of it (it holds heat). Big bodies should do better in cold places without being told to.

**Movement and hunting**

- [x] **Dispersal must be paid for.** *(built 2026-11-11, with a `seed_throw` gene as the costly seed-throwing trait)* Children land touching the parent, not up to 2.5 cells away for free. Reaching new ground then needs movement or a costly seed-throwing trait. Also stop children landing across a steep drop, so raised mountains really isolate populations.
- [x] **Faster predation as a specialisation.** *(built 2026-11-11 as the `bite` gene)* Not a global speed-up: an evolvable trait that lets a hunter drain faster, with extra energy cost, so fast killing is something a lineage commits to. (Claude's reading: a gene or part parameter that multiplies contact rate and adds upkeep.)
- [x] **A danger / kin sense.** *(built 2026-11-11 as three evolvable brain weights; kin = same species)* One extra brain input alongside "living mass nearby": a weighted sum over nearby bodies of how safe or unsafe they are to be near, built from whether they share the creature's lineage, their size relative to it, and how they move. Needs a sensor, like the other senses. (Open: "lineage" today is the founder id; real kinship wants the species IDs of section 9, so this fits best after or alongside species detection. The weights should be evolvable, not fixed, or we are scripting who counts as an enemy.)

**The dead**

- [x] **Corpses worth eating.** *(built 2026-11-11; capacity and the LUCA's dividing point went up by 200 too, or a division left neither body enough to last a night)* A LUCA-sized corpse should hold about 244 energy (today ~44), and that energy decays over time. Raise the body build cost to match, so the audit still balances.
- [x] **Nutritious meals as a specialisation.** *(built 2026-11-11 as the `digestion` gene; applies to living prey and corpses)* An evolvable trait that gets more out of what is eaten, with extra energy cost. (Claude's reading: a digestion gene on the eater that raises transfer efficiency above the base 70% and adds upkeep; confirm whether it should apply to corpses only or to living prey too.)

**Terrain**

- [x] **Slope shade.** *(built 2026-11-11)* Ground facing away from the sun and valley floors get less noon light. A static field, recomputed only when terrain changes, so the terrain brush changes where plants can live.
- [x] **Matter washes downhill.** *(built 2026-11-11)* Every few ticks a little soil nutrient moves to the lowest neighbouring cell. Ridges become bright but poor, valleys rich but dim. Must keep `total_matter()` constant.

**More rules (second batch, picked by Uday 2026-11-11)**

- [x] **Sunbathing and moving conflict.** *(built 2026-11-11)* Light capture drops while a body is moving (section 2.2 always said this; it was never built). Pushes lineages to commit to staying put or to moving: the plant/animal split.
- [x] **Dormancy.** *(built 2026-11-11)* A trait that drops upkeep to near zero when it is dark or cold, at the cost of being helpless and slow to wake. A second answer to night and winter besides storing more, and a seed bank after a catastrophe.
- [x] **Poison.** *(built 2026-11-11 as a body part)* A part that makes a body costly to eat: whoever drains it loses energy. Costs upkeep to carry. A different defence from armour; opens the door to warning colours and bluffing once pigment can be sensed.
- [x] **Gene transfer on contact.** *(built 2026-11-11; through feeding contact, into the genome only)* When two bodies touch, a small chance that one part's blueprint is copied across. Reuses the contact rule; lets an invention spread between lineages. A stand-in until sexual reproduction exists. Must use the sim `Rng`.
- [x] **Waste.** *(built 2026-11-11; waste holds energy, not matter; eaten through a `detox` gene, not the mouth)* Living produces a waste chemical in the cell that slowly poisons whatever made it, unless something else absorbs it as food. A second cost of crowding and a niche for cleaners. The smallest useful piece of the chemistry layer (2.1).
- [ ] **Growing up.** Children are born small and grow by taking in matter, instead of appearing full-size. Young are vulnerable, and many-small versus few-large offspring becomes a real choice. The largest item: it touches the phenotype compiler and the matter books. Best judged once species are visible.
- [x] **Evolvable mutation rate and lifespan.** *(built 2026-11-12 as `mutability` and `repair`)* Two genes: how sloppy copying is, and how much energy goes into repair to slow wear. Each needs a cost (8.2).
- [ ] **Vents.** A second energy source that needs no sun, placed by a brush (already a player power in 4.1). The first place where life in the dark has an advantage.

Suggested order for this batch: the movement conflict and dormancy first (small, and aimed at "everything is a sessile plant with a mouth"), then poison, then the rest; growing up last.

**Later (deliberately not now):** water (sea level, flagella vs legs, light fading with depth) and seasons (the bright band drifting north and south over a long year).

Suggested order when building: canopy and dispersal first (biggest effect, smallest change), then corpse energy and the size rule for mouths, then the terrain pair, then the three that add genes or senses.

### Next scaffolding

- Speed: keep creatures roughly sorted by cell, then multithread phase 1 by world tiles
- World-cast events (4.7): storms, eruptions, drift, from the sim `Rng`
- Water as a world layer + water brush

- Species follow-ups: periodic split checks (9.3 step 3), pinning a species to follow it
- More than one nutrient; matter washing downhill
- Niche construction (2.6): shade from photo patches is the cheapest next step
- Viewer follow-ups: tree-of-life and food-web panels, interpolation between ticks

---

## 12. Open questions

- **Reproduction mode timing:** when to add sexual reproduction, and how mates find each other.
- **Death and decay:** how corpses hold matter and how fast they release it.
- **Tuning workflow:** headless parameter sweeps with logged metrics.
- **Save/load:** serialising full world state (follows from determinism).
- **Later:** camera and UX, water dynamics, the chemistry layer, influence formula details.

---

## 13. Working agreement for Claude Code in this repo

- **The creature rules (phenotype, energy, mutation) are open to edit** since 2026-11-11, when Uday asked for them to be completed. Say what you changed and why, keep constants named and in one place, and leave tuning decisions to him.
- **Keep the core free of Godot.** `core/` must compile with only the standard library.
- **Determinism:** use only the `Rng` passed in; never `std::rand`, `random_device`, time, or unordered-container iteration order in sim logic.
- **Never break the energy audit.** Any change that moves energy must be reflected in `TickStats`. Run a headless check after sim changes and confirm audit error stays ≈ 1e-6.
- **Data-oriented and linear:** no per-creature heap objects in the hot path; no O(N²) loops; neighbour queries go through the spatial grid.
- **No population caps,** ever. Limits come from ecology only.
- **Powers never touch creatures.** They go through `World`'s edit functions and the `PowerSystem` log, so every change to the world is replayable.
- **Every new gene needs a cost** in the phenotype compiler.
- **Before finishing a change:** build with no warnings, run `./build/luca_headless --ticks 5000`, and check population behaviour, audit error and ms/tick against the baseline in 10.7. Also run `./build/test_powers` and `./build/test_life`.
- **Matter is conserved too.** Anything that builds, kills or moves matter must keep `total_matter()` constant.
- **The look is pixel art.** No smooth gradients, no 3D meshes for creatures; new body parts get a pixel drawing in `blob.gdshader`.
