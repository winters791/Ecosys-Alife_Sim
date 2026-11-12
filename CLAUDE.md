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

### 2.1.1 Chemistry layer: design (proposed 2026-11-12; all four stages built the same day)

**Why now (Uday, 2026-11-12):** the game kept gaining one special gene per way of feeding (`detox` for waste, `chemo` for vents, `digestion`, and a `graze` gene that was tried and shelved). That is the opposite of an emergent system. The chemistry layer is the one rule meant to replace them. Decisions taken: **energy lives in chemicals** (there is no separate energy store), reactions may be **one-to-one and two-input**, and grazing is shelved until the chemistry exists (`luca/shelved/grazing.patch`, applies with `patch -p1` from `luca/`).

**Chemicals.** A few kinds of matter (start with four, A to D), each with a fixed energy level per unit: A 0 (spent, like today's free soil nutrient), B and C in between, D 222 (so a body made of D is worth what a body is worth today). Matter is conserved unit for unit; energy is the difference in levels. Soil, corpses, bodies and every creature's store are amounts of each chemical.

**Two books, both exact.** Stored energy = the sum over every chemical everywhere of amount x level. Matter = the sum of all amounts. Energy enters only by running reactions uphill (light, vents, windfalls), and leaves only as heat.

**Enzymes are the genome's chemistry.** A list in the genome, like parts. Each one is a reaction and a rate:
- One-to-one, X -> Y, or two-input, X + Y -> Z + W (one unit each, so matter balances).
- Running downhill releases the difference in levels as energy the body can spend (respiration, digestion, chemosynthesis are all this).
- Running uphill needs energy: light caught by sun patches, spent on whatever uphill enzymes the body carries (photosynthesis is light running A -> D, but a body could as well make B or C).
- Every enzyme costs upkeep in proportion to its rate, whether or not its inputs are there.
- Mutation adds, deletes, duplicates and retunes enzymes, and swaps one chemical in a reaction for another.

**The store is chemical.** A body holds pools of each chemical (total held is capped by its capacity, as energy is today). It pays upkeep, movement and thermal cost by running its downhill enzymes on its pools; a body that cannot release enough dies. LUCA: one sun patch and one enzyme, D -> A.

**Eating moves chemicals, not energy.** Contact drains the giver's pools; corpses hold the chemicals the body was built of. What arrives goes into the taker's pools; what is lost in the transfer (today 30%) is spilled into the cell as the same chemicals, not burned. What a body can get out of a meal depends on its enzymes. Chemicals it has no use for fill its pools, and it gives off what it cannot hold into its cell.

**Uptake from the cell.** A body takes up from its cell the chemicals its enzymes use, through its skin, up to what it can hold. This replaces `detox` (waste is just chemicals others gave off) and `chemo` (a vent is a cell where A is turned into a richer chemical, so energy arrives with no matter, and a body with the right enzyme feeds on it).

**Bodies are built of what they hold.** Growing and breeding take chemicals from the body's own pools (no tissue gene). A plant that makes D is made of D; an animal living on B is made of B. So what a meal is made of, and so who can digest it, comes from the eater's way of life: herbivores and carnivores without a gut gene.

**Genes this retires:** `detox`, `chemo`, `digestion` (and the shelved `graze`). Kept for now: everything about bodies, senses and brains. Poison and fouling are revisited once chemicals exist (both could become chemicals).

**Stages:**
1. Chemicals in soil, corpses and bodies; stores become pools; light runs uphill enzymes; LUCA burns D. Both books exact. Judge: the world still lives on six seeds. *Built 2026-11-12 (`core/chem.hpp`; rules in 10.4 under "Chemistry"; numbers in 10.7). The world lives on all six seeds.*
2. Contact, corpses and spills move chemicals; uptake from the cell; excretion. `detox` goes. *Built 2026-11-12 (rules in 10.4 under "Loose chemicals"). The calls taken: a body keeps and takes up only what its enzymes can burn (a chemical on the richer side of one of its enzymes); a meal's lost share and everything it can't burn spill into the cell; it gives off whatever its store holds that it can't burn; fouling comes only from loose chemicals a body can't burn. The old waste rule (20% of upkeep given off) is gone, so in a world of D-burners nothing fouls anything.*
3. Vents and windfalls as conversions in a cell. `chemo` goes; `digestion` goes. *Built 2026-11-12 (rules in 10.4 under "Vents"). Uday's calls: a vent makes **C**, and every eater keeps **70%** of a meal. A vent first made C out of its **own cell's A**; that was changed the same day (below) to **bringing it up from below**, new matter. Windfalls stay in the corpse as proposed. Vent food is told apart by a tracer (`Soil::vent_made`), so `fed_vent` and the chemotroph census still work. **Finding, and the change it led to:** with vents using their cell's A, no vent was colonised in six seeds: each turned all the A within its radius into C within 20,000 to 40,000 ticks, the C drifted away, and the vent was left bare ground that made nothing and grew nothing. Uday chose to have vents bring up matter instead. Then two seeds of six evolved vent dwellers, but the world's matter more than doubles in 100,000 ticks (Known weak spots).*
4. Two-input reactions. *Built 2026-11-12 (rules in 10.4 under "Two-input reactions"). 300 bodies a world carry one; nothing uses a neutral one yet.*
Test at each stage: do cross-feeding (one lineage living on what another gives off) and diet splits appear without being coded (milestone 5).

**Next session: start here (written 2026-11-12 after stages 3 and 4).** All four stages are built; 10.4 ("Chemistry", "Loose chemicals", "Vents", "Two-input reactions") describes the code as it is, and 10.7 has the numbers. Where things live: `core/chem.hpp` (chemicals, `Mix`); `Enzyme`, `canonical_enzyme`, `enzyme_words` (`genome.*`); `Phenotype::enz_in/enz_out/enz_step/neutral` and `burns` (`phenotype.*`); `burn` / `charge` / `power` / `convert` / `uptake_rate` / `fouling_cost` (`energy.*`); `Simulation::run_vents` and phase 2b of `Simulation::step` (`sim.cpp`), which per body does, in order: drained by others, meals in (`take_in`), uptake, light charged, neutral exchanges, costs burned, giving off, death, growing, breeding. `tests/test_life.cpp` `test_loose_chemicals`, `test_vents` and `test_two_input_reactions` show how to build bodies with chosen enzymes.

Open after stage 4 (Uday's calls):
- **Vents bring up matter now (decided 2026-11-12), and it piles up.** A vent's C is new matter, and what nobody eats breaks down to A near the vent and stays: under each test vent the soil holds about 400 units a cell by tick 100,000 (the map's mean is 6), still rising. Levers if it matters: a sink (soil A seeping back down at vents, or anywhere), or a vent that brings up only while its cell holds less than some amount. Left as it is for now (Uday, 2026-11-12).
- **Two-input enzymes cost the same per unit of rate as one-to-one ones** but move two units of each side, so A + A ↔ D + D stores 444 a unit against 222 for A ↔ D at the same upkeep. Not taking over (16 to 39 carriers a world), but charging upkeep per unit of matter turned over (twice the rate) would be the even rule.
- **The design's example uses were wrong:** D + A → B + B is downhill (it releases 74), not neutral; the neutral transporter is D + A ↔ C + B. B + C ↔ A + D is neutral too, not uphill. Both are possible now; neither is used.

*Measuring milestone 5 (all stages):* metrics now give `loose_B/C/D`, `rx_XY`, `rx_two`, `rx_neutral`, `two_flow`, `neutral_flow`, `vent_loose` and the pool and tissue mixes, and the creature snapshot lists each body's enzymes (`enzymes`, e.g. `A-D A+A-D+D`). Still missing: a probe that, at the end of a run, lists per species the chemicals it burns, what share of its food is uptake, and which species' leavings it takes up (who gave off the chemical in its cell). Cross-feeding is "species X lives mostly on a chemical species Y gives off".

*Still open from 2.1.1:* poison and fouling as chemicals; a brain input for loose chemicals (nothing can sense them yet); excretion beyond what a body can't burn (nothing gives off a chemical it could burn but has too much of).

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
- **Brain genome:** a small recurrent net: a CTRNN (built 2026-11-11 as an evolvable discrete-time net, made a CTRNN 2026-11-12; see 10.4). Inputs come only from sensors the body has, and outputs drive only effectors it has. A blind creature has no visual inputs.
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
4. **Add brains and sensors.** Test: does real behaviour emerge (chasing, fleeing, foraging)? *First version built 2026-11-11; hidden neurons became a CTRNN with evolved time constants 2026-11-12. Movers evolve and persist; whether they chase or flee has not been measured.*
5. **Add the chemistry layer.** Test: cross-feeding or symbiosis. *All four stages built 2026-11-12 (2.1.1): energy lives in chemicals, enzymes (one-to-one and two-input) are genes, spills and leavings are food through the skin, and vents make a chemical. Cross-feeding has not been seen: loose B and C stay at a few units a world, and nobody lives on a vent.*

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
- Brushes: height, heat and vents (built 2026-11-12), and later water and nutrients.

### 4.7 A world that acts on its own

Storms, eruptions, slow climate drift and the odd meteor happen without the player, at a rate set by a slider (off is allowed). They are ordinary power commands cast by the world from the sim's `Rng`, so they are deterministic, logged and replayable like the player's. The point: you come back to a world with a history you didn't write. *Built 2026-11-12 (`core/events.*`, see 10.5). The dice are a stream of their own seeded from the world seed, not the sim's `Rng`, so a rate of 0 leaves the world exactly as before and a power log replays a run with events off.*

### 4.8 Favourites and the fossil museum

- **Favourites:** pin a lineage to follow it through deep time, with alerts when something happens to it (*"Your Velox just lost its eyes"*). Observation only; a favourite gets no help. *Following a species is built (2026-11-12, see 10.4 and 10.6); fossils are not.*
- **Fossils:** an extinct species leaves fossils where it lived. Digging one up adds its portrait and a short life story to a museum collection.

### 4.9 Seed vault and life-bearing meteors

Bank any living lineage (a ~20-individual sample, same as a creature file) in a vault, and drop it back into this world or a friend's as a meteor. A reintroduction is an invasive species under exactly the rules of section 5: genome only, stats recomputed, evolution takes over. This is also how imported creature files arrive in the world.

*Built 2026-11-12 with the creature designer (4.10): banking from the inspector or the species card, the vault as a folder of creature files, and release by meteor (the `seed` power, 10.4 "Meteors"). Friends' worlds: the files are plain text and travel as they are; the PNG portrait wrapper (section 5) is not built.*

### 4.10 Creature designer (Uday's request, built 2026-11-12)

A screen of its own for building a creature by hand, gamified: draw the body, stamp parts on it, wire its brain, pick its enzymes, set its life cycle, and see at every step whether it would live. **The design is only a genome.** Released, it is an invasive species (section 5): it pays its costs under the world's physics, and evolution takes over from its first child. Nothing in the designer is a rule of its own; every number comes from the real phenotype compiler and energy rules (`core/design.*`).

- **Body:** a pen pulls the outline (length, width, boxiness); parts are stamped on the rim (mirrored in pairs by default), dragged round it or out on a stalk, resized with the wheel. Coverage arcs (mouth reach, armour, sun patch) are drawn.
- **Brain:** the CTRNN as a graph, senses left, urges right, neurons between; drag to wire, wheel to weigh. Ready-made reflex cards (Wander, Hunt, Stop to eat, Run when bitten, ...) add wiring. "Try it" sets the senses and shows the urges after N ticks (`probe_brain`).
- **Chemistry:** enzyme cards (X [+ X2] ⇄ Y [+ Y2], rate), with what each releases or stores.
- **Life:** breeding threshold, child's share, birth size, seed throw, comfort, tolerance, bite, dormancy, repair, mutation rate.
- **Test tank:** a 32 × 32 flat world of its own (with or without wild LUCAs, under the habitat's sun and air) where a dozen of the design land by meteor; population chart, generations, diet.
- **Energy budget, always in view:** a verdict (Thrives / Gets by / Must eat / Needs a vent / Starves / Won't live), sun in against upkeep out, the upkeep bill line by line (each part, enzyme and gene by its marginal cost), its body in numbers, warnings (e.g. a motor with no brain, enzymes too slow to pay its costs).
- Templates: Luca, Swimmer, Tank, Tentacles, Frond, Walker, Puffball, Hunter (`design_templates`; the parade uses all but Hunter).

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
| **Phenotype** | Stats derived from the genome | Computed at birth, and again at each step of growing up | Mass, upkeep, photosynthesis rate, max speed |
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
| `dormancy` | How deeply it sleeps through dark and cold, 0 to 1 (LUCA: 0) | A little upkeep always; asleep it is helpless, and it wakes slowly, so it misses the dawn |
| `seed_throw` | How far a child can be flung, in cells (LUCA: 0) | Upkeep per cell of range, plus energy ∝ child mass × distance² for every throw |
| `elongation` | Up to 3× as long as wide, same volume (0..1, LUCA: 0) | More skin (sun room, but heat loss, fouling, `SKIN_UPKEEP`); a long body moves more cheaply and faster (2026-11-12) |
| `mutability` | How sloppily it is copied: every mutation chance × 2^mutability, from −3 (an eighth) to +2 (four times) (LUCA: 0) | Below 0, `FIDELITY_UPKEEP` 0.02 per halving (proofreading); above 0 free, paid for in broken children (2026-11-12) |
| `repair` | Slows wear: the age clock runs 1 + 2 × repair times slower, 0 to 1 (LUCA: 0) | Up to 15% more upkeep (`REPAIR_SHARE`), so a bigger body pays more |
| `squareness` | Round to box (0..1, LUCA: 0) | Up to 24% more skin, and 30% more drag on the move |
| `birth_size` | Fraction of adult mass a child is born at, 0.1 to 1 (LUCA: 0.5); it grows the rest itself | A small child is cheap to make but easy to eat, catches little light and can't breed until grown; a big one costs its parent more (2026-11-12) |
| `parts` | Body modules, each with a size, an angle on the outline (`theta`) and, for mouths and sun patches only, a stalk | Each adds mass and upkeep; a stalk adds mass and upkeep per unit of length. Where a part sits matters (10.4) |
| `brain` | Hidden neuron count, a list of weighted links, and per neuron a time constant `tau` (1 to 1,000 ticks) and a bias, all evolved | Empty for LUCA; every neuron, link and unit of weight or bias costs a little upkeep (nearly nothing since 2026-11-12), and a slow neuron costs `MEMORY_COST` per e-fold of tau |

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
3. **Periodic split checks:** every few thousand ticks, sample each species and check whether it's forming two separate clusters (e.g. either side of a mountain). If so, split it and give the new one a procedural name. *Built 2026-11-12 (`Simulation::check_splits`, see 10.4).*

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
| `core/events.*` | The world acting on its own: storms, eruptions, meteors, climate drift, cast as powers | Scaffolding (built 2026-11-12) |
| `core/chronicle.hpp` | Append-only natural history log | Scaffolding (built 2026-11-11) |
| `core/creature_file.*` | Creature files: a name, an origin and one or more genomes, as text (read, write, validate) | Scaffolding (built 2026-11-12) |
| `core/design.*` | The designer's sums (`assess_design`: upkeep line by line, income, warnings), templates, `probe_brain`, `bank_species`, and `Trial` (the test tank) | Claude, at Uday's request (2026-11-12) |
| `core/species.*` | Genome distance, species registry (ids, names, parents, populations, splits) | Scaffolding (built 2026-11-11); Uday tunes the weights |
| `core/rng.hpp` | Deterministic xoshiro256** | Scaffolding |
| `core/config.hpp` | World settings and units | Scaffolding |
| `headless/main.cpp` | CLI runner | Scaffolding |
| `tools/plot.py` | `run.png` + `map.png` | Scaffolding |
| `godot/` | GDExtension glue: `LucaWorld` wraps the sim and hands Godot packed arrays | Scaffolding (built 2026-11-11) |
| `viewer/` | Godot 4.7 project: `main.gd` (camera, brushes, UI), `tree_view.gd` (tree of life), `species_card.gd` (species card), `designer.gd` (creature designer and seed vault) with `design_canvas.gd`, `brain_graph.gd` and `sprite_stage.gd`, terrain and blob shaders | Scaffolding (built 2026-11-11) |
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

Options: `--ticks --seed --size --pop --light uniform|gradient --sun --day --day-swing --windfall --nutrients --interval --script --follow --events --out --no-snapshot --quiet`. `--events F` lets the world act on its own (default 0, off). `--follow NAME` follows the species of that name from the moment it appears and writes its alerts to `alerts.txt`.

God powers, headless: `./build/luca_headless --ticks 16000 --script scripts/long_winter.txt`. A script can drop creatures: `<tick> seed <x> <y> <radius> <count> <creature file on one line, ' ; ' between lines>`. Each run also writes `chronicle.txt`, `powers.txt` (every command cast, replayable with `--script`) and `species.csv` (every species there has ever been, with its parent: the tree of life as a table; the last column, `split`, is 1 for a species made by a split check).

### 10.3 Units (proposed; settle before tuning)

- 1 tick = 1 minute of world time
- 1 energy unit = what one cell receives per tick under full sun, **averaged over a day**. With the day/night cycle noon is twice that and night is zero.
- 1 day = `day_length` ticks (default 600, so the clock shown in the viewer is not literal minutes; `--day 0` turns the cycle off)
- 1 cell = `cell_size` world units (default 1)
- For scale: a LUCA burns about 0.35 a tick (0.46 before `METABOLIC_RATE` went to 0.75), holds up to 650, divides at 600, and its body is worth 244 (since 2026-11-11; before that 450, 400 and 44). Since chemistry stage 1 these are chemicals: a LUCA's body is 1.1 units of D, its store holds up to 2.9 units, and its one enzyme turns over 0.015 D a tick (3.3 energy).

### 10.4 Rules the scaffolding enforces

- **Finite light, canopy rule (2026-11-11):** each cell gets `light` energy per tick. If the bodies in it ask for more, the tallest (`Phenotype::canopy`: core radius, plus the stalks its sun patches stand on, since 2026-11-12) takes its fill first, then the next, and smaller ones get what is left; bodies of exactly the same size share alike. A full-patch LUCA asks for the whole cell, so a smaller body under it gets nothing unless the taller one's patch is sparse. Unabsorbed light is lost. (`Simulation::share_light_by_canopy`)
- **Costs capped at available energy;** a creature at zero dies.
- **Energy audit:** each metrics interval checks `stored(end) == stored(start) + in − out`. Expected error ≈ 1e-6 (float rounding). A jump means energy is being created or destroyed, and evolution will exploit it. "Stored" is energy in living creatures + energy locked in their bodies + energy left in corpses + energy in waste; since chemistry stage 1, the first three are the energy of the chemicals in every store, body and corpse (amount × level). "In" is light plus windfalls (`in_windfall`) plus vents (`in_vent`). "Out" is upkeep (less the part given off as waste), thermal, movement, transfer loss, corpse decay, waste breaking down, fouling, poison and overflow (`out_waste`: a full body can't hold more; not the waste chemical).
- **Matter audit:** soil nutrient (chemical A) + corpse matter + loose chemicals + the mass of every living store and body changes only by what vents bring up from below: `total_matter() - matter_brought_up()` is constant (`Simulation::total_matter()`, column `matter_total`; `TickStats::matter_in`, column `matter_in`). With no vents, total matter is constant.
- **Reproduction transfers energy and matter** (never creates them). Since chemistry stage 1, everything comes out of the parent's store, as it is mixed: first the child's body, then the cost of a throw (burned), then the child's store, `offspring_share` of what is left. A body breeds when its store is worth `repro_threshold` and holds more than 0.75 × `birth_size` of its body's mass. It tries once every 8 ticks.
- **Dispersal is paid for (2026-11-11):** a child lands touching its parent (their two reaches apart, about one cell for LUCAs). The `seed_throw` gene flings it up to that many cells further, at a cost (`throw_cost`, booked under movement). `offspring_spread` is gone. A child's path stops at any step in the ground taller than `birth_max_step` (4 height units between neighbouring cells; natural terrain tops out near 3.1), so a tall raised mountain blocks births as well as walkers.
- **Death leaves a corpse** in the cell: since chemistry stage 1 its body and whatever its store held go into the cell's corpse, a `Mix` of chemicals (a LUCA's body is 1.1 units of D, worth 244). Corpses weather away slowly on their own (0.4% every 8 ticks), burning down to A in the soil; anything with an absorber eats them as chemicals (see Chemistry below).
- **Windfalls (2026-11-12, Uday's request):** at the start of every tick, after powers, food falls on random cells: `windfall_rate` (0.001, `config.hpp`) energy per cell per tick on average, in lumps of `windfall_size` (250, about one LUCA corpse), so one lump every ~16 ticks on a 128×128 map. The number of lumps per tick is the whole part of the mean plus one more by chance, and each lands on a uniformly random cell; all from the sim `Rng` (`Simulation::drop_windfalls`). A lump goes into the cell's corpse, so it is eaten, sensed (the "corpse here" input and the lean towards the dead) and weathered exactly like a corpse. Since chemistry stage 1 a lump is D made out of the cell's own A (`windfall_size` / 222 units, or what A the cell has), so it carries matter, and a bare cell gets nothing. It is the second way energy enters the world: booked in `energy_in` and broken out as `in_windfall` (metrics columns `in_windfall`, `windfalls`). Because it sits in the corpse field, food from it counts as `fed_dead`: a body living on windfalls shows in the census as a scavenger. `--windfall 0` turns it off.
- **Growing up (2026-11-12):** a child is born at `birth_size` of its adult mass and grows the rest. A young body is the adult scaled down by mass (`compile_phenotype(g, grown)`): mass, body energy, storage and upkeep (Kleiber again) shrink with it, its length goes as the cube root and its skin, sun area and canopy height with it, and it loses heat faster. Pull, armour and speed are not scaled; the size rule at the mouth already makes a small body easy to drain and weak at draining. Every 8 ticks (`age & 7 == 4`) an awake young body puts the energy it holds above `GROW_RESERVE` (0.3) of what it can hold into body, at most `GROW_STEP` (5%) of its adult mass at a time and at least `GROW_MIN` (1%) unless that finishes it, and is recompiled at its new size (`Phenotype::grown`). Since chemistry stage 1 the body is built out of its own store, as it is mixed, so a plant's young are made of what it made; the soil only matters through the light the store was filled with. Only a grown body breeds. The parent pays for the child's body at birth size out of its own store. Chemicals move from store to body, so neither book changes; `TickStats::grown_mass` counts the matter built. Bodies placed by `add_creature` and the first LUCAs start grown (it takes an optional `grown`).
- **Chemistry, stage 1 (2026-11-12, 2.1.1; `core/chem.hpp`):** matter is four chemicals, A to D, at energy levels 0 / 74 / 148 / 222 a unit (`LEVEL`). A is the soil's free nutrient (`Soil::nutrient`); a store (`Creatures::pool`), a body (`Creatures::tissue`) and a corpse (`Soil::corpse`) are each a `Mix` of chemicals, and what they are worth is amount × level. A store never holds A, and holds at most `pool_capacity` units (`energy_capacity` / 222).
  - **Enzymes are genes** (`Genome::enzymes`, up to `MAX_ENZYMES` 8): each is a reversible reaction X ↔ Y (or, since stage 4, X + Y ↔ Z + W; see "Two-input reactions") with a rate in units a tick (`MIN_ENZYME_RATE` 0.0005 to `MAX_ENZYME_RATE` 0.2). Upkeep is `ENZYME_UPKEEP` (1.0) per unit of rate, used or not. LUCA has one, A ↔ D at 0.015. Mutation adds, deletes, copies and retunes enzymes and swaps one chemical of a reaction for another (`P_ENZ_*`); the species distance counts them (`W_ENZ_KIND`, `W_ENZ_RATE`).
  - **Spending is burning** (`burn`): every cost is paid by running enzymes downhill on the store, in the order they sit in the genome, each at most its rate. A reaction that ends at A gives the A to the cell's soil. A body dies when it can't pay its costs in full this tick.
  - **Gaining is charging** (`charge`): light and a vent's flow are kept by running enzymes uphill; one that starts at A takes it from the cell's soil, and only while the store has room. Light it can't store is never taken in.
  - **Power caps moving** (`power`): the most a body can release in a tick is what its enzymes turn over. A step costing more than that, less upkeep and heat, is not taken, so a fast or climbing body needs fast enzymes and pays their upkeep.
  - **Eating moves chemicals:** contact drains the giver's store as it is mixed, and corpses are eaten as chemicals. Since stage 2 the eater keeps `efficiency` of what it can burn, or what fits in its store, and the rest spills into its cell (next bullet); in stage 1 the rest burned.
- **Loose chemicals, chemistry stage 2 (2026-11-12; replaces waste and `detox`):** each cell holds chemicals outside any body (`Soil::dissolved`, a `Mix`; `Soil::waste` is what they are worth, kept in step for the viewer and metrics). A body **can burn** a chemical when one of its enzymes has it on the richer side (`Phenotype::burns`, `can_burn`); LUCA burns only D.
  - **Spills:** of a meal, living or dead, an eater keeps `efficiency` of the chemicals it can burn (or what fits in its store). The rest, and every chemical it can't burn, spills into its cell as it is. Nothing burns at the mouth any more (`out_transfer` and `out_waste` stay 0): the old 30% transfer loss is food for whatever is near.
  - **Giving off:** after paying its costs, a body gives off into its cell whatever its store holds that it can't burn: what its own enzymes made (a body burning D to B gives off the B) or what its parent handed down. Counted, with spills, in `excreted`.
  - **Uptake through the skin:** each tick a body asks for `UPTAKE_RATE` (0.05) × skin of its cell's stock of every chemical it can burn (`uptake_rate`; × (1 − sleep)). If the bodies in a cell ask for more than all of it, each share is scaled down. It takes up only what its store has room for. Counted as `eaten_waste` and `fed_waste`, so the census's **cleaners** are bodies living mostly on loose chemicals. No gene: what a body can take up is set by its enzymes.
  - **Fouling:** a body loses `FOUL_HARM` (0.5) × skin a tick per unit of loose chemical in its cell that it can't burn (`fouling_cost`, `out_fouling`), asleep or not. So one body's leavings are another's food and a third's poison.
  - **Drift and decay:** every 8 ticks each cell hands `WASTE_SPREAD` (20%) of its loose chemicals to its four neighbours and `WASTE_DECAY` (4%) of what is left breaks down to A in the soil, its energy lost as heat (`out_waste_decay`) (`Simulation::spread_dissolved`).
  - The cross-feeding the design asks for needs nothing more: a body with D ↔ B gives off B, and a neighbour with A ↔ B takes it up and lives partly on it (`test_loose_chemicals`). Metrics: `loose_B`, `loose_C`, `loose_D` (total units loose in the world); the snapshot's `detox` column is now `burns` (the bit mask). The inspector and species card say what a body burns.
  - `nutrient_mean` went from 0.8 to 6, because stores now hold matter.
  - The metrics give `pool_B/C/D` and `tissue_B/C/D` (total units), `mean_enzymes`, and `rx_XY` (bodies carrying each reaction). The inspector lists enzymes and what the store and body are made of.
- **One share of a meal for all (chemistry stage 3):** the `digestion` gene is gone (`DIGEST_GAIN`, `DIGEST_UPKEEP`, `W_DIGESTION`, `mean_digestion` with it); every eater keeps `TRANSFER_EFFICIENCY` (70%) of what it can burn and spills the rest. Uday's call.
- **Two-input reactions (chemistry stage 4, 2026-11-12):** an `Enzyme` may have a second input and output, X + Y ↔ Z + W, one unit of each, so matter balances (`x2`, `y2`; `NO_CHEM` on both for one-to-one). `canonical_enzyme` (`genome.cpp`) is the one way to write one: sides sorted, the poorer side first, never the same on both sides; `clamp_genome` and the phenotype compiler both use it. Written in files and the snapshot as e.g. `A+A-D+D` (`enzyme_words`).
  - **Burning and charging** (`burn`, `charge`, `power` in `energy.cpp`) run every enzyme the same way: downhill from the richer side to the poorer when burning, uphill when charging, releasing or storing (level of one side − level of the other) a unit. A runs only as far as the scarcer input allows (A comes from the soil, and X + X needs two X a unit) and, when it leaves the store holding more than before (A taken from the soil into richer chemicals), only as far as there is room. One-to-one enzymes behave exactly as before. `power` now takes the cell's soil A.
  - **Neutral exchanges:** a reaction with equal levels on both sides (B + C ↔ A + D, D + A ↔ C + B, ...) releases nothing either way. The compiler finds what the body burns directly (the richer side of every downhill enzyme) and runs each neutral enzyme from the side holding fewer of those to the side holding more; if both hold as many, it never runs (and still costs upkeep). A running one takes its inputs every tick at its rate, as far as they and room allow (`convert`, called in phase 2b after light is charged), and its input chemicals count as burnable (kept from meals, taken up through the skin). So a D-burner with B + C → A + D lives on loose B and C. `Phenotype::neutral` is the bit mask.
  - `burns` now means: on the richer side of a downhill enzyme, or on the input side of a running neutral one.
  - **Mutation:** `P_ENZ_PAIR` (0.01) gives a one-to-one enzyme a random second pair or takes a two-input one's away; a swap can change any of the four chemicals. New enzymes are one-to-one.
  - **Species distance:** each distinct reaction is its own kind (`W_ENZ_KIND`), so X + Y ↔ Z + W is not X ↔ Z.
  - **Cost:** `ENZYME_UPKEEP` per unit of rate, as for one-to-one (so a two-input enzyme moves twice the matter for the same upkeep; see 2.1.1 "Open after stage 4").
  - **Metrics:** `rx_two` (bodies with any two-input enzyme), `rx_neutral` (bodies with a running neutral one), `two_flow` (units turned over by two-input enzymes) and `neutral_flow` (by neutral ones). The creature snapshot ends with an `enzymes` column. The inspector writes them `A+A<->D+D`, and neutral ones with `<=>`.
- **Vents (2026-11-12; a chemical from below since chemistry stage 3):** a world layer (`World::vent`, strength 0 to `vent_max` 3) painted by the `vent` brush power (open +, close −), like height and heat. It warms its cell by `vent_heat` (12 °C) per unit of strength. Each tick, day and night, a vent cell brings up loose C (`VENT_CHEM`, `energy.hpp`) from below, `vent_power` (1.0) × strength energy's worth (1/148 of a unit a tick at strength 1), whatever its soil holds (`Simulation::run_vents`). Energy and matter are both new: the energy is booked in `energy_in` and broken out as `in_vent` (the third way energy enters the world), the matter as `matter_in` (the only way matter enters; `Simulation::matter_brought_up()` is the running total). What nobody eats breaks down to A where it drifts, so the ground around a vent grows rich. (For a few hours on 2026-11-12 the vent made C out of its own cell's A; see 2.1.1 stage 3 for why that was dropped.) There is no gene: bodies that can burn C take it up through their skin like any loose chemical, and bodies that can't are fouled by it. The `chemo` gene, `CHEMO_RATE`, `CHEMO_UPKEEP` and `vent_demand` are gone.
  - **Telling vent food apart (observation only):** `Soil::vent_made` counts how much of a cell's loose C a vent made. It drifts and breaks down with the rest, and uptake takes from it in proportion, so C taken up on or near a vent counts as `fed_vent` and the rest as `fed_waste`. The census's **chemotrophs** (over 60% of food from vents) and the species card's diet still work. Metrics column `vent_loose` is the world's total.
  - With no vents painted, the sim skips the vent pass (`World::any_vents`).
- **Meteors: creatures from outside (2026-11-12, 4.9; `Simulation::land_meteor`):** the `seed` power drops `count` (1 to 500) bodies from a creature file within `radius` of a spot, on a sunflower spiral, cycling through the file's genomes. Each is clamped, compiled and lands grown, with a store worth 60% of its breeding threshold made of the richest chemical it can burn (a plant arrives full of D, a vent dweller of C) and a body of D. That energy and matter come from outside: booked as `energy_in` (broken out as `in_meteor`, metrics column `in_meteor`) and `matter_in` (`matter_brought_up()` includes it). They all join one species: the nearest living species if the founder is close enough, otherwise a new root species named after the file, with one lineage. The chronicle says where it fell. Cast between ticks (the viewer while paused) it lands at once and is booked in the next tick's stats. A meteor touches no creature already there. Files: `core/creature_file.*`; version 1, `luca-creature 1`, then `name`, `origin` and one `body` block per genome; unknown keys are an error, and every genome is clamped on reading.
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
- **The brain is evolved, structure and all (2026-11-11, Uday's request).** The genome holds a number of hidden neurons (0 to `MAX_HIDDEN` = 8) and a list of links (up to `MAX_LINKS` = 48), each from an input or hidden neuron to a hidden neuron or an output, with a weight. Inputs (9): a constant, hunger, light, crowding, cold, corpse here, own speed, and touch (eating, hurt; see below). Outputs (11): how hard to lean towards light, the living, the dead, comfort, soil matter, kin, bigger bodies, moving bodies; wander; inertia; throttle. Hidden neurons are a **CTRNN** (2026-11-12): each has a state, a time constant `tau` and a bias. It fires tanh(state + bias), and each tick all states move together, each 1/tau of the way towards what its links feed it (one Euler step a tick). A neuron with tau 1 and no bias is exactly the old instant neuron, so brains evolved before behave as they did. A slow neuron smooths what it feels and holds on to it for about tau ticks after (a memory of food, of danger, of the time of day). Neurons wired to themselves or to each other can also hold a state or keep a rhythm. With no hidden neurons it is a set of reflexes, which is where every brain starts.
- **Touch (2026-11-12):** two brain inputs every body feels, no sensor needed. `IN_EATING` is the energy it took in from the living and the dead last tick, 1 at twice its upkeep. `IN_HURT` is the energy drained out of it last tick, 1 at its upkeep. Both are read in phase 1 from what phase 2 booked the tick before (`Creatures::ate`, `hurt`), so they cost nothing and need no randomness. Before this a mover could not tell it was eating, so it could not learn to stop on its prey, and prey could not tell it was being eaten, so it could not learn to run. Like every input, these only change behaviour through evolved links (e.g. eating → throttle down, hurt → throttle up). `test_touch` checks that both are felt and that a prey wired "hurt → run" sits still until bitten.
- **Wander drifts (2026-11-11):** the wander output pushes towards each creature's own wander angle (`Creatures::wander`), which drifts **by distance travelled, not by time**: each tick it takes a normal step of sqrt(2 × stride / `WANDER_PATH`) radians, where stride is how far the body is about to go (`WANDER_PATH` 1.0 world unit, in `sim.cpp`). So a wanderer's path stays fairly straight for about a world unit whatever its speed. A newborn's angle comes from its id, so births draw nothing from the `Rng`.
  - **History of the "spinning in circles" (2026-11-11):** first the angle was a fresh random one every tick (jitter on the spot). Then it drifted by a fixed 0.5 rad every tick (`WANDER_TURN`), which looked fine for fast bodies but not for real movers: nearly all of them creep at 0.01 to 0.03 a tick, so they turned ~23° a tick while their loops were a tenth of a cell across, and the sprite rotated in place. Measured with a probe over 300 ticks: mean turn 23° a tick, path straightness 0.19. With turning by distance: 5 to 7° a tick, straightness 0.72 to 0.78.
  - **The spin was holding the world together.** With movers able to wander straight, slow sun-fed drifters (plants with a tiny motor) roamed onto fresh, unshaded, unfouled ground and took over: seed 7 fell to 800 creatures, 600 of them movers, 7 species. Lowering `FULL_STRIDE` from 0.02 to 0.005 (a creeper now loses most of its sunlight) and `WANDER_PATH` from 3 to 1 brought the world back (numbers in 10.7). `test_wander_turns_by_distance` checks that a slow wanderer does not spin.
  - **Mutation:** nudge weights; add a link; lose a link; grow a neuron in the middle of a link (the old link stays); lose a neuron with its links; nudge a neuron's tau (by a factor of e^(0.5 × normal), `P_TAU`, `TAU_STEP`) or its bias (`P_BIAS`). A new neuron gets a tau anywhere from 1 to `MAX_TAU` (1,000), evenly in log, so fast relays and slow memories are equally likely to appear. Rates are the `P_*` constants in `mutation.cpp`. The species distance counts tau (`W_TAU` 0.05 per e-fold) and bias (with the weights) for neurons both brains have.
  - **Cost (nearly free since 2026-11-12, Uday's call):** upkeep per hidden neuron (`NEURON_COST` 0.00012), per link (`LINK_COST` 0.00002), per unit of weight or bias (`BRAIN_COST` 0.0001), and per e-fold of each neuron's tau (`MEMORY_COST` 0.00002), all in `phenotype.cpp`. Each is a hundredth of what it was (0.012 / 0.002 / 0.01 / 0.002), so a full brain (8 neurons, 48 links, weights near 1) costs about 0.007 a tick, about 2% of a LUCA's upkeep. The costs are not zero, so unused wiring still drifts away slowly, and `MAX_HIDDEN` and `MAX_LINKS` cap the size. A brain on a body that cannot move costs nothing and does nothing.
  - **Layout:** `compile_brain()` turns the genome into a fixed-size `BrainPlan`; `think()` runs it. Plans and hidden state live in their own arrays in `Creatures`, off the hot path; only movers read them.
- **Danger / kin sense (2026-11-11):** three brain outputs, felt only with a sensor. Each body within two cells pulls or pushes the creature by `OUT_KIN` if it is the same species, `OUT_BIGGER` × how much bigger it is (negative when smaller), and `OUT_MOVING` × how fast it moved last tick; nearer and heavier bodies count for more. Nothing says which bodies are enemies: the weights start as noise and evolve. It walks the spatial grid, so the cost is bounded by local crowding. **This is the one rule that reads species ids**, so `SPECIES_THRESHOLD` and the distance weights now touch behaviour.
- **Moving is paid per distance as well as per speed (2026-11-11):** `movement_cost` is mass × (`TRANSPORT_COST` 2.0 × distance + `MOVE_COST` 3.0 × distance²) a tick, plus climbing. Before, only the squared term existed, so covering a distance at speed v cost mass × 3 × v × distance: creeping was nearly free and the world's movers were slow drifting plants. Now creeping a cell costs about two-thirds as much as walking it.
- **Senses are cheap (2026-11-11):** sensor upkeep is 0.05 per unit of size (it was 0.3, the dearest part there was; seeing one cell cost about 40% of a LUCA's upkeep, and nearly every mover was blind).
- **Optional traits (`seed_throw`, `bite`, `dormancy`, `repair`)** stay at exactly 0 until a rarer mutation switches them on (`P_NEW_TRAIT`), and are lost again below a sliver. A plain nudge bounced off zero and left every lineage paying upkeep for a little of each.
- **Sunbathing and moving conflict (2026-11-11):** a body's light demand is multiplied by `light_while_moving(moved)`: 1 if it sat still last tick, falling in proportion to 0.2 at a stride of 0.005 world units or more (`MOVING_SHADE` 0.8, `FULL_STRIDE`, in `energy.cpp`; 0.08 until wander began to drift, 0.02 until wander turned by distance, see 10.7). It applies before the canopy shares light out.
- **Folding (2026-11-12, Uday's call; current):** a body that moves more than `FOLD_STRIDE` (0.001) in a tick folds its sun patches shut, and they take `UNFOLD_TICKS` (600, a day) to open again, catching light in proportion (`Creatures::unfold`, `next_unfold` in `energy.hpp`; it multiplies light demand before the canopy shares light out). A creeper that moves every few ticks stays folded; a plant that moves once to get away loses part of a day's light. It replaced the half-sun rule below the same day: in six seeds it gave the fullest and most diverse worlds measured, with no creepers (10.7). `test_folding` checks it.
- **Movers catch half the sun (2026-11-12, replaced by folding the same day; `MOVER_SUN` now 1 = off):** a body that can move (a motor and a brain) has its `light_capture` halved, moving or not (`MOVER_SUN` 0.5, `phenotype.cpp`). Light-while-moving still cuts it further on the ticks it moves. The proportional rule alone could not stop part-time creepers: most moved on 5 to 50% of ticks and kept about two-thirds of their sunlight. A motor with no brain, which cannot move, is not affected.
- **Dormancy (2026-11-11):** each creature has a `sleep` level, 0 awake to 1. With the `dormancy` gene it climbs by 1/20 a tick, up to the gene's value, whenever its cell is dark or colder than its comfort range, and drops by 1/90 a tick otherwise (quick to sleep, slow to wake). At sleep `s` a body pays (1 − 0.9 s) of its upkeep and thermal cost, catches (1 − s) of its light and pulls (1 − s) as hard on the living and the dead. Above 0.1 it cannot move or breed; at 0.5 or more it ages at an eighth of the usual pace. Its membrane is unchanged, so it can be eaten in its sleep. Rules are `next_sleep` and `cost_while_asleep` in `energy.cpp`; `dormancy` is an optional trait like `bite`.
- **Poison (2026-11-11):** a new part type. A body's `toxicity` is its total poison size, capped at 2. Whatever drains it loses `toxicity` × the energy drained, as heat (`out_poison` in `TickStats`), paid out of what it holds, so a poisonous meal can cost more than it gives (a meal returns 70%). Poison is light (0.1 mass per unit) but dear to keep (0.12 upkeep per unit), and unlike armour does not slow the bite. It works on living bodies only: a poisonous body's corpse is safe to eat. Nothing can sense it yet, so eaters cannot learn to avoid it; they are only selected against.
- **Gene transfer (2026-11-11):** each tick that a body is draining another, there is a `gene_transfer_chance` (0.0001, in `config.hpp`; 0 = off) that it takes up the blueprint of one of that body's parts, picked at random, as a half-size bud (`transfer_part` in `mutation.cpp`, drawn from the sim `Rng` in contact order). "Touch" here means the existing contact rule, so only bodies with a mouth take anything up. The blueprint goes into the taker's **genome only**: its own body and stats do not change, and its children are built from the new plan and pay for the part. So a living body's genome can now hold parts its body lacks (the inspector's parts list reads the genome). `TickStats` counts transfers, cross-species ones and ones that bring a kind of part the taker lacked; the chronicle logs the first cross-species one.
- **Waste (2026-11-11; replaced by loose chemicals in chemistry stage 2, kept here as history):** `WASTE_SHARE` (20%) of every body's upkeep leaves it not as heat but as waste in its cell (`Soil::waste`), still holding that energy. Every body in the cell, its maker included, loses `WASTE_HARM` (0.0025) × waste × skin area a tick to it, as heat (`out_fouling`), asleep or not. Every 8 ticks each cell hands `WASTE_SPREAD` (20%) of its waste to its four neighbours and `WASTE_DECAY` (4%) of it breaks down to heat (`out_waste_decay`). A body with the `detox` gene (an optional trait like `bite`) takes up to 0.3 × detox a tick of its cell's waste, shared like corpses, and keeps its usual `efficiency` of it. Waste is energy only: the matter books do not change. "Stored" energy now includes waste. The census counts **cleaners** (over 60% of food from waste), the chronicle logs the first ten, and `fed_waste` is tracked per creature. Rules: `fouling_cost`, `waste_demand` (`energy.cpp`), `Simulation::spread_waste`. One look at the cell in phase 2 does all three things: take up, pay for fouling, excrete.
- **Wear:** upkeep is multiplied by `1 + wear × (age / AGE_SCALE)²` (`AGE_SCALE` 10000; `wear` 1 unless repaired). Nothing dies of old age as such; an old body costs more than it earns.
- **Repair and copying fidelity (2026-11-12):** the `repair` gene (an optional trait like `bite`) makes the wear clock run up to three times slower (`Phenotype::wear` = 1 / (1 + `REPAIR_GAIN` × repair)²) and multiplies upkeep by 1 + `REPAIR_SHARE` (0.15) × repair. The `mutability` gene scales every chance in `mutate()` (and in the brain's mutation) by 2^mutability; step sizes are unchanged. Copying better than the usual rates costs `FIDELITY_UPKEEP` (0.02) per halving; copying worse is free, and its only price is mutation load. Both are in the species distance (`W_MUTABILITY` 0.3 per doubling, `W_REPAIR` 1.5), the metrics (`mean_mutability`, `mean_repair`) and the creature snapshot.
- **Observation only:** `fed_light / fed_living / fed_dead` per creature and `Simulation::census()` count ways of life from real energy flows. Sim logic never reads them.
- **Species record book (2026-11-12, observation only):** each creature counts its kills and children (`Creatures::kills`, `children`). A kill goes to the body that drained the most from the victim in the tick it died (`top_taker_` in `sim.cpp`). At each death the registry keeps per-species tallies (born in all, died eaten or otherwise, kills, a kill web of killer and victim species) and records (eldest, most kills, most young, with the holder's id and tick). `take_habits()` runs with the census and stores each species' means (diet shares including vents, movers, sighted, sleepers, armoured, poisonous, size, age, speed, light, temperature, height, where it lives and how spread out). Sim logic never reads any of it, and it draws nothing from the `Rng`: a 20,000-tick run is identical with and without it. Individuals get syllable names from their id (`creature_name`).
- **Species** (`core/species.*`, section 9.3 steps 1 and 2) are labels the game works out; only the kin sense reads them. Every creature carries a species id. A newborn keeps its parent's species unless its genome is further than `SPECIES_THRESHOLD` (1.0) from that species' founder; then it joins a living daughter species that is close enough, or founds a new one. The distance is Euclidean over weighted gene differences (a plain sum made every small drift a species: 164 at once instead of ~27). Shape genes are left out. No randomness is used.
- **Species splits (2026-11-12, 9.3 step 3):** every `SPLIT_INTERVAL` (2,000) ticks, `Simulation::check_splits` looks inside every established species with at least 2 × `SPLIT_MIN` (30) alive. It takes a sample of up to `SPLIT_SAMPLE` (48) members, evenly spaced over the creature arrays, and cuts it in two (two centres that are themselves sample members, refined for 3 rounds). It is two species when each side holds at least `SPLIT_SHARE` (25%) of the sample, the centres are at least `SPLIT_GAP` (0.8) apart, and that gap is at least `SPLIT_SEPARATION` (3) times the mean spread. Then the side whose centre is further from the founder becomes a new daughter species (`Species::by_split`), with that centre as its representative. Every member goes to the nearer centre, and both sides must keep `SPLIT_MIN` (15). The chronicle says where each half lives (*"Kibuis splits in two: Mexal in the south-west, Kibuis in the south. Mexal is darker."*), or that they split in the same place. Only labels change, so no energy or matter moves. The kin sense sees the new labels. Deterministic, no `Rng`; constants in `sim.cpp`.
- **Following a species (2026-11-12, 4.8):** `Simulation::follow(species)`; observation only. What happens to a followed species goes to `Simulation::alerts()`, a second `Chronicle`, not the main one. Every `WATCH_INTERVAL` (200) ticks it is surveyed:
  - **Part types and moving:** most of them have it once `TRAIT_ON` (60%) carry it, and have lost it at `TRAIT_OFF` (40%) or less. *"Most of your Tipal now have eyes."*, *"Your Tipal have lost their mouths."*, *"... have begun to move."*
  - **Size:** mean adult mass 20% (`SIZE_ALERT`) above or below the last report.
  - **Numbers:** halved, or doubled; size and doubling need `WATCH_MIN` (20) alive.
  - **Events, as they happen:** extinction, a daughter species becoming established, and a split.
- Splits and following use no randomness and change no creature.
- **Determinism:** same seed + build + machine → identical runs (verified). Cross-machine determinism is not a goal yet; creature files don't need it.

### 10.5 God powers (built 2026-11-11)

- **Six powers so far:** `terrain`, `heat` and `vent` (soft round brushes), `sun` and `climate` (global), and `seed` (drop creatures from a creature file by meteor; it adds bodies and never touches existing ones, and is logged and replayable like the rest). A vent line in a script is `<tick> vent <x> <y> <radius> <amount> [duration]`. Script format is documented in `core/powers.hpp`.
- **The world acts on its own (2026-11-12, 4.7, `core/events.*`):** `WorldEvents` runs at the start of every tick, before the powers. At `event_rate` 1 (`config.hpp`, default 0; `--events`), one draw a tick decides whether an event starts. In 100,000 ticks that gives about 12 storms, 2 eruptions, 2 meteors and 4 climate drifts (`PER_100K`), one every 5,000 ticks. Each event is a handful of ordinary power commands, marked `quiet`, so the chronicle gets one line from the event instead of one per command, plus a closing line when it ends. Where they land is uniform over the map.
  - **Storm:** a cold spot 6 to 14 °C deep and 10 to 22 units across. It rolls in over 200 ticks, sits for 600 to 2,400 and rolls out over 400. The ground ends exactly as it was.
  - **Eruption:** a cone 8 to 18 high and 4 to 7 across rises over 300 ticks and stays. With it come 15 °C of heat and a strength-2 vent in the crater, which fade together over 20,000 to 40,000 ticks. A cone that steep blocks births and walkers like a raised mountain. Eruptions release no matter: the matter books have no way in from outside.
  - **Meteor:** a crater 2.5 to 6 across and 2.5 × its radius deep, with a rim, and a 30 °C flash that cools over 3,000 ticks. One in five (`GREAT_METEOR_CHANCE`) also dims the sun by 0.25 over 200 ticks, clearing over 15,000 to 40,000: an impact winter.
  - **Climate drift:** the whole world warms or cools by a normal step (sd 1.5 °C, at most 3) over 10,000 to 30,000 ticks. Each step also undoes 30% of the offset so far, so the climate wanders but comes back.
  - **Dice:** they come from the events' own `Rng` (seeded from the world seed), not the sim's. So rate 0 changes nothing, and `powers.txt` from a run with events replays it exactly with `--events 0` (tested; with events on as well they would happen twice). `Simulation::trigger_event` starts one of a kind now, for tests.
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
- **Inspector** says whether a body can live on what vents make (it burns C), and writes two-input enzymes as `A+A<->D+D` (neutral ones `<=>`). It lists each part with its size, where it sits (front, left, back-right, ...) and its stalk, and says whether the body is round, oval, long or boxy. Each hidden neuron gets a line with its tau and how it is firing right now (a bar, + or −), updated as the creature moves.
- **Day and night** tint the whole picture. When days pass faster than about one a second, the tint holds a steady dusk instead of strobing.
- **Stats panel:** day and clock, population, the census (sun-fed, hunters, scavengers, cleaners, vent-fed, mixed diet, able to move), and the six most numerous species with their colours.
- **Colour by species** (checkbox or `C`, `--species` on the command line): skin is painted in the species colour instead of the pigment. The hue rides in the unused height slot of each instance's origin.
- **Overlays:** natural, temperature, light, nutrients, the dead, waste, vents. In the natural view, heavily fouled ground turns murky olive, and a vent darkens the ground to basalt with orange and yellow cracks that grow denser with strength and glow through the night. Vent strength rides in the alpha channel of the soil texture. `--vent=x,z[,strength]` opens a vent before `--warmup`, for screenshots.
- **Tree of life** (`T`, or the button; `viewer/tree_view.gd`; rebuilt 2026-11-12, the pixel tree with crowns was unreadable past a few dozen species): a scrollable cladogram over nearly the whole window. One row per species that ever had 10 alive; time runs left to right, a line runs from the tick a species branched off to now or to its extinction, and a stem drops from the parent's line. Children are listed newest first under their parent, so no stem crosses another line. A right-hand column gives each name and how many are alive (or when it died). Hover a row for its story in the footer; click to select it (its line of descent lights up, the rest dims); double-click or Enter to open its branch on its own, with breadcrumbs back; click the triangle to fold a clade; wheel, trackpad or drag to scroll; arrows move and fold; Backspace or right-click steps out; `E` hides dead ends; Esc closes. It opens on the followed creature's species. Data comes from `LucaWorld.get_species_tree()`. `--tree` or `--tree=Name` opens it at start.
- **Growing up:** a sprite's radius is the radius as grown so far, so the young are drawn small (its parts keep their adult size: pull and armour don't scale). The inspector says how far it has grown and what size its young are born at.
- **Following a species:** the inspector's Follow button (or `F`) follows the picked creature's species. Followed species are listed under the top six in the stats panel, with "extinct" once gone. Their alerts appear in the chronicle in gold (`ALERT_COLOR`). `--inspect=x,z --follow` follows from the start, for screenshots.
- **Species card** (`viewer/species_card.gd`, 2026-11-12): beside the tree, it shows the selected species. At the top is a living portrait: a few real members drawn by the same blob shader in a small SubViewport over ground tinted by their habitat. Movers walk along their real heading. An extinct species is drawn from its founder's blueprint in stone (`stone` uniform in `blob.gdshader`). Below that:
  - a niche, a classification worked out from diet, parts and kill rate: Flytrap, Canopy frond, Roaming hunter, Ambush predator, Scavenger, Waste recycler, Vent dweller, Mixotroph, Parasite and so on;
  - tags such as Armoured, Sleeps at night, Cold-hardy, Many small young and Cannibal, judged against where life actually lives;
  - a written natural history;
  - a diet bar (sun, the living, the dead, waste, vents);
  - the body of its eldest living member, part by part;
  - the record book (eldest ever, most kills, most young, with holders' names);
  - a food web of who it kills and who kills it (click a name to jump to that species);
  - its range.
  `P` or the inspector's "Its species" button opens the card on the inspected creature's species, and species in the stats panel are clickable. Data comes from `LucaWorld.get_species_profile(id)` and `get_sprites(ids, species)`. The rules for niches and tags are in `_niche` and `_tags`; they are first guesses to tune.
- **Inspector** follows the picked creature by id as it moves, and shows its species, the species it branched from, and its diet as percentages of sun / the living / the dead.
- **Sandbox brushes:** raise/lower land, warm/cold spot, open/close vent (a gentler brush: about half a unit of strength a second), sun and climate buttons, an instant/gradual toggle. A stroke is many small instant commands; only the first is written to the chronicle (`quiet` flag).
- **World acts on its own:** a slider under WHOLE WORLD sets the event rate, 0 to 4. The viewer starts at 1 (`--events=F` on the command line; the parade is always 0). Events show in the chronicle like the player's powers.
- **Painting works while paused:** `Simulation::flush_powers()` applies due powers immediately, with the same result as waiting for the next tick.
- **Time:** pause, 1x, 4x, 16x, max. The sim gets a 10 ms budget per frame; past that the world runs slower (7.1).
- **Not done yet:** position interpolation between ticks, the food web view, sim on its own thread, a pixel font for the UI.
- **Creature designer and seed vault (2026-11-12, 4.10):** `B` (or the LIFE buttons) opens the designer over everything and pauses the world; closing it restores the speed. `V` opens the seed vault, a folder of creature files at `user://creatures` (on macOS `~/Library/Application Support/Godot/app_userdata/Ecosys/creatures`). The inspector has "Edit in designer" (the creature's own genome) and "Bank its species"; the species card has "Bank in the seed vault" (a sample of up to 20 living members, `bank_species`). Release (designer or vault) hands the file back to the world: the pointer becomes an orange ring, a click drops them by meteor, Esc cancels. Undo/redo (Cmd+Z), Cmd+S saves, Delete removes what is picked, V / P / M / 1 to 8 pick tools on the Body tab. Data comes from `LucaWorld.assess_design`, `clamp_design`, `design_sprite`, `probe_brain`, `write_creature_file`, `read_creature_file`, `bank_species`, `genome_of`, `release` and the `trial_*` calls (`godot/luca_design.cpp`). Command line: `--designer[=TAB]`, `--template=NAME`, `--vault`, `--bank` (banks the most numerous species at the start).
- **Checking the viewer:** `./view.sh --warmup=40000 --zoom=8 --at=100,24 --frames=40 --shot=/tmp/x.png` skips ahead 40,000 ticks, zooms in, saves a screenshot and quits. Keep `--frames` small: macOS throttles a window that is behind other windows to a few frames a second, so a long run looks like a hang.

### 10.7 Baseline numbers (default settings, 128×128, seed 42, measured 2026-11-11)

**After chemistry stages 3 and 4 (2026-11-12; six seeds 42 / 7 / 99 / 1 / 2 / 3, 100,000 ticks, mean of the last fifth; with vents means four strength-2 vents at tick 0, `0 vent x y 5 2` at (32, 112), (96, 112), (64, 72), (64, 24)):**
- **Stage 3 (vents as conversions, no `chemo` or `digestion`), no vents:** 3,190 creatures and 67 established species (2,480 and 57 in stage 2); hunters 86 (61), scavengers 16 (11), mixed feeders 324 (220), movers 31 (15), cleaners 3. Per seed 3,210 / 3,310 / 2,460 / 3,410 / 3,510 / 3,220; lowest after the first tenth 1,770 / 1,560 / 880 / 1,470 / 1,760 / 1,610. Probably fuller because the two genes no longer hitchhike (both were carried at low levels by many bodies, for upkeep and nothing).
- **Stage 3 with vents, when they used their cell's A (since dropped):** 2,870 creatures, 58 species, 88 hunters, **no chemotrophs in any seed** (with the `chemo` gene there were some in every seed). At tick 100,000 only 1 to 17 bodies stand within 5 cells of any vent, nearly all plain D-burners; none lives mostly on vent food. Vents made about 5.5 energy a tick in all, against about 320 if their ground had A.
- **Why (probe, seed 42, stage 4 build):** soil A within 4.5 cells of each vent falls from 1.3 to 5.8 at tick 0 to under 0.6 by tick 20,000 and to 0 by 60,000 (the vent at (64, 24) keeps 0.001). The C drifts outward and breaks down there: the ring 5 to 9 cells out gains A (5.4 to 6.9 around the far-edge vent). No body ever stands on a vent in that run. 162 to 568 bodies a world burn C, but they make it from light (A ↔ C charges as well as burns) and none is on a vent before it runs dry.
- **Stage 4 (two-input reactions), no vents:** 2,570 creatures, 60 species; hunters 56, scavengers 10, mixed 215, movers 24, cleaners 2. Per seed 3,880 / 1,630 / 3,340 / 1,390 / 3,020 / 2,150; lowest after the first tenth 1,720 / 1,210 / 1,470 / 870 / 950 / 1,530. Below stage 3 by a fifth, mostly in seeds 7 and 1; one sweep, and the spread between seeds is as wide as the difference.
- **Two-input enzymes are carried and run:** 58 to 735 bodies a world carry one (mean 300); they turn over about 1 unit a tick in all (`two_flow`). Most common: A + A ↔ A + D (LUCA's own reaction with a pair added, in every world, up to 720 carriers), A + A ↔ C + D (510 in seed 99), A + D ↔ C + D, A + A ↔ D + D (twice the energy per unit of rate as A ↔ D; 16 to 39 a world).
- **Neutral exchanges are not used:** under one body a world carries a running one, and `neutral_flow` is about 0.
- **Stage 4 with vents, still using their cell's A:** 2,680 creatures, 67 species, 69 hunters, still no chemotrophs; 3 to 14 bodies near vents at the end.
- **Vents bringing up matter (the rule now; same four vents, same six seeds):** 2,960 creatures, 65 species, 85 hunters, 15 scavengers, 314 mixed feeders, 21 movers. Per seed 3,600 / 3,400 / 3,340 / 2,720 / 2,270 / 2,410; lowest after the first tenth 1,710 / 1,760 / 1,770 / 1,130 / 710 / 1,470. **Vent dwellers in two seeds:** the chronicle's "Life at the vents" at tick 49,100 (seed 2) and 67,550 (seed 3). At tick 100,000 seed 2 has 99 bodies near vents, 98 of which burn C (49 burn B, C and D), and 22 living mostly on vent food; seed 3 has 27, all burning B, C and D. In the other four, 14 to 25 bodies near vents, nearly all plain D-burners. Vents bring up 187 energy a tick (1.26 units of C), 18,700 a row; loose C stands at about 0.85 a unit at a vent's centre and 245 units world-wide. Fouling rises to 425 a row (80 without vents). Matter: 98,900 at the start, 225,000 at tick 100,000; soil A under each vent about 90 a cell at tick 20,000, 250 at 60,000, 400 to 450 at 100,000. Worst audit error 1.9e-7.
- **Cross-feeding still not seen:** loose B and C are 1 to 4 units a world without vents.
- **Books:** worst audit error 2.1e-7 in every run. `--ticks 5000`: 838 creatures, 0.048 ms/tick, audit error ≈ 1.4e-7 (stage 3 alone: 867).

**After chemistry stage 2: loose chemicals (2026-11-12; six seeds, 100,000 ticks, mean of the last fifth, against stage 1):**
- **Population and species:** 2,480 creatures (3,230) and 57 established species (71).
- **Ways of life:** hunters 61 (39), scavengers 11 (17), mixed feeders 220 (115), movers 15 (19), cleaners 2 (18).
- **Per seed (42 / 7 / 99 / 1 / 2 / 3):** 2,840 / 1,440 / 1,590 / 3,450 / 3,400 / 2,140 creatures; 62 / 38 / 35 / 77 / 75 / 53 species; hunters 73 / 31 / 33 / 96 / 77 / 58; lowest after the first tenth 1,740 / 1,270 / 610 / 690 / 1,690 / 1,130. No seed crashed; seed 99's dip is shallower (610 against 430), seed 1's deeper (690 against 1,650).
- **Where the food went:** the 30% a meal used to lose as heat now lands in the cell, and every D-burner around (the eater included) takes it up. Uptake is about what detox used to eat (7,560 against 8,060 a row), but it now goes to plants and hunters alike, so hunting pays more: hunters and mixed feeders rose, plants and population fell. Fouling is nearly gone (33 a row against 9,600): almost every loose chemical is D, which nearly everything burns.
- **Cross-feeding not seen yet:** loose B and C are a fraction of a unit in each world (loose D 4 to 12 units). Bodies carrying A ↔ B number 18 to 191 a world. Nobody has measured who lives on whose leavings (see "Measuring milestone 5" in 2.1.1).
- **Books:** worst audit error 1.9e-7; matter drifts by 1.6e-5 over 100,000 ticks (float rounding; 5e-6 in stage 1).
- `--ticks 5000`: 807 creatures, 0.048 ms/tick, audit error ≈ 1.4e-7.

**After chemistry stage 1 (2026-11-12; six seeds, 100,000 ticks, mean of the last fifth, against the folding + cheap brains + touch build):**
- **Population and species:** 3,230 creatures (3,470) and 71 established species (53).
- **Ways of life:** hunters 39 (70), scavengers 17 (11), mixed feeders 115 (105), movers 19 (75), cleaners 18.
- **Per seed (42 / 7 / 99 / 1 / 2 / 3):** 3,540 / 3,910 / 1,840 / 3,380 / 3,620 / 3,100 creatures; 85 / 85 / 28 / 81 / 85 / 64 species; lowest after the first tenth 1,440 / 1,650 / 430 / 1,650 / 1,460 / 1,300. Seed 99 nearly crashed.
- **Chemistry:** 1.5 enzymes a body. Nearly every body carries A ↔ D; 115 to 180 bodies carry each other reaction. Stores and bodies are almost all D (B and C a fraction of a percent): nothing uses B or C yet.
- **Books:** worst audit error 1.7e-7; matter drifts by 5e-6 (float rounding over 100,000 ticks).
- **Why movers and hunters fell:** probably the power cap. A mover's step must be paid for out of what its enzymes turn over this tick.
- **`nutrient_mean`:** at the old 0.8 the soil can't fill the stores (too thin). At 6: 776 creatures at 5,000 ticks (800 before chemistry) and 3,478 at 30,000 (3,109).
- `--ticks 5000`: 776 creatures, 0.040 ms/tick, audit error ≈ 1.4e-7.

**After touch (eating / hurt inputs, 2026-11-12; six seeds, 100,000 ticks, mean of the last fifth, against the CTRNN build):**
- **Population and species:** 4,090 creatures (3,730) and 52 established species (50).
- **Ways of life:** hunters 47 (46), mixed feeders 129 (97), movers 34 (40).
- **Per seed:** 4,700 / 4,450 / 2,630 / 4,300 / 4,930 / 3,510 creatures; no seed fell below 2,600.
- **Hidden neurons:** 11 in running brains on average (3 before), 7 of them slow.
- **Brains wire to touch:** at tick 100,000 the probe finds that most brains in the bigger mover populations carry a link from eating (104 of 166 in seed 42, 43 of 48 in seed 99) and many from hurt (53 of 166; 30 of 36 in seed 7). Seed 42 holds 88 hidden neurons, 68 of them slow.
- **Behaviour is not clean yet:** no brain links eating straight to slowing down, and movers go as fast or faster the tick after a meal, so "stop on your prey" has not evolved. The tick after being drained, movers go 7 to 18% faster in three seeds (the same in the others), which may be crowding rather than fleeing.
- **Movers are animals, not fleeing plants:** of 269 bodies able to move across the six worlds, 126 live mostly on the living, 103 are mixed, 12 on the dead and 2 on the sun.
- `--ticks 5000`: 814 creatures, 0.037 ms/tick, audit error ≈ 1e-7; worst audit error in the six-seed runs 4e-7.

**Grazeable leaves, tried and shelved (2026-11-12; six seeds, 100,000 ticks; patch in `luca/shelved/grazing.patch`):** sun patches were leaves a mouth could bite off (a `graze` gene split its pull between leaves and store), the plant caught light in proportion to what was left and regrew them from its store and soil. No seed in any variant evolved a mover living mostly on leaves: regrowth over 3 days (4,050 creatures, 35 hunters), over 1 day (4,370, 38), leaves open to small and armoured-against grazers (4,180, 48), leaves worth 3x and 5x the body's energy per mass (3,380 / 3,550 creatures, 39 / 33 hunters). The `graze` gene drifted at about 0.05; a few hundred sessile bodies used it to bite their neighbours' leaves. Draining a plant's store (up to 650) always beat biting its leaf (22 for a LUCA, 89 at 5x). Shelved in favour of the chemistry layer (2.1.1).

**After folding (600) and nearly free brains (2026-11-12; six seeds, 100,000 ticks, mean of the last fifth, against folding with the old brain costs):**
- **Population and species:** 3,470 creatures (4,660) and 53 established species (54).
- **Ways of life:** hunters 70 (53), scavengers 11 (23), mixed feeders 105 (136), movers 75 (32).
- **Per seed:** 4,080 / 5,060 / 2,580 / 3,000 / 2,880 / 3,210 creatures.
- **Brains:** running brains 75 and hidden neurons 20 (12 slow), against 32 and 1.
- **Movers are all animals:** of 446 across six worlds at tick 100,000, 286 live mostly on the living, 137 mixed, 23 on the dead, none on the sun.
- **"Stop on your prey" has evolved:** in seed 42, 57 of 83 brains link eating to a slower throttle, and movers go at 0.17 of their top speed the tick after a meal against 0.48 otherwise. In seed 3 it is 17 of 49 brains, 0.34 against 0.43. In seeds 7 and 42, 20 and 15 brains link hurt to a faster throttle.
- **More hunting thins the world:** cheaper thinking made hunters better, and the world is about a quarter emptier.
- Worst audit error 4e-7. `--ticks 5000`: 800 creatures, 0.042 ms/tick, audit error ≈ 1e-7.

**Folding tried in place of `MOVER_SUN` (same six seeds, with touch; adopted at 600 ticks, see the next section):** a body that moves more than `FOLD_STRIDE` (0.001) in a tick folds its sun patches, and they reopen over `UNFOLD_TICKS` (`energy.hpp`). The idea: a plant that moves only to flee should lose a little, and a creeper that moves every few ticks should stay folded. `MOVER_SUN` was set to 1 for these runs.
- **`UNFOLD_TICKS` 200:** 3,230 creatures, 38 species, 36 hunters, 31 movers; two seeds fell under 2,000.
- **`UNFOLD_TICKS` 600 (a day):** 4,660 creatures, 54 species, 53 hunters, 23 scavengers, 136 mixed feeders, 32 movers; per seed 5,000 / 5,480 / 4,510 / 4,270 / 4,970 / 3,720, so no seed crashed. This is the fullest and most diverse world measured. Its movers are hunters (162 of 270 living mostly on the living) and faster (median top speed 0.048 against 0.032).
- **Neither variant brings back creepers, and neither brings fleeing plants:** 9 and 1 sun-fed movers across six worlds.
- **Adopted (Uday's call):** `UNFOLD_TICKS` 600 and `MOVER_SUN` 1.

**After the CTRNN brain (2026-11-12; six seeds 42 / 7 / 99 / 1 / 2 / 3, 100,000 ticks, events off, mean of the last fifth, against the build before it):**
- **Population and species:** 3,730 creatures (3,800 before) and 50 established species (47).
- **Ways of life:** hunters 46 (36), mixed feeders 97 (109), movers 40 (15).
- **Per seed:** 5,040 / 4,940 / 4,450 / 2,780 / 2,510 / 2,660 creatures; movers 6 / 35 / 6 / 121 / 61 / 8. Seed 42 is identical to before because no neuron ever appeared there.
- **Hidden neurons are still rare, because brains are:** 1 to 45 running brains in most worlds at most times. They appear only where movers boom: seed 1 at tick 60,000 had 156 brains carrying 39 neurons, and seed 2 at tick 80,000 had 173 brains carrying 28. At tick 100,000 four bodies in all six worlds have one (none has two).
- **When neurons do appear, most are slow:** 33 of 39 (seed 1) and 24 of 28 (seed 2) have tau of 30 ticks or more, geometric mean 52 and 243 ticks. New neurons draw tau evenly in log from 1 to 1,000, so the share above 30 at birth is about half; selection keeps the slow ones. Whether they are used for anything (remembering prey, timing the day) has not been measured.
- **Inspector check:** the viewer reproduces the headless world tick for tick (`./view.sh --seed=2 --pop=200 --events=0 --warmup=100000 --inspect=74.14,24.35`). It shows a moving hunter (73% of its food from the living) with one neuron of tau 37 firing near its peak.
- `--ticks 5000`: 814 creatures, 0.038 ms/tick, audit error ≈ 1e-7 (unchanged; no neurons appear that early).

**World events at rate 1 (2026-11-12; six seeds 42 / 7 / 99 / 1 / 2 / 3, 100,000 ticks, mean of the last fifth, against the same build with events off):**
- **Population and species:** 3,520 creatures (3,800 off) and 47 established species (47).
- **Ways of life:** hunters 57 (36), scavengers 11 (14), mixed feeders 101 (109), movers 42 (15).
- **Per seed:** 4,940 / 3,490 / 2,880 / 5,020 / 2,430 / 2,370 creatures and 58 / 45 / 39 / 69 / 36 / 34 species. With events off, every seed reaches the same tick-by-tick world, so these differences are the events' doing, but the spread between seeds is as wide as ever.
- **What happened:** 6 to 22 storms per world, 0 to 2 eruptions, 0 to 5 meteors (two great ones, both in seed 1, which ended with the most species), 1 to 6 climate drifts.
- **Die-offs:** seeds 2 and 3 had a great dying (seed 3 fell to 464 at its lowest). Neither follows an event closely; both come with a boom of movers, like the moving-hunter crashes without events.
- **Vents:** no eruption vent was colonised: no chemotrophs in any seed. A vent lasts 20,000 to 40,000 ticks, and `chemo` has to evolve first.
- `--ticks 5000` with events off is unchanged: 814 creatures, audit error ≈ 1e-7.

**Species splits (2026-11-12; same six seeds, 100,000 ticks, `MOVER_SUN` build):** 4 / 3 / 2 / 1 / 1 / 1 splits (seeds 42 / 7 / 99 / 1 / 2 / 3), all after tick 50,000, against 59 to 100 species established by newborns. Most split a species between two neighbouring regions; a few in one place, along a way of life (*"Thomer splits in two in the south-east: Thipua goes its own way. Thipua has lost its eyes."*). Population and audit are unchanged and the check costs nothing measurable; `--ticks 5000` gives 814 creatures, 0.034 ms/tick, audit error ≈ 1e-7.

**After movers catch half the sun (`MOVER_SUN` 0.5, 2026-11-12; since replaced by folding):**
- `--ticks 5000`: 814 creatures, 0.039 ms/tick, audit error ≈ 1e-7.
- Six seeds (42 / 7 / 99 / 1 / 2 / 3), 100,000 ticks, mean of the last fifth: 3,800 creatures (3,360 under growing up alone), 46 established species (53), 36 hunters (50), 14 scavengers (11), 108 mixed feeders (128), **15 movers (300)**; movers by seed 7 / 40 / 7 / 6 / 20 / 10, so **no seed is taken over by creepers**. Per seed 5,040 / 5,390 / 4,460 / 4,040 / 2,330 / 1,560 creatures, 52 / 72 / 67 / 31 / 30 / 24 species.
- **Moving hunters boom and bust.** In seeds 1, 2 and 3 movers that hunt rise to 130 to 210 between ticks 35,000 and 70,000 with hunters past 100, the world falls to about 1,500 to 1,700, and then the movers die out (0 to 13 left) and the plants recover. The first real predator-prey cycle seen; nothing yet keeps the movers from overgrazing themselves out. The low species counts in those three seeds are the crash.
- **The literal reading was tried first:** half the sun on any tick a body moved at all, combined with the existing cut (same seeds): 3,670 creatures, 54 species, 203 movers, 801 of them in seed 42 and 255 in seed 7. Creepers that sit still most of the time escape it.

**After growing up (2026-11-12; LUCA's `birth_size` 0.5):**
- `--ticks 5000`: 830 creatures (721 before), 0.037 ms/tick, audit error ≈ 1.4e-7; `matter_total` constant to the last digit.
- Six seeds (42 / 7 / 99 / 1 / 2 / 3), 100,000 ticks, mean of the last fifth, against the vent build without vents: 3,360 creatures (3,850), 53 established species (70), 50 hunters (29), 11 scavengers (8), 128 mixed feeders (102), 300 movers (43); 82 moving hunters at tick 100,000 across the six (45). Audit error under 2e-7.
- **Being born small pays:** mean `birth_size` falls steadily from 0.50 to 0.43 (0.39 to 0.46 by seed) and is still falling. With LUCA started at full size (`birth_size` 1.0) it falls too, from 1.0 to 0.86, and that world is otherwise the old one (3,810 creatures, 68 species, 28 hunters, 42 movers); 8% of bodies are growing at any time there, against 25% at 0.5.
- **The young feed hunters:** hunters nearly double, as the size rule predicts.
- **Sun-fed creepers took over two seeds of six** (seeds 42 and 1: 930 and 810 movers, 84 to 86% of their food from the sun, top speed 0.01 to 0.02; those worlds hold 2,450 creatures and 51 and 47 species), as drifting plants did before moving cost sunlight. In seed 42 the creepers rise from tick 20,000, in seed 1 from 50,000.
- **Why (probed 2026-11-12, seeds 42 / 1 / 7 / 3 at tick 100,000): the young starve in the shade.** A young body is shorter than every adult, and the canopy gives a cell's light to the tallest first. 70 to 80% of young sessile bodies share their cell with a taller one; they catch about 0.33 light a tick against 0.38 upkeep, and half die within a day. Young movers are under a taller body only 11 to 27% of the time and catch about 0.6. (Before, a child was as tall as its parent, give or take a mutation, so it overtopped its parent about half the time.) Young that survive past 1,000 ticks are mostly stuck on bare soil (56 to 80%): their parent built its children out of the same cell.
- **Checked by two variants** (same six seeds): young that stand at their adult height in the canopy (a counterfactual) end the takeover: 4,390 creatures, 62 species, 21 hunters, 46 movers (at most 102 in a seed), and birth size falls further, to 0.32. Leaves passing 30% of the light that reaches them down to the layer below (crowded cells only) does not: 4,090 creatures, 51 species, 358 movers, 1,560 in seed 42.

**After vents (2026-11-12):**
- `--ticks 5000`: 721 creatures, 0.032 ms/tick, audit error ≈ 1.1e-7.
- **No vents painted** (six seeds 42 / 7 / 99 / 1 / 2 / 3, 100,000 ticks, mean of the last fifth): 3,850 creatures, 70 established species, 29 hunters, 102 mixed feeders, 43 movers; the same as the windfall row below, within the spread between seeds. The `chemo` gene still appears: 480 to 2,330 bodies carry a little (mean 0.05 to 0.14), paying up to 3% more upkeep for nothing, as `detox` once did.
- **Four vents at tick 0** (`0 vent x y 5 2` at (32, 112), (96, 112), (64, 72), (64, 24); same seeds): 3,880 creatures, 73 species, 21 hunters, 111 mixed feeders; **chemotrophs in every seed**, 2 / 21 / 213 / 5 / 125 / 12 (mean 63), the first between ticks 5,800 and 47,200, and the chronicle's "Life at the vents" (10 at once) in five seeds of six. Bodies on a vent carry far more `chemo` than the rest (0.94 near the mid-map vent in seed 99, against 0.14 world-wide). Vents give 2,300 a row against 206,000 of all energy in, about 1%. Audit error under 2e-7.
- **The two vents at the cold far edge stayed empty in every seed.** Life stops at about y = 70 to 87 and never crossed the dead ground in between. Moved to y = 96, just beyond the frontier: still empty in all six. The mid-map vent and the one in the bright band were colonised in four and six seeds. In that run chemotrophs numbered 4 to 324 (mean 129) and species 84; the difference from the four-vent run is the two vents everyone shares, i.e. seed-to-seed spread.

**After windfalls (2026-11-12; measured on a build that also held uncommitted work on mutability and repair genes, so compare the rows below with each other, not with older sections):**
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
- Brain: `NEURON_COST`, `LINK_COST`, `BRAIN_COST`, `MEMORY_COST` (`phenotype.cpp`); `MAX_HIDDEN`, `MAX_LINKS`, `limits::MAX_TAU` (`genome.hpp`); `P_TAU`, `TAU_STEP`, `P_BIAS` (`mutation.cpp`); `W_TAU` (`species.cpp`).
- `phenotype.cpp`: `BITE_UPKEEP`; `sim.cpp`: `SOCIAL_CELLS`, `SOCIAL_FAST` (the kin sense), `WANDER_PATH` (how far a wanderer goes before its direction is lost).
- Cold nights: `temp_day_swing` (`config.hpp`), `HEAT_LAG` (`phenotype.cpp`).
- Loose chemicals (stage 2): `UPTAKE_RATE`, `WASTE_SPREAD`, `WASTE_DECAY` (`energy.hpp`); `FOUL_HARM` (`energy.cpp`). `WASTE_SHARE`, `WASTE_HARM`, `WASTE_EAT_RATE` and `DETOX_UPKEEP` are gone with the `detox` gene.
- Windfalls: `windfall_rate`, `windfall_size` (`config.hpp`). Bigger, rarer lumps make finding one matter more; a higher rate feeds the world more.
- Splits and following (`sim.cpp`): `SPLIT_INTERVAL`, `SPLIT_MIN`, `SPLIT_SAMPLE`, `SPLIT_SHARE`, `SPLIT_GAP`, `SPLIT_SEPARATION`; `WATCH_INTERVAL`, `TRAIT_ON`, `TRAIT_OFF`, `SIZE_ALERT`, `WATCH_MIN`. A lower `SPLIT_GAP` or `SPLIT_SEPARATION` gives more splits.
- World events (`events.cpp`): `PER_100K` (how often each kind happens at rate 1), the size ranges of each kind (`STORM_*`, `ERUPT_*`, `METEOR_*`, `WINTER_*`, `DRIFT_*`), `GREAT_METEOR_CHANCE`; `event_rate` (`config.hpp`) and the viewer's starting rate (`event_rate` in `main.gd`).
- Chemistry: `LEVEL` (`chem.hpp`); `ENZYME_UPKEEP` (`phenotype.cpp`); LUCA's enzyme rate 0.015 (`genome.cpp`); `MAX_ENZYMES`, `MIN_ENZYME_RATE`, `MAX_ENZYME_RATE` (`genome.hpp`); `P_ENZ_ADD`, `P_ENZ_DELETE`, `P_ENZ_COPY`, `P_ENZ_SWAP`, `P_ENZ_PAIR` (`mutation.cpp`); `W_ENZ_KIND`, `W_ENZ_RATE` (`species.cpp`).
- `config.hpp`: `day_length`, `nutrient_mean` (6 since chemistry stage 1; it was 0.8), `nutrient_patchiness`, `gradient_min`, `birth_max_step`, `slope_shade`, `wash_rate`, `gene_transfer_chance`.
- `world.cpp`: `FACING_LOSS`, `VALLEY_LOSS`, `HORIZON`, `STEEP_DROP` (shade and drainage).
- Dispersal: `THROW_COST` (`energy.cpp`), `THROW_UPKEEP` (`phenotype.cpp`), `limits::MAX_THROW`.
- Body plans (`body.hpp`): `MOUTH_ARC_BASE`, `MOUTH_ARC_PER_SIZE`, `COVER_ARC`, `MEMBRANE_BASE`, `ARMOUR_RESIST`, `ARMOUR_CLOGS` (moved here from `phenotype.cpp`), `STALK_CLEARS`, `BOX_SKIN`, `BOX_DRAG`; `thrust_facing` in `body.cpp`. Keep the arc constants in step with `viewer/blob.gdshader`.
- Shape and stalks (`phenotype.cpp`): `SKIN_UPKEEP`, `STALK_MASS`, `STALK_UPKEEP`; `limits::MAX_STALK`.
- Copying and repair (`phenotype.cpp`): `FIDELITY_UPKEEP`, `REPAIR_GAIN`, `REPAIR_SHARE`; `limits::MIN_MUTABILITY`, `MAX_MUTABILITY`; `AGE_SCALE` (`energy.cpp`).
- Movers' sunlight: `MOVER_SUN` (`phenotype.cpp`; 1 turns it off), `MOVING_SHADE`, `FULL_STRIDE` (`energy.cpp`); folding `UNFOLD_TICKS` (0 = off), `FOLD_STRIDE` (`energy.hpp`).
- Growing up: `GROW_STEP`, `GROW_MIN`, `GROW_RESERVE` (`sim.cpp`); LUCA's `birth_size` (`genome.cpp`); `limits::MIN_BIRTH`.
- Vents: `vent_power`, `vent_heat`, `vent_max` (`config.hpp`); `VENT_CHEM` (`energy.hpp`, C by Uday's call); `VENT_BRUSH` (`viewer/main.gd`, how fast the brush paints).

### Known weak spots

- **Matter piles up around vents (chemistry stage 3).** Vents bring up new matter, and nothing takes matter out of the world. With the four test vents, total matter goes from 98,900 to 225,000 in 100,000 ticks (1.26 units a tick), and the soil under each vent reaches about 400 units of A a cell, rising steadily. Life doesn't seem to mind yet (A is spent matter: it costs light to use), but nothing bounds it. Options in 2.1.1 "Open after stage 4". Uday chose to leave it for now (2026-11-12).
- **Vent dwellers are slow and patchy:** two seeds of six by tick 100,000. The far-edge vents stay empty, as they always have (life never reaches them).
- **Neutral exchanges and cross-feeding are unused (stage 4).** Two-input enzymes spread mostly as variants of A ↔ D. Loose B and C stay scarce, so a body living on them has little to live on.

- **Chemistry stage 2 emptied the world a little** (2,480 creatures and 57 species, against 3,230 and 71 in stage 1). Spilled meals feed the eater and its neighbours, so hunting pays better and hunters rose to 61. Levers, all Uday's: `UPTAKE_RATE` (lower slows how fast spills are taken up, so more decays), `WASTE_DECAY`, `TRANSFER_EFFICIENCY`, or spilling only part of the lost share and burning the rest. Without a crowding cost from fouling (only chemicals a body can't burn foul it), canopy shade is the main limit on crowding again.

- **Chemistry stage 1 thinned movers and hunters** (19 and 39 a world against 75 and 70), probably through the power cap on moving; seed 99 fell to 430 before recovering. B and C are carried by a few bodies but have no use until chemicals can leave a body other than by being eaten (stage 2). The inspector's "soil" is now chemical A.

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
- **Prey that flees has not evolved (2026-11-12).** Touch lets a body feel it is being eaten, but almost nothing that is eaten can move: sun-fed movers number 1 to 9 across six worlds under every rule tried, because any motor costs sunlight. Movers are hunters, and their prey sits still. Still not built: food worth remembering (patchy food that recurs in place) and a day worth planning around (a reason to be somewhere at dawn).
- **Brains stayed small, CTRNN or not (2026-11-12, before touch and cheap brains).** Under `MOVER_SUN` there were few movers, so few brains, so few neurons; the CTRNN gave neurons memory but has little to work on (10.7). When movers boom, slow neurons are the ones kept. The open question is what would make thinking pay more often: prey that flees, food worth remembering, a day worth anticipating.
- **Brains stayed small before the CTRNN too.** Across three 200,000-tick runs hundreds of creatures carry a brain, with about 5 links each, but hidden neurons are rare: usually under 1 brain in 10 has one, the most seen in any brain is 2, and they come and go (one run reached about half of brains with a neuron, then lost them). Thinking costs upkeep and so far a few reflexes are enough. The costs and the structural mutation rates are the levers; so is giving brains more to think about (prey that flees, a day worth planning around).
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
- **Growing up brought the drifting plants back in two seeds of six** (10.7). The cause is the canopy: a young body is shorter than any adult, so in a crowded cell it gets no light, and only creeping out from under the canopy rescues it. Letting 30% of the light through the leaves did not help. Letting the young stand at their adult height did, but it is a counterfactual, not yet a rule; read physically it would be "a sapling grows up before it grows out". Other levers: LUCA's `birth_size` (`genome.cpp`; 1.0 gives the old world back), `GROW_RESERVE`, `GROW_STEP` (`sim.cpp`). Uday's call. Uday chose to leave the rules as they are for now (2026-11-12). **Ended the same day by `MOVER_SUN`** (movers catch half the sun), then by folding, which replaced it: no seed of six is taken over (10.7).
- **Movers are now rare again (15 a world on average)**, and the moving hunters that do appear overgraze and crash (10.7). Nothing makes them limit themselves yet; senses for prey density or something that eats movers might.
- **Birth size is still falling at tick 100,000** in every seed and from both starting points; where it settles is not known.
- **A vent does not carry life into the cold on its own.** A vent warms and feeds only its own few cells, and bodies spread by landing next to a parent, so a vent more than a few cells beyond the edge of life is never reached (10.7). LUCAs scattered there at the start die long before anything that burns the vent's chemical evolves (and since stage 3 an uncolonised vent runs dry; see above). "Start from a dead planet with vents only" (4.4) needs something that gets life onto a vent: seeding it (the seed vault, 4.9), wider vent warmth, or vents that start where life already is. Movers have no sense for vents either, beyond leaning towards comfort (vents are warm).
- Shape barely evolves (mean elongation and squareness about 0.1). Extra skin only pays where sun patches outgrow the skin, and streamlining only for the few fast movers.
- The creature snapshot (`creatures.csv`) has five new columns: elongation, squareness, mouth_stalk, canopy_lift, tail_facing; then chemo (gone since chemistry stage 3, with digestion), fed_vent, birth_size, grown, and (stage 4) enzymes. Metrics gained `in_vent`, `chemotrophs`, `mean_chemo` (replaced by `vent_loose` in stage 3), `growing`, `mean_birth_size`, `grown_mass`, then `neurons` (hidden neurons in running brains), `slow_neurons` (tau 30 or more) and `mean_tau` (geometric mean).
- **Designer:** the verdict estimates income from the day-averaged light of one cell at the habitat's sun, as if the body had the cell to itself; crowding and canopy shade are only felt in the test tank. A released design that lands in a crowded place does worse than its verdict. The Release button and the vault's Release drop up to 100 at a time; nothing in the world limits how often (Sandbox).
- Banking writes into the seed vault folder without asking; files are never overwritten (a second bank of the same name gets `_2`).
- `tools/plot.py` gained two panels (ways of life, energy flows) that have not been run: matplotlib isn't installed on this machine.

### What the game is lacking (assessed 2026-11-11, in order of importance)

The headline: **you can't see evolution happening.** The sim produces real variety, but the game gives no way to notice it.

1. **Species: mostly built 2026-11-11.** Genome distance, species ids at birth, names, chronicle entries, a species list, a colour-by-species view and the tree of life now exist. Split checks (9.3 step 3) and following one species with alerts (4.8) were built 2026-11-12. Still missing: fossils and the museum (4.8).
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
- [x] **Nutritious meals as a specialisation.** *(built 2026-11-11 as the `digestion` gene; applies to living prey and corpses; removed in chemistry stage 3, 2026-11-12: every eater keeps 70%)* An evolvable trait that gets more out of what is eaten, with extra energy cost. (Claude's reading: a digestion gene on the eater that raises transfer efficiency above the base 70% and adds upkeep; confirm whether it should apply to corpses only or to living prey too.)

**Terrain**

- [x] **Slope shade.** *(built 2026-11-11)* Ground facing away from the sun and valley floors get less noon light. A static field, recomputed only when terrain changes, so the terrain brush changes where plants can live.
- [x] **Matter washes downhill.** *(built 2026-11-11)* Every few ticks a little soil nutrient moves to the lowest neighbouring cell. Ridges become bright but poor, valleys rich but dim. Must keep `total_matter()` constant.

**More rules (second batch, picked by Uday 2026-11-11)**

- [x] **Sunbathing and moving conflict.** *(built 2026-11-11)* Light capture drops while a body is moving (section 2.2 always said this; it was never built). Pushes lineages to commit to staying put or to moving: the plant/animal split.
- [x] **Dormancy.** *(built 2026-11-11)* A trait that drops upkeep to near zero when it is dark or cold, at the cost of being helpless and slow to wake. A second answer to night and winter besides storing more, and a seed bank after a catastrophe.
- [x] **Poison.** *(built 2026-11-11 as a body part)* A part that makes a body costly to eat: whoever drains it loses energy. Costs upkeep to carry. A different defence from armour; opens the door to warning colours and bluffing once pigment can be sensed.
- [x] **Gene transfer on contact.** *(built 2026-11-11; through feeding contact, into the genome only)* When two bodies touch, a small chance that one part's blueprint is copied across. Reuses the contact rule; lets an invention spread between lineages. A stand-in until sexual reproduction exists. Must use the sim `Rng`.
- [x] **Waste.** *(built 2026-11-11; waste holds energy, not matter; eaten through a `detox` gene, not the mouth)* Living produces a waste chemical in the cell that slowly poisons whatever made it, unless something else absorbs it as food. A second cost of crowding and a niche for cleaners. The smallest useful piece of the chemistry layer (2.1).
- [x] **Growing up.** *(built 2026-11-12 as a `birth_size` gene; the young grow from soil in their own cell)* Children are born small and grow by taking in matter, instead of appearing full-size. Young are vulnerable, and many-small versus few-large offspring becomes a real choice. The largest item: it touches the phenotype compiler and the matter books. Best judged once species are visible.
- [x] **Evolvable mutation rate and lifespan.** *(built 2026-11-12 as `mutability` and `repair`)* Two genes: how sloppy copying is, and how much energy goes into repair to slow wear. Each needs a cost (8.2).
- [x] **Vents.** *(built 2026-11-12 as a brush power and a `chemo` gene; vents also warm the ground. Since chemistry stage 3 a vent makes loose C and the gene is gone)* A second energy source that needs no sun, placed by a brush (already a player power in 4.1). The first place where life in the dark has an advantage.

Suggested order for this batch: the movement conflict and dormancy first (small, and aimed at "everything is a sessile plant with a mouth"), then poison, then the rest; growing up last.

**Later (deliberately not now):** water (sea level, flagella vs legs, light fading with depth) and seasons (the bright band drifting north and south over a long year).

Suggested order when building: canopy and dispersal first (biggest effect, smallest change), then corpse energy and the size rule for mouths, then the terrain pair, then the three that add genes or senses.

### Next scaffolding

- Speed: keep creatures roughly sorted by cell, then multithread phase 1 by world tiles
- World events that move matter: volcanoes that bring up nutrients, or erosion. Today's events change heat, height, vents, sun and climate only.
- Water as a world layer + water brush

- Species follow-ups: fossils and the museum (4.8); following a lineage across its daughter species (today a follow stays on one species)
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
- **Matter is conserved too.** Anything that builds, kills or moves matter must keep `total_matter()` constant, except what vents bring up from below, which is booked in `matter_in` (`total_matter() - matter_brought_up()` stays constant).
- **The look is pixel art.** No smooth gradients, no 3D meshes for creatures; new body parts get a pixel drawing in `blob.gdshader`.
