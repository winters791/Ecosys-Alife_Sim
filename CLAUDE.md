# Ecosystem God Game — Design & Project Reference

Owner: Uday · Last updated: 2026-11-11

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

> **Look changed again 2026-11-11 (Uday): soft 3D creatures.** The pixel-art look "isn't working"; the game is now a 3D world of squishy blobs seen from a tilted camera, close to the original plan in 6.2 and 6.4. Section 6.7 is how it works now; 6.6 (pixel bodies) is history.
>
> **Earlier the same day:** the game was **top-down pixel art, like WorldBox**, not 3D. Creatures are small pixel sprites and the ground is pixel tiles. The simulation is unchanged (still 2.5D: a heightmap, slopes, climates). Read "3D blob", "mesh" and "shader jiggle" below as history; 6.6 is how it works now.

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
- **What each part looks like:** poison = violet warts scattered over the skin; photo = green pixels over the skin; absorber = a dark red mouth on the leading edge (teeth when large); flagellum = a wiggling tail; legs = stubs that step; sensor = an eye (two when large); armour = grey plates round the rim; storage = a pale fat spot. Pigment is skin colour, fullness is body size, shape genes are lumps in the outline.
- **Zoom is in whole screen pixels per sprite pixel** where it can be, so the art stays crisp.

### 6.7 Soft 3D bodies (decided and built 2026-11-11; replaces 6.6)

- **Every piece of a creature is an instanced mesh.** One MultiMesh each for bodies, shadows, eyes, legs, tails, leaves, mouths and horns: eight draw calls however many creatures there are. `LucaWorld::get_creature_meshes()` lays every piece out each frame from the creature's real numbers; the shaders add the motion.
- **What each part looks like:** photo = green leaves sprouting from the top (more cover, more and bigger leaves); absorber = a red mouth low on the face (bigger with bite); flagellum = a tapering tail that wags; legs = one to three pairs that hold the body off the ground and step; sensor = white eyes with pupils (two when large) that blink; armour = bone horns along the back; storage = a fatter body; poison = violet spots on the skin. Shape genes stretch the body; fullness plumps it; species gives the hue and pigment darkens it ("Colour by species" makes the hues flat and vivid).
- **One liberty with "drawn from the body":** a blind creature still gets two small dot eyes so it has a face. Real eyes (whites and pupils) mean sensors.
- **Movement:** a body with legs walks (legs swing in turn, body bobs); one that moves with a tail and no legs hops in arcs with squash and stretch; one that stays put just breathes. All of it is driven by how far the creature actually moved last tick, and stops when the world is paused. A sleeping body flattens, greys, shuts its eyes, folds its legs and droops its leaves.
- **Shadows** are a soft disc per creature, lying along the slope, shrinking as the creature leaves the ground. No real-time shadow maps.
- **Ground:** one grid lifted by the height field in the vertex shader (so brushes reshape it live), at 0.35 of the sim's heights so hills are gentle. Colour comes from the same fields life uses: richer soil is greener, bare soil sandy, dim ground cooler, cold ground frosted, corpses leave pale flecks. The overlays are unchanged.
- **Camera:** perspective, looking down at 40°. Drag or WASD to pan, wheel to zoom, Q / E to turn.
- **Cost:** about 40 fps with 3,700 creatures on this Mac (measured with the window in the background, which macOS throttles). The sim itself is unchanged and no population cap was added.

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
| `dormancy` | How deeply it sleeps through dark and cold, 0 to 1 (LUCA: 0) | A little upkeep always; asleep it is helpless, and it wakes slowly, so it misses the dawn |
| `seed_throw` | How far a child can be flung, in cells (LUCA: 0) | Upkeep per cell of range, plus energy ∝ child mass × distance² for every throw |
| `parts` | Body modules | Each adds mass and upkeep |
| `brain` | Hidden neuron count and a list of weighted links, both evolved | Empty for LUCA; every neuron, link and unit of weight costs upkeep |

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
| `core/genome.*` | Genome, brain weight layout, gene limits + `clamp_genome()`, `make_luca()` | Scaffolding (extend freely) |
| `core/phenotype.*` | Genome → derived stats | **Uday** wrote steps 1–6; Claude added contact, storage, senses, brain (2026-11-11) |
| `core/energy.*` | Light demand, upkeep + wear, thermal, movement, contact flow, corpse rules | Claude, at Uday's request (2026-11-11); Uday tunes |
| `core/mutation.*` | Parent genome → child genome | Claude, at Uday's request (2026-11-11); Uday tunes |
| `core/world.*` | Heightmap (fBm value noise), light and temperature fields, time of day | Scaffolding |
| `core/soil.hpp` | Per-cell free nutrient, corpse matter and corpse energy | Scaffolding (built 2026-11-11) |
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
| `viewer/` | Godot 4.7 project: `main.gd` (camera, brushes, UI), `tree_view.gd` (tree of life), `ground.gdshader`, and one shader per creature piece sharing `creature.gdshaderinc` | Scaffolding (built 2026-11-11) |
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

Options: `--ticks --seed --size --pop --light uniform|gradient --sun --day --nutrients --interval --script --out --no-snapshot --quiet`.

God powers, headless: `./build/luca_headless --ticks 16000 --script scripts/long_winter.txt`. Each run also writes `chronicle.txt`, `powers.txt` (every command cast, replayable with `--script`) and `species.csv` (every species there has ever been, with its parent: the tree of life as a table).

### 10.3 Units (proposed; settle before tuning)

- 1 tick = 1 minute of world time
- 1 energy unit = what one cell receives per tick under full sun, **averaged over a day**. With the day/night cycle noon is twice that and night is zero.
- 1 day = `day_length` ticks (default 600, so the clock shown in the viewer is not literal minutes; `--day 0` turns the cycle off)
- 1 cell = `cell_size` world units (default 1)
- For scale: a LUCA burns about 0.35 a tick (0.46 before `METABOLIC_RATE` went to 0.75), holds up to 650, divides at 600, and its body is worth 244 (since 2026-11-11; before that 450, 400 and 44).

### 10.4 Rules the scaffolding enforces

- **Finite light, canopy rule (2026-11-11):** each cell gets `light` energy per tick. If the bodies in it ask for more, the biggest (by core radius) takes its fill first, then the next, and smaller ones get what is left; bodies of exactly the same size share alike. A full-patch LUCA asks for the whole cell, so a smaller body under it gets nothing unless the taller one's patch is sparse. Unabsorbed light is lost. (`Simulation::share_light_by_canopy`)
- **Costs capped at available energy;** a creature at zero dies.
- **Energy audit:** each metrics interval checks `stored(end) == stored(start) + in − out`. Expected error ≈ 1e-6 (float rounding). A jump means energy is being created or destroyed, and evolution will exploit it. "Stored" is energy in living creatures + energy locked in their bodies + energy left in corpses. "In" is light only. "Out" is upkeep, thermal, movement, transfer loss, corpse decay and waste (a full body can't hold more).
- **Matter audit:** soil nutrient + corpse matter + the mass of everything alive is constant (`Simulation::total_matter()`, column `matter_total`).
- **Reproduction transfers energy and matter** (never creates them): the parent pays the child's energy share plus the energy to build its body, and the body's mass comes out of the soil in the parent's cell. No free matter there, no child. A body tries once every 8 ticks.
- **Dispersal is paid for (2026-11-11):** a child lands touching its parent (their two reaches apart, about one cell for LUCAs). The `seed_throw` gene flings it up to that many cells further, at a cost (`throw_cost`, booked under movement). `offspring_spread` is gone. A child's path stops at any step in the ground taller than `birth_max_step` (4 height units between neighbouring cells; natural terrain tops out near 3.1), so a tall raised mountain blocks births as well as walkers.
- **Death leaves a corpse** in the cell: its mass, and the energy that was locked in the body (222 per unit of mass, so 244 for a LUCA). Corpses weather away slowly on their own (0.4% every 8 ticks); anything with an absorber breaks them down faster, and matter returns to the soil in proportion to the energy eaten.
- **Slope shade (2026-11-11):** the sun stands over the y = 0 edge. A cell loses light for dropping away from the sun (15% per unit of height per cell, up to 50%) and for lying below its rim, the ground three cells out (6% per unit of depth, up to 40%). `World::shade()` is a static field, recomputed around a terrain brush stroke only; `slope_shade` in the config scales it (0 = off). On the default map the mean is 0.94 and the darkest cell 0.53.
- **Matter washes downhill (2026-11-11):** every 64 ticks a cell sends `wash_rate` (0.001) × steepness of its free nutrient to its lowest neighbour (`World::downhill()`, eight neighbours; steepness is the drop over 2 height units, capped at 1). Corpse matter stays put until it is freed. All taking is worked out before any giving, and `total_matter()` is unchanged.
- **Day and night:** `World::set_time(tick)` runs at the start of every tick, after powers. Daylight is a pure function of the tick (no randomness), 0 at night to 2 at noon.
- **Contact:** a body with an absorber looks at the 3×3 cells around it. For each body within reach, energy flows at `edge × pull² / (pull + membrane)`, where `edge` is (taker radius / giver radius)², at most 2 (the size rule, 2026-11-11: equal bodies are unchanged, a body half its victim's size pulls a quarter as hard, and nothing is ever immune); 70% arrives, 30% is lost as heat. If several bodies pull on one, each gets the same fraction. Reach is capped at 1.5 cells, so very large bodies don't yet touch further.
- **Movement:** only a body with a motor part *and* a brain moves. A motor part with no brain gets a small random one at the next mutation.
- **The brain is evolved, structure and all (2026-11-11, Uday's request).** The genome holds a number of hidden neurons (0 to `MAX_HIDDEN` = 8) and a list of links (up to `MAX_LINKS` = 48), each from an input or hidden neuron to a hidden neuron or an output, with a weight. Inputs (7): a constant, hunger, light, crowding, cold, corpse here, own speed. Outputs (11): how hard to lean towards light, the living, the dead, comfort, soil matter, kin, bigger bodies, moving bodies; wander; inertia; throttle. Hidden neurons are tanh and all update together from the inputs and last tick's hidden values, so a neuron wired to itself or to another is memory (a small discrete-time recurrent net, in place of the CTRNN of 2.4). With no hidden neurons it is a set of reflexes, which is where every brain starts.
- **Wander drifts (2026-11-11):** the wander output pushes towards each creature's own wander angle (`Creatures::wander`), which turns by a normal step of `WANDER_TURN` (0.5 rad, in `sim.cpp`) each tick, about 23° on average. It used to be a fresh random angle every tick, and since every first brain starts with a wander weight of 1.0 (other weights ~0.1) movers jittered on the spot, facing a new way every frame (the "spinning in circles" in the viewer). A newborn's angle comes from its id, so births draw nothing from the `Rng`.
  - **Mutation:** nudge weights; add a link; lose a link; grow a neuron in the middle of a link (the old link stays); lose a neuron with its links. Rates are the `P_*_LINK` / `P_*_NEURON` constants in `mutation.cpp`.
  - **Cost:** upkeep per hidden neuron (`NEURON_COST` 0.012), per link (`LINK_COST` 0.002) and per unit of weight (`BRAIN_COST` 0.01), all in `phenotype.cpp`. A brain on a body that cannot move costs nothing and does nothing.
  - **Layout:** `compile_brain()` turns the genome into a fixed-size `BrainPlan`; `think()` runs it. Plans and hidden state live in their own arrays in `Creatures`, off the hot path; only movers read them.
- **Danger / kin sense (2026-11-11):** three brain outputs, felt only with a sensor. Each body within two cells pulls or pushes the creature by `OUT_KIN` if it is the same species, `OUT_BIGGER` × how much bigger it is (negative when smaller), and `OUT_MOVING` × how fast it moved last tick; nearer and heavier bodies count for more. Nothing says which bodies are enemies: the weights start as noise and evolve. It walks the spatial grid, so the cost is bounded by local crowding. **This is the one rule that reads species ids**, so `SPECIES_THRESHOLD` and the distance weights now touch behaviour.
- **Optional traits (`seed_throw`, `bite`, `digestion`)** stay at exactly 0 until a rarer mutation switches them on (`P_NEW_TRAIT`), and are lost again below a sliver. A plain nudge bounced off zero and left every lineage paying upkeep for a little of each.
- **Sunbathing and moving conflict (2026-11-11):** a body's light demand is multiplied by `light_while_moving(moved)`: 1 if it sat still last tick, falling in proportion to 0.2 at a stride of 0.02 world units or more (`MOVING_SHADE` 0.8, `FULL_STRIDE`, in `energy.cpp`; 0.08 until wander began to drift, see below in 10.7). It applies before the canopy shares light out.
- **Dormancy (2026-11-11):** each creature has a `sleep` level, 0 awake to 1. With the `dormancy` gene it climbs by 1/20 a tick, up to the gene's value, whenever its cell is dark or colder than its comfort range, and drops by 1/90 a tick otherwise (quick to sleep, slow to wake). At sleep `s` a body pays (1 − 0.9 s) of its upkeep and thermal cost, catches (1 − s) of its light and pulls (1 − s) as hard on the living and the dead. Above 0.1 it cannot move or breed; at 0.5 or more it ages at an eighth of the usual pace. Its membrane is unchanged, so it can be eaten in its sleep. Rules are `next_sleep` and `cost_while_asleep` in `energy.cpp`; `dormancy` is an optional trait like `bite`.
- **Poison (2026-11-11):** a new part type. A body's `toxicity` is its total poison size, capped at 2. Whatever drains it loses `toxicity` × the energy drained, as heat (`out_poison` in `TickStats`), paid out of what it holds, so a poisonous meal can cost more than it gives (a meal returns 70%). Poison is light (0.1 mass per unit) but dear to keep (0.12 upkeep per unit), and unlike armour does not slow the bite. It works on living bodies only: a poisonous body's corpse is safe to eat. Nothing can sense it yet, so eaters cannot learn to avoid it; they are only selected against.
- **Gene transfer (2026-11-11):** each tick that a body is draining another, there is a `gene_transfer_chance` (0.0001, in `config.hpp`; 0 = off) that it takes up the blueprint of one of that body's parts, picked at random, as a half-size bud (`transfer_part` in `mutation.cpp`, drawn from the sim `Rng` in contact order). "Touch" here means the existing contact rule, so only bodies with a mouth take anything up. The blueprint goes into the taker's **genome only**: its own body and stats do not change, and its children are built from the new plan and pay for the part. So a living body's genome can now hold parts its body lacks (the inspector's parts list reads the genome). `TickStats` counts transfers, cross-species ones and ones that bring a kind of part the taker lacked; the chronicle logs the first cross-species one.
- **Wear:** upkeep is multiplied by `1 + (age / 5000)²`. Nothing dies of old age as such; an old body costs more than it earns.
- **Observation only:** `fed_light / fed_living / fed_dead` per creature and `Simulation::census()` count ways of life from real energy flows. Sim logic never reads them.
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

### 10.6 Viewer (built 2026-11-11, rebuilt as pixel art, then as soft 3D, the same day)

> The drawing described below as pixel art (flat ground quad, sprites, `blob.gdshader`, `terrain.gdshader`) was replaced by the 3D view of 6.7. The UI, overlays, brushes, inspector, chronicle, species list and time controls are the same. The tree-of-life panel is still drawn in pixel art.
> **Checking the 3D view:** `./view.sh --parade` opens an empty world with one hand-made creature of each body plan (sitter, walker, hopper, armoured, poisonous, big hunter, sleeper). `--zoom=K` now means closeness (camera distance is 160 / K). Screenshots: `luca/docs/view3d_parade.png`, `view3d_crowd.png`, `view3d_world.png`.

Run with `./view.sh` (needs `thirdparty/godot-cpp`, which is git-ignored; the clone command is in the script). Only macOS is tested.

- **One class crosses the boundary:** `LucaWorld` (a `RefCounted`). Godot asks it for fields and for one MultiMesh buffer per frame; it tells it which power was cast.
- **Top-down, orthographic, flat.** The ground is one quad; `terrain.gdshader` draws tiles from the height, temperature, light and soil textures (nearest-filtered): height bands with ordered dither, three-step hill shading, dark cliff edges, snow where it is cold, darker rich soil, pale specks where corpses lie.
- **Creatures** are one MultiMesh of quads, one draw call; see 6.6. A sprite needs no rotation or scale, so the nine basis slots of each instance transform carry body data, and the shader reads them back from `MODEL_MATRIX`. Layout is documented in `godot/luca_world.h`.
- **Day and night** tint the whole picture. When days pass faster than about one a second, the tint holds a steady dusk instead of strobing.
- **Stats panel:** day and clock, population, the census (sun-fed, hunters, scavengers, mixed diet, able to move), and the six most numerous species with their colours.
- **Colour by species** (checkbox or `C`, `--species` on the command line): skin is painted in the species colour instead of the pigment. The hue rides in the unused height slot of each instance's origin.
- **Overlays:** natural, temperature, light, nutrients, the dead.
- **Tree of life** (`T`, or the button; `viewer/tree_view.gd`): a pixel-art tree with time running upward. The trunk is the oldest species; each species that ever had 10 alive branches off at the tick it was born. Living branches end in a crown in the species colour, sized by population; extinct ones are grey stubs; a limb is as thick as the life it carries. Hover a branch for its story, click it to make it the trunk (zoom in), click the trunk or right-click to step back out. Data comes from `LucaWorld.get_species_tree()`. `--tree` or `--tree=Name` opens it at start.
- **Inspector** follows the picked creature by id as it moves, and shows its species, the species it branched from, and its diet as percentages of sun / the living / the dead.
- **Sandbox brushes:** raise/lower land, warm/cold spot, sun and climate buttons, an instant/gradual toggle. A stroke is many small instant commands; only the first is written to the chronicle (`quiet` flag).
- **Painting works while paused:** `Simulation::flush_powers()` applies due powers immediately, with the same result as waiting for the next tick.
- **Time:** pause, 1x, 4x, 16x, max. The sim gets a 10 ms budget per frame; past that the world runs slower (7.1).
- **Not done yet in the 3D view:** a restyle of the tree-of-life panel to match, corpses as objects (they are flecks on the ground), level of detail for far creatures, and a check of frame rate with the window in front.
- **Not done yet:** position interpolation between ticks, the food web view, sim on its own thread, a pixel font for the UI.
- **Checking the viewer:** `./view.sh --warmup=40000 --zoom=8 --at=100,24 --frames=40 --shot=/tmp/x.png` skips ahead 40,000 ticks, zooms in, saves a screenshot and quits. Keep `--frames` small: macOS throttles a window that is behind other windows to a few frames a second, so a long run looks like a hang.

### 10.7 Baseline numbers (default settings, 128×128, seed 42, measured 2026-11-11)

**After wander drifts, with `FULL_STRIDE` 0.02 and `WANDER_TURN` 0.5 (current):**
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

- `energy.cpp`: `METABOLIC_RATE` (0.75 since the retune), `AGE_SCALE` (10000), `MOVING_SHADE`, `FULL_STRIDE` (0.02), `MOVE_COST`, `CLIMB_COST`, `CONTACT_RATE`, `CORPSE_RATE`, `THERMAL_SCALE`; `energy.hpp`: `TRANSFER_EFFICIENCY`, `CORPSE_WEATHERING`.
- `phenotype.cpp`: thrust, membrane and armour numbers, capacity, `BODY_ENERGY_PER_MASS`, `BRAIN_COST`, `TOLERANCE_COST`.
- `energy.cpp`: `SIZE_POWER`, `SIZE_EDGE_MAX` (the size rule at the mouth).
- `mutation.cpp`: the `P_*` rates, including `P_NEW_TRAIT` and the brain's `P_ADD_LINK`, `P_DEL_LINK`, `P_ADD_NEURON`, `P_DEL_NEURON`.
- Brain: `NEURON_COST`, `LINK_COST`, `BRAIN_COST` (`phenotype.cpp`); `MAX_HIDDEN`, `MAX_LINKS` (`genome.hpp`).
- `phenotype.cpp`: `BITE_UPKEEP`, `DIGEST_GAIN`, `DIGEST_UPKEEP`; `sim.cpp`: `SOCIAL_CELLS`, `SOCIAL_FAST` (the kin sense), `WANDER_TURN` (how fast wandering turns).
- `config.hpp`: `day_length`, `nutrient_mean`, `nutrient_patchiness`, `gradient_min`, `birth_max_step`, `slope_shade`, `wash_rate`, `gene_transfer_chance`.
- `world.cpp`: `FACING_LOSS`, `VALLEY_LOSS`, `HORIZON`, `STEEP_DROP` (shade and drainage).
- Dispersal: `THROW_COST` (`energy.cpp`), `THROW_UPKEEP` (`phenotype.cpp`), `limits::MAX_THROW`.

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
- [ ] **Cold nights.** Temperature swings with day and night, and a big body feels a smoothed version of it (it holds heat). Big bodies should do better in cold places without being told to.

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
- [ ] **Waste.** Living produces a waste chemical in the cell that slowly poisons whatever made it, unless something else absorbs it as food. A second cost of crowding and a niche for cleaners. The smallest useful piece of the chemistry layer (2.1).
- [ ] **Growing up.** Children are born small and grow by taking in matter, instead of appearing full-size. Young are vulnerable, and many-small versus few-large offspring becomes a real choice. The largest item: it touches the phenotype compiler and the matter books. Best judged once species are visible.
- [ ] **Evolvable mutation rate and lifespan.** Two genes: how sloppy copying is, and how much energy goes into repair to slow wear. Each needs a cost (8.2).
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
- **The look is soft 3D** (6.7). Creatures are instanced meshes, never Godot nodes; a new body part gets a piece in `LucaWorld::get_creature_meshes()` and a shader (or a place in an existing one) in `viewer/`.
