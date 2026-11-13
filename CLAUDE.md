# Ecosystem God Game — Design & Project Reference

Owner: Uday · Last updated: 2026-11-12

An artificial-life ecosystem simulator played as a god game. It's like Dwarf Fortress, but more *fundamental*: there are no predefined classes of creature. Simple physical laws (energy, matter, inheritance, mutation) produce producers, predators, parasites, decomposers and symbionts through selection alone. The player shapes the world and its laws, never the creatures directly, and watches life respond.

This file holds everything decided so far, and the rules the code follows now. Long designs not yet built, older measurements and the 2026-11-12 review live in `docs/` (split out in roadmap step 0.6); section numbers are kept, so a reference like "2.7" or "14.2.7" means the same section wherever it is:

| Where | What |
| --- | --- |
| `docs/design/water.md` | 2.7 Water layer (nine stages) |
| `docs/design/predation.md` | 2.8 Rewarding predation |
| `docs/design/cooperation.md` | 2.9 Cooperation and symbiosis |
| `docs/design/sex.md` | 9.4 Sexual reproduction |
| `docs/design/brain.md` | 14.9 Sensor and motor brain |
| `docs/design/stoichiometry.md` | Stoichiometry: one mineral bodies are built from (14.10.2, roadmap step 4) |
| `docs/harness.md` | 14.6 Snapshots, forks, laws, `ab.py`, the shelf: design and as built |
| `docs/review-2026-11-12.md` | 14.1 to 14.5 and 14.10: the design review in full |
| `docs/history.md` | 10.7's older measurements, replaced rules from 10.4, and the finished lists from 11 |

Each of those headings is still here with a summary. Read the file in `docs/` before working on what it covers. Section 13 has the working rules for Claude Code in this repo. Section 14 is a review of the design against the code (2026-11-12), with a design for snapshots, forks and settable laws (14.6), the roadmap (14.7), the removal of camouflage (14.8), a design for a sensor and motor brain (14.9), and points taken from an outside review (14.10).

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
4. Two-input reactions. *Built 2026-11-12 (rules in 10.4 under "Two-input reactions"). 300 bodies a world carry one. Neutral exchanges (equal levels both sides) were built here and deleted after the ablation pass (14.10.8), 2026-11-12.*
Test at each stage: do cross-feeding (one lineage living on what another gives off) and diet splits appear without being coded (milestone 5).

**Next session: start here (written 2026-11-12 after stages 3 and 4).** All four stages are built; 10.4 ("Chemistry", "Loose chemicals", "Vents", "Two-input reactions") describes the code as it is, and 10.7 has the numbers. Where things live: `core/chem.hpp` (chemicals, `Mix`); `Enzyme`, `canonical_enzyme`, `enzyme_words` (`genome.*`); `Phenotype::enz_in/enz_out/enz_step/neutral` and `burns` (`phenotype.*`); `burn` / `charge` / `power` / `convert` / `uptake_rate` / `fouling_cost` (`energy.*`); `Simulation::run_vents` and phase 2b of `Simulation::step` (`sim.cpp`), which per body does, in order: drained by others, meals in (`take_in`), uptake, light charged, neutral exchanges, costs burned, giving off, death, growing, breeding. `tests/test_life.cpp` `test_loose_chemicals`, `test_vents` and `test_two_input_reactions` show how to build bodies with chosen enzymes.

Open after stage 4 (Uday's calls):
- **Vents bring up matter now (decided 2026-11-12), and it piles up.** A vent's C is new matter, and what nobody eats breaks down to A near the vent and stays: under each test vent the soil holds about 400 units a cell by tick 100,000 (the map's mean is 6), still rising. Levers if it matters: a sink (soil A seeping back down at vents, or anywhere), or a vent that brings up only while its cell holds less than some amount. Left as it is for now (Uday, 2026-11-12).
- **Two-input enzymes cost the same per unit of rate as one-to-one ones** but move two units of each side, so A + A ↔ D + D stores 444 a unit against 222 for A ↔ D at the same upkeep. Not taking over (16 to 39 carriers a world), but charging upkeep per unit of matter turned over (twice the rate) would be the even rule.
- **The design's example uses were wrong:** D + A → B + B is downhill (it releases 74), not neutral; the neutral transporter is D + A ↔ C + B. B + C ↔ A + D is neutral too, not uphill. Both are possible now; neither is used.

*Measuring milestone 5 (all stages):* metrics now give `loose_B/C/D`, `rx_XY`, `rx_two`, `two_flow`, `vent_loose` and the pool and tissue mixes, and the creature snapshot lists each body's enzymes (`enzymes`, e.g. `A-D A+A-D+D`). A probe of which species' leavings each takes up is built (the leavings tag, 2026-11-12: `--probe` writes `leavings.csv`; see 10.4 "Loose chemicals"). Cross-feeding is "species X lives mostly on a chemical species Y gives off". First readout (seed 42, 6,000 ticks on from 30,000): the largest flows by far are species living on their own leavings, as 14.2.2 predicts.

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

### 2.7 Water layer: design (proposed 2026-11-12; stage 1 built 2026-11-13)

*In [docs/design/water.md](docs/design/water.md).* Nine stages; stage 1 (sea level, light under water), stage 2 (water as a solvent) and stage 3 (motors by medium, buoyancy; laws, neutral by default), and a depth sense, built 2026-11-13 (10.4 "Sea"): sea level and light under water; water as a solvent; motors by medium and buoyancy; sinking and a sediment sink; currents; heat buffer and ice; rivers; tides; drying out. Roadmap step 6 builds stages 1 and 3 first (14.4).

### 2.8 Rewarding predation: design (proposed 2026-11-12; nothing built)

*In [docs/design/predation.md](docs/design/predation.md).* Why hunters boom and die out, three suspected causes (a full hunter keeps killing; gear costs the same used or not; it can't rest on purpose) and ideas A to E (satiation, resting against working costs, a rest output, cheaper fat, kills worth coming back to). Cause 1 and idea A were not supported by the probe (14.4); roadmap step 3.

### 2.9 Cooperation and symbiosis: design (proposed 2026-11-12; nothing built)

*In [docs/design/cooperation.md](docs/design/cooperation.md).* Why nothing cross-feeds (a partial burn is strictly worse) and ideas A to F: rate against yield, product inhibition, a mouth the brain can close, adhesion from birth, enzymes outside the body, inherited partners; plus the leavings tag and knockout experiments for seeing it. 2.9 A was tried and did not work (14.2.7); roadmap step 4.

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
- **Automatic food web:** built from actual energy transfers; nothing is labelled "predator". *Built 2026-11-12 (`core/foodweb.*`, `viewer/food_web.gd`; 10.6).*
- **Creature inspector:** body parts, energy budget, brain firing as it moves.
- **Heatmap overlays:** light, each chemical, temperature, population density.
- **Natural history chronicle:** an auto-written log like Dwarf Fortress's Legends mode, e.g. *"Tick 41,200: lineage Velox splits in two across the Eastern Range."* Species get procedural names.

**Event detectors** power all of these and double as milestone tests: speciation, first predator (first sustained creature-to-creature energy flow), new trophic level, extinction.

### 4.4 Goals: sandbox plus scenarios

Scenarios are checked automatically by the event detectors:
- Evolve something that moves in under 10,000 ticks. *Built 2026-11-12 as "First steps" (`core/scenario.*`, 10.6); untouched, only 2 of 30 default worlds manage it.*
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

Storms, eruptions, slow climate drift and the odd meteor happen without the player, at a rate set by a slider (off is allowed). They are ordinary power commands cast by the world from dice of its own (see the end of this paragraph), so they are deterministic, logged and replayable like the player's. The point: you come back to a world with a history you didn't write. *Built 2026-11-12 (`core/events.*`, see 10.5). The dice are a stream of their own seeded from the world seed, not the sim's `Rng`, so a rate of 0 leaves the world exactly as before and a power log replays a run with events off.*

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
- **Life:** breeding threshold, child's share, birth size, seed throw, comfort, tolerance, bite, dormancy.
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
- **Water:** flagella work in water and legs on land, so colonising land becomes a visible evolutionary event. Designed in stages in 2.7 (motors by medium are stage 3).
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
- **Skin colour is the hue and pigment genes** (2026-11-12), drawn exactly as `core/colour.hpp` works it out. (Hunters also sensed bodies by it until camouflage was decided against, 2026-11-12; see 14.8.) **Leaves are always green and mouths always red** (Uday), whatever the skin (and in the colour-by-species view): leaves bright green when full, dark when hungry; mouths dark red.
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

*Review 2026-11-12: rules 2 and 7 were not fully kept. Rule 7's half is paid (step 1, 14.5): the tick and the field passes touch only cells that hold something, so an empty 512×512 world costs about 0.05 ms/tick. The hot loops still read the 264-byte phenotype (rule 2); see 14.5 item 3 for why that was left.*

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
| `pigment` | Skin darkness | Absorbs heat: good in cold, bad in hot; also how it shows against the ground |
| `hue` | Skin colour on the wheel, 0..1, wrapping (LUCA 0.09, tan); drawn muted and earthy | No upkeep and no use: drawing only since camouflage was removed (14.8), so it drifts (2026-11-12) |
| `thermal_optimum` | Preferred temperature | Which climates are cheap to live in |
| `thermal_tolerance` | Width of comfort range | Wide costs extra upkeep; specialists are cheaper |
| `repro_threshold` | Energy needed before dividing | Low = many weak offspring; high = few strong ones |
| `offspring_share` | Energy fraction given to child | Same r/K trade-off from the parent's side |
| `bite` | Drains the living faster: pull × (1 + bite), up to ×4 (LUCA: 0) | Upkeep 0.06 per unit, paid whether or not there is anything to bite |
| `dormancy` | How deeply it sleeps through dark and cold, 0 to 1 (LUCA: 0) | A little upkeep always; asleep it is helpless, and it wakes slowly, so it misses the dawn |
| `seed_throw` | How far a child can be flung, in cells (LUCA: 0) | Upkeep per cell of range, plus energy ∝ child mass × distance² for every throw |
| `elongation` | Up to 3× as long as wide, same volume (0..1, LUCA: 0) | More skin (sun room, but heat loss, fouling, `SKIN_UPKEEP`); a long body moves more cheaply and faster (2026-11-12) |
| `squareness` | Round to box (0..1, LUCA: 0) | Up to 24% more skin, and 30% more drag on the move |
| `birth_size` | Fraction of adult mass a child is born at, 0.1 to 1 (LUCA: 0.5); it grows the rest itself | A small child is cheap to make but easy to eat, catches little light and can't breed until grown; a big one costs its parent more (2026-11-12) |
| `parts` | Body modules, each with a size, an angle on the outline (`theta`) and, for mouths and sun patches only, a stalk | Each adds mass and upkeep; a stalk adds mass and upkeep per unit of length. Where a part sits matters (10.4) |
| `brain` | Hidden neuron count, a list of weighted links, and per neuron a time constant `tau` (1 to 1,000 ticks) and a bias, all evolved | Empty for LUCA; every neuron, link and unit of weight or bias costs a little upkeep (nearly nothing since 2026-11-12), and a slow neuron costs `MEMORY_COST` per e-fold of tau |

**Likely additions:** reproduction mode (`sex`, `gamete_size` and a mate preference; designed in 9.4), chemistry genes (which reactions a creature catalyses).

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
- Rates are the same for every lineage (the `mutability` gene that scaled them was deleted after the ablation pass, 14.10.8); a god-mode "law" knob could scale them for the whole world later. Cap or price part counts so genomes can't bloat forever.

---

## 9. Speciation

### 9.1 Two modes

- **Asexual (start here):** no interbreeding, so species are **clusters in genome space** detected by the game. Labels drive the tree of life, chronicle and colours, but don't change sim behaviour.
- **Sexual (add later; designed in 9.4):** one rule makes species real: **two creatures can only produce viable offspring if their genomes are similar enough** (fertility falls with genetic distance, reaching zero past a threshold). Then a population split by a mountain diverges; when the mountain erodes, the halves either merge (with a hybrid zone) or stay as two true species. Neither outcome is scripted.

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

### 9.4 Sexual reproduction: design (proposed 2026-11-12; nothing built; after water, 2.7)

*In [docs/design/sex.md](docs/design/sex.md).* Five stages, nothing built, after water: recombination and mating by contact; gametes as a drifting field; gametes stuck to skin (pollination); gamete size (anisogamy); mate preference. Build stage 1 without the fertility curve first (14.10.5).

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
| `core/snapshot.*` | Snapshots: the whole state of a world in one file (save, load, exact round trip) | Scaffolding (built 2026-11-12) |
| `core/scenario.*` | Scenarios: a goal with a clock, checked with the census (`scenario_list`, `check_scenario`); saved in snapshots | Scaffolding (built 2026-11-12) |
| `core/laws.*` | Laws: the tunable numbers of the physics in one struct (`g_laws`), by name, settable without a rebuild | Scaffolding (built 2026-11-12) |
| `core/species.*` | Genome distance, species registry (ids, names, parents, populations, splits) | Scaffolding (built 2026-11-11); Uday tunes the weights |
| `core/foodweb.*` | The food ledger (energy taken in, by taker species and where it came from) and `food_web()`: nodes, edges, trophic levels | Scaffolding (built 2026-11-12) |
| `core/footprint.hpp` | Disc-overlap stencils for light over a body's footprint (`LIGHT_FOOTPRINT`) | Scaffolding (built 2026-11-12) |
| `core/rng.hpp` | Deterministic xoshiro256** | Scaffolding |
| `core/config.hpp` | World settings and units | Scaffolding |
| `headless/main.cpp` | CLI runner | Scaffolding |
| `tools/plot.py` | `run.png` + `map.png` | Scaffolding |
| `tools/ab.py` | Paired comparisons and sweeps: control against variants over forks of saved worlds or fresh seeds (14.6.4) | Scaffolding (built 2026-11-12) |
| `tools/shelf.py` | The shelf of mature worlds in `worlds/` (git-ignored): builds and extends it (14.6.5) | Scaffolding (built 2026-11-12) |
| `godot/` | GDExtension glue: `LucaWorld` wraps the sim and hands Godot packed arrays | Scaffolding (built 2026-11-11) |
| `viewer/` | Godot 4.7 project: `main.gd` (camera, brushes, UI), `tree_view.gd` (tree of life), `food_web.gd` (food web), `species_card.gd` (species card), `designer.gd` (creature designer and seed vault) with `design_canvas.gd`, `brain_graph.gd` and `sprite_stage.gd`, terrain and blob shaders | Scaffolding (built 2026-11-11) |
| `view.sh` | Build the extension and open the viewer | Scaffolding |
| `tests/test_powers.cpp` | Brush, ramp, script, determinism and audit tests (`./build/test_powers`) | Scaffolding |
| `tests/test_life.cpp` | Day/night, mutation, costs, movement, contact, scavenging, species, energy and matter books (`./build/test_life`) | Scaffolding (built 2026-11-11) |
| `scripts/long_winter.txt` | Demo god-power script | Scaffolding |
| `scripts/four_vents.txt` | The four test vents of 10.7, open from tick 0 | Scaffolding |

All paths are inside `luca/`.

Stack: C++20, CMake, Python + matplotlib for plots, Godot 4.7 + godot-cpp for the viewer. The core has **no Godot dependency**; `godot/` depends on the core, never the reverse.

### 10.2 Build and run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/luca_headless --ticks 20000 --out out      # --help for all options
python3 tools/plot.py out
```

Options: `--ticks --seed --size --pop --light uniform|gradient --sun --day --day-swing --windfall --nutrients --interval --script --follow --events --load --save --save-every --fork-seed --knockout --probe --scenario --set --laws --list-laws --out --no-snapshot --quiet`. `--save FILE` saves the world at the end, `--save-every N` also every N ticks (`world.snap` becomes `world_12000.snap`), and `--load FILE` continues a saved world for `--ticks` more ticks: its config comes from the file, so the flags that set up a new world are refused with it (`--events`, `--interval` and `--script` still apply). Metrics rows fall on multiples of `--interval` counted from tick 0, so a loaded run's rows line up with a straight run's. `--events F` lets the world act on its own (default 0, off). `--follow NAME` follows the species of that name from the moment it appears and writes its alerts to `alerts.txt`. `--set NAME=VALUE` (repeatable) and `--laws FILE` (one `NAME VALUE` a line, `#` comments) change a law for the run; with `--load` they apply on top of the laws the world was saved under. `--list-laws` prints every law with its value. Each run writes `laws.txt`, the laws that differ from the defaults, readable back with `--laws`. With `--load`: `--fork-seed N` gives the world new dice, and `--knockout SPECIES` (name or id, repeatable) kills every member at once (14.6.3). `--probe` also writes `probe.csv`, the meal and death ledgers (14.6.4), and `leavings.csv`, who took up whose leavings. `--scenario KEY` sets a goal (`first-steps`) and says at the end whether it was met.

Comparisons and the shelf (14.6.4, 14.6.5):

```bash
tools/shelf.py --ticks 100000                       # 30 seeds to 100,000 (the default)
tools/ab.py --worlds worlds/plain_s*_100000.snap --forks 2 --ticks 20000 --variant "--set TRANSFER_EFFICIENCY=0.5"
tools/ab.py --seeds 42,7,99,1,2,3 --ticks 100000 --variant "--set UNFOLD_TICKS=200" --variant "--set UNFOLD_TICKS=1200"
```

God powers, headless: `./build/luca_headless --ticks 16000 --script scripts/long_winter.txt`. A script can drop creatures: `<tick> seed <x> <y> <radius> <count> <creature file on one line, ' ; ' between lines>`. Each run also writes `chronicle.txt`, `powers.txt` (every command cast, replayable with `--script`) and `species.csv` (every species there has ever been, with its parent: the tree of life as a table; the last column, `split`, is 1 for a species made by a split check).

### 10.3 Units (proposed; settle before tuning)

- 1 tick = 1 minute of world time
- 1 energy unit = what one cell receives per tick under full sun, **averaged over a day**. With the day/night cycle noon is twice that and night is zero.
- 1 day = `day_length` ticks (default 600, so the clock shown in the viewer is not literal minutes; `--day 0` turns the cycle off)
- 1 cell = `cell_size` world units (default 1)
- For scale: a LUCA burns about 0.35 a tick (0.46 before `METABOLIC_RATE` went to 0.75), holds up to 650, divides at 600, and its body is worth 244 (since 2026-11-11; before that 450, 400 and 44). Since chemistry stage 1 these are chemicals: a LUCA's body is 1.1 units of D, its store holds up to 2.9 units, and its one enzyme turns over 0.015 D a tick (3.3 energy).

### 10.4 Rules the scaffolding enforces

- **Finite light, canopy rule (2026-11-11):** each cell gets `light` energy per tick. If the bodies in it ask for more, the tallest (`Phenotype::canopy`: core radius, plus the stalks its sun patches stand on, since 2026-11-12) takes its fill first, then the next, and smaller ones get what is left; bodies of exactly the same size share alike. A full-patch LUCA asks for the whole cell, so a smaller body under it gets nothing unless the taller one's patch is sparse. Unabsorbed light is lost. (`Simulation::share_light_by_canopy`)
- **Light over the footprint (adopted 2026-11-13; law `LIGHT_FOOTPRINT`, 1):** a body asks for light in every cell the disc of its radius covers, centred on its cell, in proportion to how much of the disc falls there (`core/footprint.hpp`: a stencil per radius in steps of 0.02). In each cell the canopy rule above shares that cell's light among every body asking there, tallest first (`Simulation::share_light_by_footprint`). Light falling off the map's edge is lost. So a body bigger than its cell catches light its neighbours don't use, and shades them where it overlaps them. `LIGHT_FOOTPRINT=0` gives back the one-cell rule, which `test_canopy` still checks.
- **A body that cannot pay its costs in full this tick dies.** What it can pay is limited by its store and by what its enzymes can release in a tick ("Spending is burning", below). Until chemistry stage 1 the rule was "costs capped at available energy; a creature at zero dies". *Measured 2026-11-12: 0.6 to 1.6% of deaths happen with enough in store to pay (14.2).*
- **Energy audit:** each metrics interval checks `stored(end) == stored(start) + in − out`. Expected error ≈ 1e-6 (float rounding). A jump means energy is being created or destroyed, and evolution will exploit it. "Stored" is energy in living creatures + energy locked in their bodies + energy left in corpses + energy in waste; since chemistry stage 1, the first three are the energy of the chemicals in every store, body and corpse (amount × level). "In" is light plus windfalls (`in_windfall`) plus vents (`in_vent`). "Out" is upkeep (less the part given off as waste), thermal, movement, transfer loss, corpse decay, waste breaking down, fouling, poison and overflow (`out_waste`: a full body can't hold more; not the waste chemical).
- **Matter audit:** soil nutrient (chemical A) + corpse matter + loose chemicals + the mass of every living store and body changes only by what vents bring up from below: `total_matter() - matter_brought_up()` is constant (`Simulation::total_matter()`, column `matter_total`; `TickStats::matter_in`, column `matter_in`). With no vents, total matter is constant.
- **Reproduction transfers energy and matter** (never creates them). Since chemistry stage 1, everything comes out of the parent's store, as it is mixed: first the child's body, then the cost of a throw (burned), then the child's store, `offspring_share` of what is left. A body breeds when its store is worth `repro_threshold` and holds more than 0.75 × `birth_size` of its body's mass. It tries once every 8 ticks.
- **Dispersal is paid for (2026-11-11):** a child lands touching its parent (their two reaches apart, about one cell for LUCAs). The `seed_throw` gene flings it up to that many cells further, at a cost (`throw_cost`, booked under movement). `offspring_spread` is gone. A child's path stops at any step in the ground taller than `birth_max_step` (4 height units between neighbouring cells; natural terrain tops out near 3.1), so a tall raised mountain blocks births as well as walkers.
- **Death leaves a corpse** in the cell: since chemistry stage 1 its body and whatever its store held go into the cell's corpse, a `Mix` of chemicals (a LUCA's body is 1.1 units of D, worth 244). Corpses weather away slowly on their own (0.4% every 8 ticks), burning down to A in the soil; anything with an absorber eats them as chemicals (see Chemistry below).
- **Windfalls (2026-11-12, Uday's request):** at the start of every tick, after powers, food falls on random cells: `windfall_rate` (0.001, `config.hpp`) energy per cell per tick on average, in lumps of `windfall_size` (250, about one LUCA corpse), so one lump every ~16 ticks on a 128×128 map. The number of lumps per tick is the whole part of the mean plus one more by chance, and each lands on a uniformly random cell; all drawn from one key a tick (`Draw::Windfall`, `Simulation::drop_windfalls`). A lump goes into the cell's corpse, so it is eaten, sensed (the "corpse here" input and the lean towards the dead) and weathered exactly like a corpse. Since chemistry stage 1 a lump is D made out of the cell's own A (`windfall_size` / 222 units, or what A the cell has), so it carries matter, and a bare cell gets nothing. It is the second way energy enters the world: booked in `energy_in` and broken out as `in_windfall` (metrics columns `in_windfall`, `windfalls`). Because it sits in the corpse field, food from it counts as `fed_dead`: a body living on windfalls shows in the census as a scavenger. `--windfall 0` turns it off.
- **Growing up (2026-11-12):** a child is born at `birth_size` of its adult mass and grows the rest. A young body is the adult scaled down by mass (`compile_phenotype(g, grown)`): mass, body energy, storage and upkeep (Kleiber again) shrink with it, its length goes as the cube root and its skin, sun area and canopy height with it, and it loses heat faster. Pull, armour and speed are not scaled; the size rule at the mouth already makes a small body easy to drain and weak at draining. Every 8 ticks (`age & 7 == 4`) an awake young body puts the energy it holds above `GROW_RESERVE` (0.3) of what it can hold into body, at most `GROW_STEP` (5%) of its adult mass at a time and at least `GROW_MIN` (1%) unless that finishes it, and is recompiled at its new size (`Phenotype::grown`). Since chemistry stage 1 the body is built out of its own store, as it is mixed, so a plant's young are made of what it made; the soil only matters through the light the store was filled with. Only a grown body breeds. The parent pays for the child's body at birth size out of its own store. Chemicals move from store to body, so neither book changes; `TickStats::grown_mass` counts the matter built. Bodies placed by `add_creature` and the first LUCAs start grown (it takes an optional `grown`).
- **Sensor brain, stage 1 (2026-11-13, 14.9; law `SENSOR_BRAIN`, **on by default since 2026-11-13 (Uday)**; `--set SENSOR_BRAIN=0` runs the old brain, identical to the build before, tested on a fresh world and on a loaded shelf world):**
  - **Node numbers are fixed offsets:** inputs from 0, outputs from `FIRST_OUTPUT` 32, hidden from `FIRST_HIDDEN` 48 (`genome.hpp`), so a new input or output renumbers no saved brain. Snapshot version 3 reads version 2 files and renumbers their links (`legacy_source` / `legacy_target`, `snapshot.cpp`); creature files name nodes, so they need nothing.
  - **Sensors:** a sensor `Part` has a `channel` (`SenseChannel`: light, warmth, corpse, soil, loose B / C / D, living mass). Off the midline it is a mirrored pair, one point at +theta and one at −theta (`SensorPoint`, in `BrainPlan`, up to 8). Each tick a point reads its field at the body's reach plus `SENSE_CELLS_PER_SIZE` × size cells out along the heading plus its angle, by bilinear interpolation over four cell centres (`Simulation::sample_channel`).
  - **The bus:** per channel the brain gets a level (`m / (|m| + k)`, size-weighted mean) and a side contrast (`(left − right) / (|left| + |right| + k)`, 0 unless both sides have a sensor), one `k` per field. Inputs `bus_level(c)` and `bus_contrast(c)`; plus `IN_NOISE` (a hash of id and tick, `Draw::Noise`).
  - **Effectors:** `OUT_THRUST` (0 to 1 of top speed, plus `THRUST_BASE`) and `OUT_TURN` (−1 to 1 of `TURN_RATE` radians a tick). Heading is state (`hx`, `hy`). Turning is paid as rim distance × `TURN_COST` in `movement_cost` and counts as moving (folding). The lean, kin and wander outputs are not read with the law on.
  - **Mutation:** with the law on, new links draw from every input and the two effectors only; a new sensor gets a random channel and a sensor's channel changes at `P_CHANNEL`; the first brain is bias → thrust 0.5 and noise → turn 1. With it off the draws are the ones the old build made.
  - **Stage 2, mouth gate (2026-11-13, same law):** `OUT_MOUTH` scales a body's pull on the living and the dead by `1 − shut`, where `shut = 1 − clamp(1 + output, 0, 1)` (a brain that says nothing leaves the mouth open; read one tick late, kept per body in `Creatures::shut` and saved in snapshots). A brain now exists on any body with a motor or a mouth, and a brain on a body that cannot move only gates its mouth. A mouth with no motor gets two random links to the mouth when it has no brain. Metrics `rooted_brains` and `gated`. Three seeds, 40,000 ticks (`ab_out/brain2`): 2,100 to 2,900 bodies a world (40 to 60%) carry a brain on a rooted body, and 98% of those are wired to the mouth; population 3,367 → 5,017 (+49%, 3 of 3 seeds), species +8%, hunters unchanged. As 14.9 feared, thinking is nearly free and brains drift onto everything with a mouth; most of that wiring is the two random links a new brain starts with, so it does not yet show the gate is *used*. The fuller world may come from partly shut mouths easing grazing. Not yet shown: a link from hunger, eating or crowding to the mouth that does something.
  - **Stage 3, the sleep drive (2026-11-13, same law):** `OUT_SLEEP` is the sleep level a brain aims for, 0 to 1, held to the `dormancy` gene and reached at the old rates (`next_sleep_driven`; `Creatures::rest`, saved). With the law on, a body with a brain has no dark-or-cold trigger; a brainless sleeper keeps the old reflex (the open question, decided that way). A sleeper has a brain (`dormancy` > 0 counts as something to drive) and keeps thinking while asleep, since the brain is what wakes it; asleep it cannot move or gate its mouth. A new sleeper's first brain is the old reflex as wiring: bias 0.5, light −1.5, cold +2 into sleep. Metric `sleep_wired`. Three seeds, 40,000 ticks (`ab_out/brain3`): mean dormancy 0.05 to 0.07 (0.05 to 0.10 before), 1,100 to 1,250 brains wired to sleep a world, population +32%, species +26%, movers 103 → 267; deep sleepers are near 0 at the sampling times in both arms, so no new habit (sleeping off a meal, ambush) is shown. *Stages 1 to 3 are all built behind `SENSOR_BRAIN`; **it is on by default as of 2026-11-13.** `--ticks 5000` is now 1,329 creatures, 0.095 ms/tick, audit 1.7e-7 (1,352 with the old brain). `test_life` runs the old brain by default (`main` sets the law to 0, `legacy_laws()`) and the three sensor-brain tests set it themselves. The shelf in `worlds/` was rebuilt under the new brain; the old one is in `worlds_legacy/` (old-style brains there do nothing with the law on). The results above rest on short runs (3 seeds, 40,000 ticks) except stage 1 (6 seeds, 100,000).*
  - **Metrics:** `eyed` (running brains with a sensor point) and `steering` (running brains with a direct side-contrast → turn link); `ab.py` reports both.
  - **Not built:** the moving-mass, size and hue-band channels (kin by likeness of hue), fore-and-aft contrast, per-motor turning strength, and the species card for the new brain. *Designer, 2026-11-13:* the part panel has a channel picker for sensors; the brain graph shows the 26 senses and only thrust and turn when the law is on (`layout.out_start`, `layout.sensor_brain`); the reflex cards and the Wander and Hunter templates switch to the new brain; the extension builds and the designer opens with the law off. Not checked with the law on (the viewer has no switch for laws yet). Laws added: `SENSOR_BRAIN` (0), `TURN_RATE` (0.3), `TURN_COST` (0.1), `THRUST_BASE` (0), `ARC_FOLDS` (0), `P_CHANNEL` (0.05).
  - **Fresh seeds, 6 seeds × 100,000 ticks (`ab_out/brain1*`):** first movers appear at the same ticks as before, at 30,000 ticks the sensor brain has the more movers (+50%, 3 seeds), but by 100,000 they have died out (movers 89 → 1 to 6 a world) at `TURN_COST` 1, 0.1 and 0.03, `TURN_RATE` 0.35, 0.05 and 0.02, and with `THRUST_BASE` 0.5 (move by default, as the old throttle did). So the cost of turning, the turn rate and the default thrust are not the cause. **The cause was folding:** turning counted as moving, so any body that turned folded its sun patches (`FOLD_STRIDE`), which shut the creeper niche the old movers live in. With `ARC_FOLDS` 0 (turning is paid for but does not fold; now the default, with `TURN_COST` 0.1) the same six seeds hold 111 movers against 89 (+24%, 4 of 6 seeds), population -4%, species +5%, and 59 brains steer by side contrast (0 to 5 before). *Stage 1 now passes its judge on six seeds; the law is still off by default until the designer and the rest are ready.* Trace of seed 42: the movers at tick 30,000 are hunters (1,005 eaten from the living against 8 from light), the world fell from 5,637 to 849 creatures by tick 45,000 (they ate their prey down), and food came back without the movers. A few brains do wire a side contrast to the turn (4 to 7 in all six seeds). Population and species are unchanged within noise. *Stage 1 does not yet pass its judge ("movers persist"); the law stays off.*
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
  - **The leavings tag (2026-11-12, observation only; `Simulation::tag_leavings`):** each cell remembers the species that gave off most of its loose chemicals and how much of their worth that covers (`leaver_`, `left_`). A species that gives off more than the tag holds takes it over; the tag drifts and fades with the chemicals; uptake credits the taker's species with that share of what it took up (`leavings_eaten()`, by taker and giver species). Headless `--probe` writes it as `leavings.csv`, most first. Not saved in snapshots: a loaded world starts untagged. Draws nothing and changes no run.
  - **Drift and decay:** every 8 ticks each cell hands `WASTE_SPREAD` (20%) of its loose chemicals to its four neighbours and `WASTE_DECAY` (4%) of what is left breaks down to A in the soil, its energy lost as heat (`out_waste_decay`) (`Simulation::spread_dissolved`).
  - The cross-feeding the design asks for needs nothing more: a body with D ↔ B gives off B, and a neighbour with A ↔ B takes it up and lives partly on it (`test_loose_chemicals`). Metrics: `loose_B`, `loose_C`, `loose_D` (total units loose in the world); the snapshot's `detox` column is now `burns` (the bit mask). The inspector and species card say what a body burns.
  - `nutrient_mean` went from 0.8 to 6, because stores now hold matter.
  - The metrics give `pool_B/C/D` and `tissue_B/C/D` (total units), `mean_enzymes`, and `rx_XY` (bodies carrying each reaction). The inspector lists enzymes and what the store and body are made of.
- **One share of a meal for all (chemistry stage 3):** the `digestion` gene is gone (`DIGEST_GAIN`, `DIGEST_UPKEEP`, `W_DIGESTION`, `mean_digestion` with it); every eater keeps `TRANSFER_EFFICIENCY` (70%) of what it can burn and spills the rest. Uday's call.
- **Two-input reactions (chemistry stage 4, 2026-11-12):** an `Enzyme` may have a second input and output, X + Y ↔ Z + W, one unit of each, so matter balances (`x2`, `y2`; `NO_CHEM` on both for one-to-one). `canonical_enzyme` (`genome.cpp`) is the one way to write one: sides sorted, the poorer side first, never the same on both sides; `clamp_genome` and the phenotype compiler both use it. Written in files and the snapshot as e.g. `A+A-D+D` (`enzyme_words`).
  - **Burning and charging** (`burn`, `charge`, `power` in `energy.cpp`) run every enzyme the same way: downhill from the richer side to the poorer when burning, uphill when charging, releasing or storing (level of one side − level of the other) a unit. A runs only as far as the scarcer input allows (A comes from the soil, and X + X needs two X a unit) and, when it leaves the store holding more than before (A taken from the soil into richer chemicals), only as far as there is room. One-to-one enzymes behave exactly as before. `power` now takes the cell's soil A.
  - **Equal levels:** a reaction with equal levels on both sides (B + C ↔ A + D, D + A ↔ C + B, ...) releases nothing either way, so it never runs and only costs upkeep. *Neutral exchanges, which ran such enzymes towards what the body burns, were deleted 2026-11-12: the ablation pass found they changed nothing (14.10.8).* `burns` means: on the richer side of a downhill enzyme.
  - **Mutation:** `P_ENZ_PAIR` (0.01) gives a one-to-one enzyme a random second pair or takes a two-input one's away; a swap can change any of the four chemicals. New enzymes are one-to-one.
  - **Species distance:** each distinct reaction is its own kind (`W_ENZ_KIND`), so X + Y ↔ Z + W is not X ↔ Z.
  - **Cost:** `ENZYME_UPKEEP` per unit of rate, as for one-to-one (so a two-input enzyme moves twice the matter for the same upkeep; see 2.1.1 "Open after stage 4").
  - **Metrics:** `rx_two` (bodies with any two-input enzyme) and `two_flow` (units turned over by two-input enzymes). The creature snapshot ends with an `enzymes` column. The inspector writes them `A+A<->D+D`.
- **Stoichiometry: the mineral P (2026-11-12, a law, off by default; [docs/design/stoichiometry.md](docs/design/stoichiometry.md)):** one conserved element with no energy level, beside the chemicals. It sits in five places: soil P per cell (`Soil::mineral`), corpse P per cell (`Soil::corpse_p`), and in each body its tissue P and store P (`Creatures::tissue_p`, `pool_p`).
  - **Off** (`P_BASE` 0, the default): no body needs any P, nothing takes it up, and runs are identical to the build before. Soil P is still laid out and washes downhill.
  - **What a body needs:** `P_BASE` × (mass + `P_PART_SCALE` × Σ `P_PER_SIZE` × part size). The table `P_PER_SIZE` sits in `phenotype.cpp`: armour 2, legs 1.5, mouth, tail and eye 0.5, poison 0.2, sun patch and storage 0. So a shell is dear on poor ground, with no gene for it.
  - **Phenotype fields:** `p_need` is the need at the body's present size, `p_adult` the need grown, and `p_store_cap` (`P_STORE` × `p_adult`) the most P its store keeps.
  - **Uptake:** a body takes up `P_UPTAKE` × skin × (1 − sleep) of its cell's soil P a tick while its store has room, shared out if the cell's bodies ask for more than there is.
  - **Meals:** draining a body takes its store's P in proportion to the store taken; eating a corpse takes the corpse's P in proportion to the mass taken. The eater keeps what fits in its store and gives the rest off into its cell's soil (`p_given_off`), where plants take it up.
  - **Growing and breeding (Liebig):** a growth step builds no more body than its store's P allows. A body breeds only when its store also holds its child's P. The child gets the P its body needs, plus `offspring_share` of what is left, up to its own cap.
  - **Death:** a body's P goes into its corpse. Weathering frees corpse P at `P_WEATHER_SHARE` (0.25) of the rate it frees mass, and all of it once the corpse is gone, so eating the dead is the fast way P goes back to the soil.
  - **In and out:** P does not drift. It washes downhill with soil A. Vents bring up `VENT_P` P per unit of C they make, and meteors land bodies with the P they need. Both are booked as `p_in` and `p_brought_up()`, so `total_p() - p_brought_up()` is constant.
  - **Starting soil:** laid out from its own noise (`lay_out_mineral`), patchier than A, with mean `P_MEAN` (0.3).
  - **Snapshots:** P sits in two sections of its own (`MINR`, `BODP`). A world saved before P loads with soil P laid out afresh and each body holding exactly the P it needs, so shelf worlds can be forked with `--set P_BASE=...`.
  - **Metrics:** `p_total`, `p_in`, `p_soil`, `p_corpse`, `p_bodies`, `p_taken_up`, `p_given_off`, `p_corpse_eaten`, `p_corpse_weathered`, and two counts of breeding tries by grown, awake bodies: `p_limited` (energy enough, P short) and `energy_limited`.
  - **Viewer:** a "Mineral (P)" overlay, and inspector lines for body P, store P and a child's need once P is on. `test_stoichiometry` checks the rule.
  - *Not yet judged.* First look (seed 42, 20,000 ticks, `P_BASE` 0.05; 3,504 creatures with it off): at `P_MEAN` 0.05 the world holds 169 creatures, at 0.2 1,916, at 0.5 2,899, and at 1 3,074. On a shelf world (seed 42 at tick 100,000, `P_MEAN` 0.3), 6,000 ticks with P on end at 2,798 creatures against 3,971 without, with P holding back 22% of breeding tries. Both books stay exact.
- **Sea (water stage 1, 2026-11-13):** one sea level for the world (`World::sea_level()`, config `sea_level`, default 0 = no sea; `--sea H`). A cell lies under water when its height is below it, at `depth` = sea − height. Light under water falls as e^(−depth / `LIGHT_DEPTH`) (law, 4.0 height units), in `World::refresh_light`; nothing else feels water yet (movement, heat and chemicals are later stages). The `sea` power (global, a delta, rampable, never below 0) raises or lowers it; the viewer has Sea −4 / +4 buttons, draws water in four depth bands with a glint (`terrain.gdshader`, `sea_level` uniform) and takes `--sea=H`. Saved in snapshots. `test_sea` checks the light. *Quick judge (seeds 42 / 7 / 2, 40,000 ticks, `--sea 12`, about a third of seed 42's map under water; `ab_out/water1`):* population −24%, energy in −25%, species −16%, hunters −21%, movers −60% (3 of 3 seeds): the drowned third is dark and mostly empty, as stage 1 alone should give. Default stays 0 until water does more than shade.
- **Water stage 3, a medium for movement (2026-11-13; laws, neutral by default):** a body has a `walk_speed` (on land) and a `swim_speed` (under water); `max_speed` is the faster, so "can it move" is unchanged. A tail pushes `FLAGELLUM_ON_LAND` (1) as hard on land, legs `LEGS_IN_WATER` (1) as hard in water; the brain moves at the speed of the medium of the cell it stands in. Under water `BUOYANCY` (0) takes that share off the per-mass cost of moving and of climbing (`movement_cost(..., wet)`; until 2026-11-13 climbing under water was free whatever `BUOYANCY` said, so the defaults were not neutral where there was a sea). With the defaults runs are identical, sea or no sea (`--ticks 5000` 1,329).
  - **Swimmers and walkers (2026-11-13):** the census counts `swimmers` (movers under water whose `swim_speed` beats their `walk_speed`) and `walkers` (movers on land the other way round), only when there is a sea. The chronicle logs the first 10 of each: *"The first swimmers: ..."* and *"Life crawls ashore: ..."*. The two flags and counts sit in their own snapshot section (`WATR`), so older files load. Metrics columns `swimmers`, `walkers`, `wet_bodies`, `wet_radius`, `dry_radius` (mean core radius under water and on land). `test_water_motors` checks the speeds, the costs, the census and the chronicle.
  - *Judged on one run (seed 42, `--sea 12`, `FLAGELLUM_ON_LAND=0.3 LEGS_IN_WATER=0.3 BUOYANCY=0.5`, 100,000 ticks; `ab_out/water3/seed42`), against the stage-1 sea run of the same seed (`ab_out/water1/v1/seed42`, 40,000 ticks):* movers about 2.5 times as many by tick 33,000 (195 against 76), population +10%, established species fewer (35 against 52). Life crawls ashore by tick 9,200, but swimmers never take hold: at most 4, and none after tick 60,000. The sea holds 50 to 220 bodies (5% of the world or less), and bodies there are no bigger (radius 1.23 against 1.38 on land at tick 100,000). Over the last fifth: 1,480 creatures, 75 species, 186 movers, 165 walkers, 0 swimmers; worst audit error 8.2e-7. **Reading:** a medium for movement does nothing for the sea while the sea has no food and nothing can tell it is in water. It helps movers on land. Hence stage 2 and the depth sense below.
- **Water stage 2, a solvent (2026-11-13; laws `WET_SPREAD`, `WET_UPTAKE`, 0 by default):** under water a cell hands on `WASTE_SPREAD` × (1 + `WET_SPREAD`) of its loose chemicals each pass (at most 90%; the vent tracer and the leavings tag drift with them), and a body takes up `UPTAKE_RATE` × (1 + `WET_UPTAKE`) through its skin (`spread_dissolved`, phase 1 of `step`). Spills, leavings and a vent's C mix through the sea instead of sitting where they fell. On land nothing changes. With both at 0 runs are identical.
- **The depth sense (2026-11-13):** a ninth sensor channel, `CH_DEPTH`, reads water depth (0 on land; `k` 2). It is the last channel, so no node numbers move. Mutation draws it only in a world with a sea: `mutate(g, rng, channels)` takes how many channels it may draw from, `CH_COUNT` with a sea and `LAND_CHANNELS` (8) without, for new sensors, channel changes and new brain links alike. So a dry world's runs are identical (`--ticks 5000` still 1,329), and the shelf stays valid. `test_water_solvent` checks the spread, the uptake and the draws.
  - *Judged on one run (seed 42, `--sea 12`, the stage-3 laws above plus `WET_SPREAD=2 WET_UPTAKE=1`, 100,000 ticks; `ab_out/water2/seed42`), against the stage-3 run (`ab_out/water3/seed42`), mean of the last fifth:* population 1,480 → 2,205 (+49%), established species 75 → 102, hunters 70 → 136, uptake from loose chemicals +58%, bodies under water 55 → 94; walkers 165 → 36 and movers 186 → 85. Swimmers still do not take hold (1 a world). Loose B and C fall (13 → 5, 17 → 12): they are eaten, not piling up. Bodies under water are still no bigger (radius 1.25 against 1.50 on land). Worst audit error 6.8e-7. **Reading:** the solvent feeds the whole world, not the sea in particular: richer, more diverse, more hunting, fewer movers. The sea gains bodies but still no swimmers. One run, one seed: not a verdict. Whether brains read depth was not measured (no creature snapshot in this run).
- **Holding up its weight (2026-11-13; law `SUPPORT_COST`, 0 = off):** every body pays `SUPPORT_COST` × mass^(4/3) a tick, booked with upkeep (`support_cost`, `energy.cpp`; added to `cost_upkeep_` in phase 1, not lowered by sleep). Per unit of mass a big body pays more than a small one, and under water `BUOYANCY` of it is carried. It is what gives buoyancy something to do: before it, buoyancy only cheapened moving, which is 0.05 to 0.5% of all energy spent. With the law at 0 runs are identical (`--ticks 5000` 1,329). `test_water_motors` checks it. Not in the designer's upkeep bill yet.
  - *Judged on one run (seed 42, `--sea 12`, the stage-2 and stage-3 laws above plus `SUPPORT_COST=0.015`, about 5% of a LUCA's upkeep and 17% of a radius-2 body's; 100,000 ticks; `ab_out/water_support/seed42`), against the solvent run, mean of the last fifth:* bodies shrink on land (radius 1.50 → 1.35) but not into size in water (1.25 → 1.17). Population 2,205 → 1,612 (−27%), species 102 → 89, hunters 136 → 81, movers 85 → 137, walkers 36 → 68, swimmers 1 → 0.25, bodies under water 94 → 84. Worst audit error 8.8e-7. **Reading:** a land tax on size works, but the sea is still too poor to feed big bodies, so buoyancy has nothing to pay off against. The "bigger in water" judge fails. What the sea lacks is food, not cheap weight: a sunlit shelf (a shallower sea or a larger `LIGHT_DEPTH`) or sinking (stage 4) come first.
- **Food for the sea (2026-11-13, Uday's call: no more light; feed the sea through chemistry and matter moving instead).** Three rules, all off by default, so dry worlds and the shelf are unchanged (`--ticks 5000` 1,329):
  - **Vents from the start** (config `sea_vents`, `--sea-vents N`, default 0): when a world with a sea is made, N hot vents are laid on random underwater cells (strength 1.5 to 3, radius 1.5 to 3; up to 64 tries each), keyed by which one (`Draw::SeaVents`, `Simulation::lay_out_sea_vents`). They stay open. Saved like any vent (the vent layer) and in the config.
  - **The dead dissolve under water** (law `WET_DISSOLVE`, 0 to 1): of what weathers off a corpse under water, that share becomes loose chemicals in the cell, energy kept (mostly D), instead of A and heat. Every plant burns D, so any skin under water can take it up (`weather_corpses`; metric `dissolved_dead`).
  - **Runoff and sinking** (laws `RUNOFF` on land, `SINKING` under water): every 8 ticks a cell sends that share × its steepness (at most 90%) of its loose chemicals (with the vent tracer and the leavings tag) and of its corpses (with their P) to its lowest neighbour (`World::downhill`), so what the land loses runs down into the sea and settles in hollows. Corpses move one cell a pass (`corpse_slides_`). Metric `moved_downhill`.
  - Both books stay exact: energy and matter only move. `test_sea_food` checks the vents (only under water, only with a sea, the same each time), dissolving and runoff, each with the energy audit.
  - *Judge run started 2026-11-13 (seed 42, `--sea 12 --sea-vents 6`, every water law on, 100,000 ticks; `luca/ab_out/sea_food/seed42`), against the support-cost run; results not yet written here.*
- **Hot vents open only under water (2026-11-13, Uday):** the world's own vents. A new event kind, `SeaVent` (4 per 100,000 ticks at rate 1), opens a vent of strength 1.5 to 3 and radius 1.5 to 3 on a random underwater cell (up to 32 tries; nothing happens if there is no sea or none is found), over 300 ticks, and closes it after 30,000 to 60,000 ticks. It is the last kind, so the draws that pick storms, eruptions, meteors and drifts are unchanged. An eruption opens a vent only when its crater stays under the sea after the cone rises ("A volcano rises from the sea floor ..."); on land it is a hill and hot rock only. So with no sea, the world opens no vents at all. Player-painted vents (the brush) are unchanged. The sea-vent count is saved in `WATR`, so older snapshots load. `test_powers` checks: a land volcano has no vent, a sea vent needs a sea, opens only on underwater cells and closes again, and an undersea volcano opens one. A 30,000-tick run with `--sea 12 --events 4` opened three sea vents; audit 2.2e-7.
- **Vents (2026-11-12; a chemical from below since chemistry stage 3):** a world layer (`World::vent`, strength 0 to `vent_max` 3) painted by the `vent` brush power (open +, close −), like height and heat. It warms its cell by `vent_heat` (12 °C) per unit of strength. Each tick, day and night, a vent cell brings up loose C (`VENT_CHEM`, `energy.hpp`) from below, `vent_power` (1.0) × strength energy's worth (1/148 of a unit a tick at strength 1), whatever its soil holds (`Simulation::run_vents`). Energy and matter are both new: the energy is booked in `energy_in` and broken out as `in_vent` (the third way energy enters the world), the matter as `matter_in` (the only way matter enters; `Simulation::matter_brought_up()` is the running total). What nobody eats breaks down to A where it drifts, so the ground around a vent grows rich. (For a few hours on 2026-11-12 the vent made C out of its own cell's A; see 2.1.1 stage 3 for why that was dropped.) There is no gene: bodies that can burn C take it up through their skin like any loose chemical, and bodies that can't are fouled by it. The `chemo` gene, `CHEMO_RATE`, `CHEMO_UPKEEP` and `vent_demand` are gone.
  - **Telling vent food apart (observation only):** `Soil::vent_made` counts how much of a cell's loose C a vent made. It drifts and breaks down with the rest, and uptake takes from it in proportion, so C taken up on or near a vent counts as `fed_vent` and the rest as `fed_waste`. The census's **chemotrophs** (over 60% of food from vents) and the species card's diet still work. Metrics column `vent_loose` is the world's total.
  - With no vents painted, the sim skips the vent pass (`World::any_vents`).
- **Meteors: creatures from outside (2026-11-12, 4.9; `Simulation::land_meteor`):** the `seed` power drops `count` (1 to 500) bodies from a creature file within `radius` of a spot, on a sunflower spiral, cycling through the file's genomes. Each is clamped, compiled and lands grown, with a store worth 60% of its breeding threshold made of the richest chemical it can burn (a plant arrives full of D, a vent dweller of C) and a body of D. That energy and matter come from outside: booked as `energy_in` (broken out as `in_meteor`, metrics column `in_meteor`) and `matter_in` (`matter_brought_up()` includes it). They all join one species: the nearest living species if the founder is close enough, otherwise a new root species named after the file, with one lineage. The chronicle says where it fell. Cast between ticks (the viewer while paused) it lands at once and is booked in the next tick's stats. A meteor touches no creature already there. Files: `core/creature_file.*`; version 1, `luca-creature 1`, then `name`, `origin` and one `body` block per genome; unknown keys are an error, and every genome is clamped on reading.
- **Slope shade (2026-11-11):** the sun stands over the y = 0 edge. A cell loses light for dropping away from the sun (15% per unit of height per cell, up to 50%) and for lying below its rim, the ground three cells out (6% per unit of depth, up to 40%). `World::shade()` is a static field, recomputed around a terrain brush stroke only; `slope_shade` in the config scales it (0 = off). On the default map the mean is 0.94 and the darkest cell 0.53.
- **Matter washes downhill (2026-11-11):** every 64 ticks a cell sends `wash_rate` (0.001) × steepness of its free nutrient to its lowest neighbour (`World::downhill()`, eight neighbours; steepness is the drop over 2 height units, capped at 1). Corpse matter stays put until it is freed. All taking is worked out before any giving, and `total_matter()` is unchanged.
- **Day and night:** `World::set_time(tick)` runs at the start of every tick, after powers. Daylight is a pure function of the tick (no randomness), 0 at night to 2 at noon.
- **Cold nights (2026-11-11):** the air everywhere swings by `temp_day_swing` (10 °C, peak to peak, in `config.hpp`; 0 = off) with the day, a sine an eighth of a day behind the sun: warmest mid-afternoon, coldest just before dawn, daily mean unchanged (`World::air_swing()`; `temperature()` and `env()` include it, `temperatures()` and `mean_temperature()` are daily means). No creature feels the air directly: each carries a `body_temp` that closes `heat_follow` of the gap to what it would feel (air plus sun on dark skin) each tick, where `heat_follow = 1 / (1 + HEAT_LAG × mass / skin)` (`HEAT_LAG` 120 in `phenotype.cpp`). A LUCA lags by ~130 ticks and feels about 60% of the night's chill; a radius-2 body about 30%. Thermal cost, the dormancy cold trigger and the brain's cold input all read body temperature. A child starts as warm as its parent. Not a new gene: size and fat already cost upkeep.
- **Contact:** a body with an absorber looks at the 3×3 cells around it. For each body within reach, energy flows at `edge × pull² / (pull + membrane)`, where `edge` is (taker radius / giver radius)², at most 2 (the size rule, 2026-11-11: equal bodies are unchanged, a body half its victim's size pulls a quarter as hard, and nothing is ever immune); the taker keeps 70% of what it can burn and the rest spills into its cell (since chemistry stage 2; before that the 30% was lost as heat. Most of the spill is taken up again: 14.2.2). If several bodies pull on one, each gets the same fraction. Reach is capped at 1.5 cells, so very large bodies don't yet touch further.
- **The dead in reach (adopted 2026-11-13; law `DEAD_IN_REACH`, 1):** a mouth also pulls on the corpses of the eight cells around it when the nearest point of that cell lies within its reach (its skin, or a stalk that way, capped at 1.5), with the pull it has towards the cell's middle, at `CORPSE_RATE` × that pull. A cell's corpses are shared in proportion among everyone asking, own cell included. What the eater keeps goes to its store; the rest spills into its own cell. Before, a kill's corpse lay out of its killer's reach (14.2.3).
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
- **Wander drifts (2026-11-11):** the wander output pushes towards each creature's own wander angle (`Creatures::wander`), which drifts **by distance travelled, not by time**: each tick it takes a normal step of sqrt(2 × stride / `WANDER_PATH`) radians, where stride is how far the body is about to go (`WANDER_PATH` 1.0 world unit, in `sim.cpp`). So a wanderer's path stays fairly straight for about a world unit whatever its speed. A newborn's angle comes from its id, and each step from a key of the mover's id and the tick (`Draw::Wander`).
  - The history of the old spinning wander, and why it had held the world together, is in [docs/history.md](docs/history.md) (10.4).
  - **Mutation:** nudge weights; add a link; lose a link; grow a neuron in the middle of a link (the old link stays); lose a neuron with its links; nudge a neuron's tau (by a factor of e^(0.5 × normal), `P_TAU`, `TAU_STEP`) or its bias (`P_BIAS`). A new neuron gets a tau anywhere from 1 to `MAX_TAU` (1,000), evenly in log, so fast relays and slow memories are equally likely to appear. Rates are the `P_*` constants in `mutation.cpp`. The species distance counts tau (`W_TAU` 0.05 per e-fold) and bias (with the weights) for neurons both brains have.
  - **Cost (nearly free since 2026-11-12, Uday's call):** upkeep per hidden neuron (`NEURON_COST` 0.00012), per link (`LINK_COST` 0.00002), per unit of weight or bias (`BRAIN_COST` 0.0001), and per e-fold of each neuron's tau (`MEMORY_COST` 0.00002), all in `phenotype.cpp`. Each is a hundredth of what it was (0.012 / 0.002 / 0.01 / 0.002), so a full brain (8 neurons, 48 links, weights near 1) costs about 0.007 a tick, about 2% of a LUCA's upkeep. The costs are not zero, so unused wiring still drifts away slowly, and `MAX_HIDDEN` and `MAX_LINKS` cap the size. A brain on a body that cannot move costs nothing and does nothing.
  - **Layout:** `compile_brain()` turns the genome into a fixed-size `BrainPlan`; `think()` runs it. Plans and hidden state live in their own arrays in `Creatures`, off the hot path; only movers read them.
- **Danger / kin sense (2026-11-11):** three brain outputs, felt only with a sensor. Each body within two cells pulls or pushes the creature by `OUT_KIN` if it is the same species, `OUT_BIGGER` × how much bigger it is (negative when smaller), and `OUT_MOVING` × how fast it moved last tick; nearer and heavier bodies count for more. Nothing says which bodies are enemies: the weights start as noise and evolve. It walks the spatial grid, so the cost is bounded by local crowding. **This is the one rule that reads species ids**, so `SPECIES_THRESHOLD` and the distance weights now touch behaviour.
- **Moving is paid per distance as well as per speed (2026-11-11):** `movement_cost` is mass × (`TRANSPORT_COST` 2.0 × distance + `MOVE_COST` 3.0 × distance²) a tick, plus climbing. Before, only the squared term existed, so covering a distance at speed v cost mass × 3 × v × distance: creeping was nearly free and the world's movers were slow drifting plants. Now creeping a cell costs about two-thirds as much as walking it.
- **Senses are cheap (2026-11-11):** sensor upkeep is 0.05 per unit of size (it was 0.3, the dearest part there was; seeing one cell cost about 40% of a LUCA's upkeep, and nearly every mover was blind).
- **Optional traits (`seed_throw`, `bite`, `dormancy`)** stay at exactly 0 until a rarer mutation switches them on (`P_NEW_TRAIT`), and are lost again below a sliver. A plain nudge bounced off zero and left every lineage paying upkeep for a little of each.
- **Sunbathing and moving conflict (2026-11-11):** a body's light demand is multiplied by `light_while_moving(moved)`: 1 if it sat still last tick, falling in proportion to 0.2 at a stride of 0.005 world units or more (`MOVING_SHADE` 0.8, `FULL_STRIDE`, in `energy.cpp`; 0.08 until wander began to drift, 0.02 until wander turned by distance, see 10.7). It applies before the canopy shares light out.
- **Folding (2026-11-12, Uday's call; current):** a body that moves more than `FOLD_STRIDE` (0.001) in a tick folds its sun patches shut, and they take `UNFOLD_TICKS` (600, a day) to open again, catching light in proportion (`Creatures::unfold`, `next_unfold` in `energy.hpp`; it multiplies light demand before the canopy shares light out). A creeper that moves every few ticks stays folded; a plant that moves once to get away loses part of a day's light. It replaced the half-sun rule below the same day: in six seeds it gave the fullest and most diverse worlds measured, with no creepers (10.7). `test_folding` checks it.
- *Movers caught half the sun (`MOVER_SUN`) for a few hours on 2026-11-12, before folding replaced it; deleted in step 0.5. History in [docs/history.md](docs/history.md).*
- **Dormancy (2026-11-11):** each creature has a `sleep` level, 0 awake to 1. With the `dormancy` gene it climbs by 1/20 a tick, up to the gene's value, whenever its cell is dark or colder than its comfort range, and drops by 1/90 a tick otherwise (quick to sleep, slow to wake). At sleep `s` a body pays (1 − 0.9 s) of its upkeep and thermal cost, catches (1 − s) of its light and pulls (1 − s) as hard on the living and the dead. Above 0.1 it cannot move or breed; at 0.5 or more it ages at an eighth of the usual pace. Its membrane is unchanged, so it can be eaten in its sleep. Rules are `next_sleep` and `cost_while_asleep` in `energy.cpp`; `dormancy` is an optional trait like `bite`.
- **Poison (2026-11-11):** a new part type. A body's `toxicity` is its total poison size, capped at 2. Whatever drains it loses `toxicity` × the energy drained, as heat (`out_poison` in `TickStats`), paid out of what it holds, so a poisonous meal can cost more than it gives (a meal returns 70%). Poison is light (0.1 mass per unit) but dear to keep (0.12 upkeep per unit), and unlike armour does not slow the bite. It works on living bodies only: a poisonous body's corpse is safe to eat. Nothing can sense it yet, so eaters cannot learn to avoid it; they are only selected against.
- **Gene transfer (2026-11-11):** each tick that a body is draining another, there is a `gene_transfer_chance` (0.0001, in `config.hpp`; 0 = off) that it takes up the blueprint of one of that body's parts, picked at random, as a half-size bud (`transfer_part` in `mutation.cpp`; whether, and which part, from a key of the taker's and giver's ids and the tick, `Draw::Transfer`). "Touch" here means the existing contact rule, so only bodies with a mouth take anything up. The blueprint goes into the taker's **genome only**: its own body and stats do not change, and its children are built from the new plan and pay for the part. So a living body's genome can now hold parts its body lacks (the inspector's parts list reads the genome). `TickStats` counts transfers, cross-species ones and ones that bring a kind of part the taker lacked; the chronicle logs the first cross-species one.
- *Waste (2026-11-11, with the `detox` gene) was replaced by loose chemicals in chemistry stage 2; its rules are in [docs/history.md](docs/history.md).*
- **Wear:** upkeep is multiplied by `1 + wear × (age / AGE_SCALE)²` (`AGE_SCALE` 10000). Nothing dies of old age as such; an old body costs more than it earns.
- *Repair and copying fidelity (the `repair` and `mutability` genes, 2026-11-12) were deleted the same day after the ablation pass found they changed nothing (14.10.8). Old creature files with `repair` or `mutability` lines still read (the lines are dropped); snapshots keep two retired floats per genome so the format is unchanged.*
- **Skin colour (2026-11-12, Uday's request; `core/colour.hpp`):** skin colour comes from the `hue` gene (where on the colour wheel) and `pigment` (how dark), always muted and pulled towards tan (`SKIN_CHROMA` 0.28, `SKIN_EARTH` 0.4: Uday wants earthy tones, nothing that stands out like a sore thumb). It is drawing only: no sense reads it. Hue mutates by `HUE_STEP` (0.08, `mutation.cpp`) at `P_GENE` and drifts. Species distance counts it (`W_HUE` 1.0 per turn); the chronicle says "It is bluer" and so on; the snapshot has a `hue` column; the inspector names the skin; the designer has a hue slider. *Camouflage (others sensed a body by how it showed against its ground's colour) was built the same day and removed in roadmap step 1 (14.8): the lean towards the living reads `biomass_` and the danger / kin sense weighs bodies by mass alone, as before it.*
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
- **Determinism:** same seed + build + machine → identical runs (verified). Cross-machine determinism is not a goal yet; creature files don't need it. **Draws are keyed (2026-11-12, 14.10.4):** the sim keeps no running random stream. Each draw is made from `draw_key(draw_seed_, purpose, tick, id, id)` (`core/rng.hpp`), with an `Rng` seeded from the key when more than one number is needed: the first LUCAs' places (`Draw::Start`, by index), a mover's wander step (`Wander`, its id), a gene transfer and which part (`Transfer`, taker and giver ids), a birth's mutations and landing (`Birth`, the parent's id: a body breeds at most once a tick), windfalls (`Windfall`, one key a tick), and world events (`Event` a tick, `EventTrigger` by hand, keyed from the events' own seed). So no draw depends on how many came before it or on where a body sits in the arrays. Two things still do depend on array order, and a re-sort would change runs through them: new ids are handed out in the order births are found, and per-cell sums (light demand, biomass, uptake shares) add floats in body order.

### 10.5 God powers (built 2026-11-11)

- **Seven powers so far:** `terrain`, `heat` and `vent` (soft round brushes), `sun`, `climate` and `sea` (global; `<tick> sea <amount> [duration]`), and `seed` (drop creatures from a creature file by meteor; it adds bodies and never touches existing ones, and is logged and replayable like the rest). A vent line in a script is `<tick> vent <x> <y> <radius> <amount> [duration]`. Script format is documented in `core/powers.hpp`.
- **The world acts on its own (2026-11-12, 4.7, `core/events.*`):** `WorldEvents` runs at the start of every tick, before the powers. At `event_rate` 1 (`config.hpp`, default 0; `--events`), one draw a tick decides whether an event starts. In 100,000 ticks that gives about 12 storms, 2 eruptions, 2 meteors and 4 climate drifts (`PER_100K`), one every 5,000 ticks. Each event is a handful of ordinary power commands, marked `quiet`, so the chronicle gets one line from the event instead of one per command, plus a closing line when it ends. Where they land is uniform over the map.
  - **Storm:** a cold spot 6 to 14 °C deep and 10 to 22 units across. It rolls in over 200 ticks, sits for 600 to 2,400 and rolls out over 400. The ground ends exactly as it was.
  - **Eruption:** a cone 8 to 18 high and 4 to 7 across rises over 300 ticks and stays. With it come 15 °C of heat and, only if the crater stays under the sea, a strength-2 vent (since 2026-11-13), which fade together over 20,000 to 40,000 ticks.
  - **Sea vent (2026-11-13):** a hot vent on a random underwater cell, open for 30,000 to 60,000 ticks; only with a sea (10.4 "Hot vents open only under water"). A cone that steep blocks births and walkers like a raised mountain. Eruptions release no matter: the matter books have no way in from outside.
  - **Meteor:** a crater 2.5 to 6 across and 2.5 × its radius deep, with a rim, and a 30 °C flash that cools over 3,000 ticks. One in five (`GREAT_METEOR_CHANCE`) also dims the sun by 0.25 over 200 ticks, clearing over 15,000 to 40,000: an impact winter.
  - **Climate drift:** the whole world warms or cools by a normal step (sd 1.5 °C, at most 3) over 10,000 to 30,000 ticks. Each step also undoes 30% of the offset so far, so the climate wanders but comes back.
  - **Dice:** keyed from the events' own draw seed (made from the world seed) and the tick (`Draw::Event`; `Draw::EventTrigger` for one started by hand), not from the sim's. So rate 0 changes nothing, and `powers.txt` from a run with events replays it exactly with `--events 0` (tested; with events on as well they would happen twice). `Simulation::trigger_event` starts one of a kind now, for tests.
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
- **Inspector** says whether a body can live on what vents make (it burns C), and writes two-input enzymes as `A+A<->D+D` . It lists each part with its size, where it sits (front, left, back-right, ...) and its stalk, and says whether the body is round, oval, long or boxy. Each hidden neuron gets a line with its tau and how it is firing right now (a bar, + or −), updated as the creature moves.
- **Day and night** tint the whole picture. When days pass faster than about one a second, the tint holds a steady dusk instead of strobing.
- **Stats panel:** day and clock, population, the census (sun-fed, hunters, scavengers, cleaners, vent-fed, mixed diet, able to move), and the six most numerous species with their colours.
- **Colour by species** (checkbox or `C`, `--species` on the command line): skin is painted in the species colour instead of the pigment. The hue rides in the unused height slot of each instance's origin.
- **Overlays:** natural, temperature, light, nutrients, the dead, waste, vents, mineral P (stoichiometry; `--overlay=N` opens on overlay N). In the natural view, heavily fouled ground turns murky olive, and a vent darkens the ground to basalt with orange and yellow cracks that grow denser with strength and glow through the night. Vent strength rides in the alpha channel of the soil texture. `--vent=x,z[,strength]` opens a vent before `--warmup`, for screenshots.
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
- **Save, load and scenarios (2026-11-12, roadmap step 0b):** a GAME panel at the top of the left column has Save world and Load world (Godot's file dialog in `user://saves`, on macOS `~/Library/Application Support/Godot/app_userdata/Ecosys/saves`; the same `.snap` files as `luca_headless --save`, so a headless world opens in the viewer and back), and a scenario picker with Start scenario. Loading a world or starting a scenario reloads the scene with the new world (static `pending_load` / `pending_scenario` in `main.gd`), so every texture and panel starts clean; after the first reload the command line no longer applies. A scenario starts a fresh 128² world of 200 LUCAs (the setup its goal was measured on) with new dice and world events off. A banner at the top centre gives the goal, progress ("3 of 10 movers") and ticks left; once it is decided it shows the verdict in green or red, pauses the world once, and offers Play again or Keep playing (sandbox). While a scenario that forbids it runs, releasing a creature is refused (in `_start_drop` and in `LucaWorld::release`). Glue: `save_world`, `load_world`, `get_scenarios`, `start_scenario`, `get_scenario`. Command line: `--load=PATH`, `--scenario=KEY`. Checked by screenshots: a scenario start, a headless save of seed 9 at tick 9,940 opened in the viewer and won at 9,950, the Start scenario reload, and a viewer save loaded back through the reload. The file dialogs were not clicked through by hand.
- **Food web** (`G`, or the button; `viewer/food_web.gd`; 2026-11-12, 4.3): drawn in the tree's place from energy that actually moved. The sim keeps a ledger (`FoodLedger`, `core/foodweb.*`, observation only, no `Rng`): every meal from the living is booked by taker and giver species, uptake of tagged leavings by taker and leaving species (the leavings tag), and the rest by source (sun, the dead, vents, untagged loose chemicals). It fades by 2% every census (a half-life of about 1,700 ticks). `food_web()` turns it into nodes and edges and works out each species' trophic level: sun and vents are 0, the dead and leavings 1, and a species sits one above the energy-weighted mean of what it eats. Sources stand in a column on the left; species are placed by level; edges are coloured by kind and thick by energy; flows under 3% of a species' intake are hidden unless it is selected. Hover for a description, click to select (the species card follows), wheel to show more or fewer species, Esc or `G` to close. Data from `LucaWorld.get_food_web(min_alive)`; `--web` opens it at start. `test_food_web` checks a plant and its eater.
- **Not done yet:** position interpolation between ticks (at 1x one tick is one frame, so little is lost), sim on its own thread, a pixel font for the UI.
- **Creature designer and seed vault (2026-11-12, 4.10):** `B` (or the LIFE buttons) opens the designer over everything and pauses the world; closing it restores the speed. `V` opens the seed vault, a folder of creature files at `user://creatures` (on macOS `~/Library/Application Support/Godot/app_userdata/Ecosys/creatures`). The inspector has "Edit in designer" (the creature's own genome) and "Bank its species"; the species card has "Bank in the seed vault" (a sample of up to 20 living members, `bank_species`). Release (designer or vault) hands the file back to the world: the pointer becomes an orange ring, a click drops them by meteor, Esc cancels. Undo/redo (Cmd+Z), Cmd+S saves, Delete removes what is picked, V / P / M / 1 to 8 pick tools on the Body tab. Data comes from `LucaWorld.assess_design`, `clamp_design`, `design_sprite`, `probe_brain`, `write_creature_file`, `read_creature_file`, `bank_species`, `genome_of`, `release` and the `trial_*` calls (`godot/luca_design.cpp`). Command line: `--designer[=TAB]`, `--template=NAME`, `--vault`, `--bank` (banks the most numerous species at the start).
- **Checking the viewer:** `./view.sh --warmup=40000 --zoom=8 --at=100,24 --frames=40 --shot=/tmp/x.png` skips ahead 40,000 ticks, zooms in, saves a screenshot and quits. Keep `--frames` small: macOS throttles a window that is behind other windows to a few frames a second, so a long run looks like a hang.

### 10.7 Baseline numbers (default settings, 128×128, seed 42, measured 2026-11-11)

**After keyed draws (2026-11-12, roadmap step 0.3; the new baseline). Every run changed once, as planned: the same seed now rolls different dice. Six seeds 42 / 7 / 99 / 1 / 2 / 3, 100,000 ticks, no vents, mean of the last fifth, against the build before it (camouflage, the last commit) run the same way the same day:**
- `--ticks 5000`: **978 creatures** (880 before), 0.060 ms/tick, worst audit error 1.25e-7. *Since neutral exchanges, `repair` and `mutability` were deleted (2026-11-12): 986 creatures, audit 1.3e-7 (mutation draws changed); the six- and 30-seed numbers below are from before.*
- **Population and species:** 3,060 creatures (3,020) and 83 established species (68); hunters 88 (81), scavengers 16 (14), mixed feeders 315 (275), movers 38 (13).
- **Per seed:** 3,950 / 2,900 / 2,610 / 3,800 / 3,200 / 1,920 creatures (before 2,640 / 3,440 / 3,010 / 2,620 / 3,750 / 2,650); 97 / 65 / 83 / 106 / 88 / 61 species (49 / 83 / 70 / 64 / 75 / 65); lowest after the first tenth 2,190 / 1,550 / 2,000 / 1,330 / 1,630 / 1,340 (1,460 / 1,910 / 2,050 / 1,400 / 1,640 / 1,630). Worst audit error 4.8e-7.
- **Nothing in the change should shift a mean:** every draw has the same distribution as before, only different values. The differences are the luck of the seed: each seed moved by up to 1,300 creatures and 47 species, in both directions, while the means moved by 40 and 15. Movers came out higher in all six seeds in the last fifth, but over the whole run there is no direction: movers boom and bust in every seed (0 to 150 a world), and the old dice had more of them between ticks 20,000 and 60,000; the last fifth caught the new ones mid-boom. Species came out higher in four seeds of six. Recheck both on the 30-seed shelf (step 0.4).
- **Speed:** no difference that shows through the noise of six runs at once (0.16 to 0.18 µs a creature a tick in both).

**Sensor brain baseline (2026-11-13; the current one): the rebuilt shelf, 30 fresh seeds (`THIRTY`), 100,000 ticks, mean of the last fifth** (`luca/worlds/runs/plain_s*_from0/metrics.csv`; default laws, no sea, no vents, no events):
- `--ticks 5000`: **1,329 creatures** (1,352 with `SENSOR_BRAIN=0`), 0.095 ms/tick, worst audit error 1.7e-7. Unchanged by the water work of 2026-11-13, which touches only worlds with a sea.
- **Population and species:** 1,970 creatures (1,410 to 3,150 by seed) and 96 established species (68 to 127).
- **Ways of life:** hunters 102 (51 to 216), mixed feeders 933, scavengers 11, movers 199 (47 to 402). Mean radius 1.39.
- **Brains:** 1,540 running brains on rooted bodies, 246 with a sensor point (`eyed`), 244 with a side-contrast → turn link (`steering`).
- Lowest population after the first tenth, any seed: 978 (seed 28). Worst audit error 1.0e-6.
- **The six usual seeds** (42 / 7 / 99 / 1 / 2 / 3): 2,360 / 2,430 / 1,490 / 1,820 / 1,980 / 2,060 creatures; 113 / 127 / 80 / 86 / 80 / 79 species; movers 62 / 187 / 346 / 206 / 222 / 47. Means 2,020 creatures, 94 species, 99 hunters, 179 movers.
- **Against the step-2 baseline (old brain, six seeds, below):** about the same number of species (94 against 95), a smaller world (2,020 against 2,440), fewer hunters (99 against 150) and fewer mixed feeders, and twice the movers (179 against 89). Against the step-1 30-seed baseline (before light over the footprint and the dead in reach): fewer creatures (1,970 against 2,885), more species (96 against 70), movers 199 against 26.
- ms/tick 3.5 (0.9 to 6.4), with eight runs at once and other work on the machine: not a speed baseline.

**Step-2 baseline (old brain; taken 2026-11-13): light over the footprint and the dead in reach on by default, neutral exchanges, `repair` and `mutability` deleted.** Six seeds 42 / 7 / 99 / 1 / 2 / 3, 100,000 ticks, no vents, mean of the last fifth (`luca/ab_out/baseline_step2/`), against the same seeds with both laws off (`ab_out/step2_fresh/control`):
- `--ticks 5000`: **1,352 creatures** (986 with both off), 0.090 ms/tick, worst audit error 1.7e-7.
- **Population and species:** 2,440 creatures (2,735 off) and 95 established species (60).
- **Ways of life:** hunters 150 (72), mixed feeders 1,221 (265; now half the world), scavengers 9 (12), movers 89 (9; 4 to 317 by seed). Mean radius 1.40 (1.04).
- **Per seed:** 1,610 / 2,930 / 2,280 / 2,470 / 2,730 / 2,620 creatures; 68 / 115 / 89 / 81 / 106 / 111 species; lowest after the first tenth 1,280 / 670 / 1,880 / 1,930 / 1,780 / 1,650. Worst audit error 2.6e-7.
- **Reading:** a world of bigger, mixed-feeding bodies. Most eating of the dead now goes to mixed feeders rather than scavengers, which fall a little. Movers boom in some seeds (317 in seed 2) and stay rare in others.
- ms/tick 1.1 with six runs at once (0.83 with both off, the same way): about +40%. Not a speed baseline.

**Step-1 baseline (2026-11-12, after camouflage and the sparse tick; before the deletions of step 0.5): 30 fresh seeds (`THIRTY`), 100,000 ticks, mean of the last fifth** (`luca/ab_out/baseline_step1/`):
- 2,885 creatures (1,930 to 3,830 by seed), 70 established species (33 to 105), hunters 73, scavengers 14, mixed feeders 266, movers 26 (3 to 89).
- Lowest population after the first tenth, any seed: 745. Worst audit error 2.4e-7.
- ms/tick 1.8 (0.6 to 4.4), measured with 8 runs at once on a hot fanless laptop: not a speed baseline. The 10,000-creature measurement still waits for a quiet machine.
- Close to the ablation pass's control (2,798 and 68), as expected: camouflage was read by few bodies.

*Older measurements, from chemistry stages 3 and 4 back to the first build, are in [docs/history.md](docs/history.md).*

## 11. Work split and next steps

**Split, as changed on 2026-11-11:** Uday asked Claude to finish the energy rules and mutation and to extend the phenotype compiler so that creatures could move, eat and evolve. Those files are no longer off limits. They are first guesses that give a living world; **Uday owns the tuning** and may rewrite any of them.

### Good places for Uday to tune (all are single constants)

Since 2026-11-12 nearly every float constant below is a **law** (`core/laws.hpp`; `luca_headless --list-laws` lists all 122): try a value with `--set NAME=VALUE` without rebuilding, and change the default in the `LUCA_LAWS` list in `laws.hpp`, not in the file that uses it (that file keeps the name and the comment, with the default noted as `// law, 0.75f`). Not laws yet: the per-part tables `MASS_PER_SIZE`, `COST_PER_SIZE` and `P_PER_SIZE`, integer constants, `LEVEL`, the gene limits (`limits::`), and everything in `config.hpp` (set by its own flags).


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
- Movers' sunlight: `MOVING_SHADE`, `FULL_STRIDE` (`energy.cpp`); folding `UNFOLD_TICKS` (0 = off), `FOLD_STRIDE` (`energy.hpp`).
- Growing up: `GROW_STEP`, `GROW_MIN`, `GROW_RESERVE` (`sim.cpp`); LUCA's `birth_size` (`genome.cpp`); `limits::MIN_BIRTH`.
- Stoichiometry (all laws; off until `P_BASE` > 0): `P_BASE`, `P_PART_SCALE`, `P_STORE`, `P_UPTAKE`, `P_WEATHER_SHARE`, `P_MEAN`, `VENT_P`; the table `P_PER_SIZE` (`phenotype.cpp`).
- Vents: `vent_power`, `vent_heat`, `vent_max` (`config.hpp`); `VENT_CHEM` (`energy.hpp`, C by Uday's call); `VENT_BRUSH` (`viewer/main.gd`, how fast the brush paints).

### Known weak spots

*Review 2026-11-12: several entries below share a cause. "Bodies still shrink", "most cells hold one body" and "every plant is a frond" come from light being caught in one cell only (14.2.1). "Most corpse energy still weathers away uneaten" comes from mouths not reaching the dead in the next cell (14.2.3). The far-edge vents that "stay empty" sit in the dead half of the map made by light and warmth falling together (14.2.4).*

- **Eaters keep more of a meal than `TRANSFER_EFFICIENCY` says (14.2.2; kept as it is, Uday, 2026-11-13).** An eater keeps 70% of what it can burn and spills the other 30% into its own cell as loose chemicals, then takes most of that spill back up through its skin, so in practice it keeps 85 to 90%. The spill is a large part of what hunting pays: with `SPILL_LOST=0` (the part it could burn but didn't keep is lost as heat) forks of the shelf lose 43% of their hunters, 72% of their mixed feeders and nearly all uptake (review, "Step 2 fork screen"). Levers when it matters: `SPILL_LOST=0` with a higher `TRANSFER_EFFICIENCY` (so the share kept is honest and set in one place), or spilling into the cell the meal came from rather than the eater's.
- **Matter piles up around vents (chemistry stage 3).** *Review 2026-11-12: the mantle reservoir (14.10.7) is the proposed fix when it matters.* Vents bring up new matter, and nothing takes matter out of the world. With the four test vents, total matter goes from 98,900 to 225,000 in 100,000 ticks (1.26 units a tick), and the soil under each vent reaches about 400 units of A a cell, rising steadily. Life doesn't seem to mind yet (A is spent matter: it costs light to use), but nothing bounds it. Options in 2.1.1 "Open after stage 4". Uday chose to leave it for now (2026-11-12).
- **Vent dwellers are slow and patchy:** two seeds of six by tick 100,000. The far-edge vents stay empty, as they always have (life never reaches them).
- **Neutral exchanges and cross-feeding are unused (stage 4).** Two-input enzymes spread mostly as variants of A ↔ D. Loose B and C stay scarce, so a body living on them has little to live on.

- **Chemistry stage 2 emptied the world a little** (2,480 creatures and 57 species, against 3,230 and 71 in stage 1). Spilled meals feed the eater and its neighbours, so hunting pays better and hunters rose to 61. Levers, all Uday's: `UPTAKE_RATE` (lower slows how fast spills are taken up, so more decays), `WASTE_DECAY`, `TRANSFER_EFFICIENCY`, or spilling only part of the lost share and burning the rest. Without a crowding cost from fouling (only chemicals a body can't burn foul it), canopy shade is the main limit on crowding again.

- **Chemistry stage 1 thinned movers and hunters** (19 and 39 a world against 75 and 70), probably through the power cap on moving; seed 99 fell to 430 before recovering. B and C are carried by a few bodies but have no use until chemicals can leave a body other than by being eaten (stage 2). The inspector's "soil" is now chemical A.

- Scavengers stay rare and movers are a small minority; hunters are mostly sessile mixed feeders. The decomposer and chase/flee niches exist but are thin.
- *Since light over the footprint (2026-11-13), size pays: on six fresh seeds the spread of radius doubles and 15% of bodies pass radius 1.5. The entry below is from before.*
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
- *Since the dead in reach (2026-11-13), eaten corpse energy is 2.3 times what weathers on six fresh seeds (0.59 before). The entry below is from before.*
- Most corpse energy still weathers away uneaten (about 70% in the last 50,000 ticks of a 300,000-tick run), because eaters must be in the same cell and few bodies move.
- The `seed_throw` gene has no drawing in `blob.gdshader` (it is a gene, not a part); the inspector shows it.
- Kin are eaten like anyone else; there is no self-recognition.
- **Movers are drifting plants, not animals.** A probe of every mover over 300 ticks (seeds 7, 42, 99) finds top speeds of 0.01 to 0.03 a tick, almost none with a sensor, and brains that are a single fixed wander urge. Moving pays only as slow drift to new ground. Cost per distance and cheap senses (built since) moved this a little: movers now get about a third of their food from the living, a third carry a sensor, and they are still slow (0.03 to 0.06 a tick). Still open: prey worth chasing, and anything that makes speed itself pay.
- **Waste can't be sensed.** Movers have no brain input for it, so nothing can learn to leave a fouled cell; they are only selected against it. Detox is mostly carried by plants as a side income (see 10.7) rather than making a separate cleaner niche: 5 to 15 true cleaners at a time.
- **Hunters are thinner since mouths have arcs (2026-11-12):** a sessile mouth now reaches only the neighbours in front of it, so hunters fell from 50 to 75 to about 24 across six seeds; with new mouths budding at the front, 29 and rising (10.7). Levers: `MOUTH_ARC_BASE`, `MOUTH_ARC_PER_SIZE` (`body.hpp`, and the shader); evolution's own answers are more mouths, bigger ones, and tentacles.
- **Every plant is a frond:** stalked sun patches win the canopy so reliably that 85 to 98% of bodies grow them (83% on six fresh seeds with light over the footprint, 90% without). Levers: `STALK_UPKEEP`, `STALK_MASS`. A cell still goes to the single tallest body first; nothing yet makes being under the canopy livable (shade-tolerant photosynthesis would).
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
4. **The player has little to do.** Six powers (10.5) and no goal. Nothing interesting can be caused on purpose yet (e.g. split a population with a mountain and watch two species form), and that needs point 1 to be visible.
5. **The first ten minutes are slow.** 600 LUCAs take thousands of ticks to fill the map; nothing visibly new appears before about tick 3,000.
6. **Save/load: built headless 2026-11-12** (`core/snapshot.*`, 14.6.1); not yet in the viewer.

Smaller: there is one nutrient, not several.

### TODO: deeper ecology and a less uniform world (picked by Uday, 2026-11-11)

*In [docs/history.md](docs/history.md).* Every item in it is built (canopy, mouths and size, cold nights, paid dispersal, bite, the danger / kin sense, corpses worth 244, digestion, slope shade, washing, the moving/sunbathing conflict, dormancy, poison, gene transfer, waste, growing up, mutability and repair, vents); kept as history.

### Next scaffolding

*In [docs/history.md](docs/history.md).* Superseded by the roadmap in 14.7.

## 12. Open questions

- **Reproduction mode timing:** sexual reproduction is designed in 9.4 (five stages, after water); how mates find each other is its stages 1 to 3.
- **Death and decay:** how corpses hold matter and how fast they release it.
- **Tuning workflow:** headless parameter sweeps with logged metrics. Designed in 14.6: paired forks of mature worlds.
- **Save/load:** built headless (14.6.1, 2026-11-12); the viewer still lacks it.
- **Later:** camera and UX, water dynamics (2.7), influence formula details. (The chemistry layer is built: 2.1.1.)

---

## 13. Working agreement for Claude Code in this repo

- **The creature rules (phenotype, energy, mutation) are open to edit** since 2026-11-11, when Uday asked for them to be completed. Say what you changed and why, keep constants named and in one place, and leave tuning decisions to him.
- **Keep the core free of Godot.** `core/` must compile with only the standard library.
- **Determinism:** every draw in the sim is keyed: `Rng rng(draw_key(draw_seed_, Draw::Purpose, tick_, body id[, other id]))`, or `key_uniform(key)` for a single number (`core/rng.hpp`). Never keep a running stream in the sim, and never `std::rand`, `random_device`, time, or unordered-container iteration order in sim logic. A new kind of draw gets a new `Draw` value, appended at the end. Functions that take an `Rng&` (`mutate`, `transfer_part`) use only that one.
- **Never break the energy audit.** Any change that moves energy must be reflected in `TickStats`. Run a headless check after sim changes and confirm audit error stays ≈ 1e-6.
- **Data-oriented and linear:** no per-creature heap objects in the hot path; no O(N²) loops; neighbour queries go through the spatial grid.
- **No population caps,** ever. Limits come from ecology only.
- **Powers never touch creatures.** They go through `World`'s edit functions and the `PowerSystem` log, so every change to the world is replayable.
- **Every new gene needs a cost** in the phenotype compiler.
- **Designs and history live in `docs/`** (map at the top). Keep this file to principles, current rules, the working agreement and the roadmap: a new design over a page goes in `docs/design/`, superseded measurements and replaced rules in `docs/history.md`, with a stub under the section's heading here.
- **Before finishing a change:** build with no warnings, run `./build/luca_headless --ticks 5000`, and check population behaviour, audit error and ms/tick against the baseline in 10.7. Also run `./build/test_powers` and `./build/test_life`.
- **Matter is conserved too.** Anything that builds, kills or moves matter must keep `total_matter()` constant, except what vents bring up from below, which is booked in `matter_in` (`total_matter() - matter_brought_up()` stays constant).
- **The look is pixel art.** No smooth gradients, no 3D meshes for creatures; new body parts get a pixel drawing in `blob.gdshader`.

---

## 14. Design review and roadmap (2026-11-12)

Uday asked for a review of the design against the code: holes, dead ends, rules that could be simpler and more general, and features that cost time for nothing. No code in `luca/` was changed. This section holds the findings, a design for the development harness he asked for (14.6), the roadmap (14.7), the removal of camouflage (14.8), the design of the sensor and motor brain he proposed in reply (14.9), and five points adopted from an outside review he brought in (14.10). Notes marked *Review 2026-11-12* elsewhere in this file point back here.

### 14.1 How it was checked

*In [docs/review-2026-11-12.md](docs/review-2026-11-12.md).* All of `core/` read against the design; probes on a private copy; seeds 42 and 7 at 100,000 ticks. Two seeds, not six.

### 14.2 Holes in the design

*In [docs/review-2026-11-12.md](docs/review-2026-11-12.md).*

Nine findings, each with a proposed fix:
1. A body catches light from one cell only, so nothing rewards size (bodies settle at one cell's worth; fronds are the only way up). Fix: light over the body's footprint.
2. The 30% a meal loses spills into the eater's cell and is mostly taken up again by the eater: eaters keep 85 to 90%, not 70%.
3. A kill's corpse lies in the victim's cell, out of the mouth's reach: hunters almost never eat what they kill. Fix: mouths pull on corpses within reach as on the living.
4. Light and warmth fall along the same axis, so half the map is one dead zone. Fix: vary them independently.
5. The hunters of 2.8 (movers) are not the world's hunters (rooted bodies with mouths).
6. Matter limits nothing (91 to 95% sits in the soil as A). See 14.10.2.
7. No leavings exist, and 2.9 A will not make them (tested); uneven levels with one rung per enzyme might. See 14.10.2 and 14.10.6.
8. Every sense is a hand-made output; kin reads species ids. Fix: the sensor and motor brain (14.9).
9. Rule changes are judged at the edge of what six fresh seeds can show. Fix: the harness (14.6).

### 14.3 Dead ends and dead weight

*In [docs/review-2026-11-12.md](docs/review-2026-11-12.md).* A table of mechanics nothing uses (neutral exchanges, three rules for moving costing sunlight, most of `shape[8]`, `Part::phi`, sensors on rooted bodies, gene transfer, windfalls, camouflage, `luca/DESIGN.md`, stale comments, this file's weight), each with a suggestion. The ablation pass (14.10.8) tests several of them.

### 14.4 The queued designs, re-read against the measurements

*In [docs/review-2026-11-12.md](docs/review-2026-11-12.md).* 2.8 A (satiation) should not be built first: full stores do 0.2 to 0.4% of draining. 2.8 C and 2.9 C are stage 2 of the sensor and motor brain. Water: build stages 1 and 3, then look again; stage 2's judge cannot pass while nobody gives off B or C.

### 14.5 Performance

*In [docs/review-2026-11-12.md](docs/review-2026-11-12.md).*

Five items, in order:
1. The tick pays per cell whether or not anything lives there (an empty 512² world costs as much as 7,000 creatures). Fix: touch only cells that hold something. *Done 2026-11-12 (step 1): an empty 512² world went from 1.44 to 0.056 ms/tick, runs identical tick for tick (below).*
2. Camouflage is worked out for every body every tick for almost no reader (12.5% of sim time). *Gone with camouflage (14.8), 2026-11-12.*
3. The hot loops read 264-byte structs. Fix: hot fields in their own arrays. *Left for now (2026-11-12): `Creatures` is already an array per field, and brains and body plans sit in arrays of their own that only movers and mouths read. `Phenotype` is the one wide record, and most of its fields are read in the hot loops. A profile of a full world puts the time in the per-body rules themselves (meals, burning, contact geometry), not in loading cold fields. Revisit if the 10,000-creature measurement misses 7.2's budget.*

**As built (step 1, item 1):** `SpatialGrid` (`spatial_grid.hpp`) sorts bodies by cell, with a stable two-pass radix sort on a sparse map and one counting pass over the cells on a crowded one (more than a body per 8 cells; the radix sort cost a crowded 128² world about 3%), and keeps a slice per occupied cell, reset only where bodies were; scans of a row of cells walk them one at a time (same order as before). The per-cell sums (`biomass_`, the light, corpse and uptake demands) are cleared only in the cells last tick wrote (`scratch_cells_`). `Soil` keeps two bitsets, cells that may hold a corpse and cells that may hold loose chemicals (`corpse_bits`, `loose_bits`; anything that adds sets the bit, a pass that empties a cell clears it, a load rebuilds them), and `weather_corpses` and `spread_dissolved` walk only those, in ascending order, so every float sum adds in the order it did. `Soil::waste` is recomputed where bodies, vents or the drift changed it, not over the map. Checked: four reference runs (vents and events; a small world with heavy windfalls; a third size with a different metrics interval; a snapshot continued) match the build before byte for byte in every output but timing. Not changed: `wash_soil` (every 64 ticks) and the metrics' books (every interval) still walk every cell.
4. Draws from a shared stream. *Done in step 0.3 (14.10.4).*
5. Rooted bodies are recomputed every tick; stepping them less often is a design change, last.

### 14.6 Development harness: snapshots, forks and laws (design; Uday's request, 2026-11-12)

*In [docs/harness.md](docs/harness.md).*

Snapshots, forks and laws, all built 2026-11-12 (steps 0.1 to 0.4):
1. Snapshots: `--save`, `--save-every`, `--load` (`core/snapshot.*`), exact round trip.
2. Laws: `--set`, `--laws`, `--list-laws` (`core/laws.*`), one set per process.
3. Forking: `--fork-seed`, `--knockout` (with `--load`).
4. The comparison: `tools/ab.py` (also the sweep script), and the `--probe` ledgers.
5. The shelf of mature worlds: `tools/shelf.py`, `worlds/`.
What a fork can tell you: what a rule does to a mature ecosystem, not what evolves under it. Screen on forks; confirm on fresh seeds.

### 14.7 Roadmap

**Next session: start here (updated 2026-11-13, later).** Water since the note below: a test and the shore detectors for stage 3, stage 3 judged on one run (swimmers never take hold), stage 2 (water as a solvent, laws off by default) and a depth sense built and judged on one run (a richer world, still no swimmers), and hot vents that open only under water. Uday's rule for tests now: **one run, at most 100,000 ticks.** Open: why nothing swims (a sea that is deep and dark at `--sea 12`; try a shallower sea or a larger `LIGHT_DEPTH`, then sinking, stage 4); whether to turn the water laws on by default; the default sea level.

**Earlier note (written 2026-11-13).** Done this session: step 3 (predation probes; `GEAR_STANDING` not adopted), step 4 measurements (leavings 83% own; P a brake; 2.9 B `INHIBIT_K` no effect), step 5 (sensor brain stages 1 to 3, on by default), and step 6 water stages 1 (sea, light under water; `--sea`, `sea` power, viewer) and 3 (motors by medium, buoyancy; laws neutral by default). Open, in order:
1. *(Done 2026-11-13: all 30 worlds rebuilt; baseline in 10.7.)* **The shelf rebuild** under the sensor brain (`tools/shelf.py --ticks 100000`, started 11:42, about 12:25 to finish; log `/private/tmp/claude-501/shelf_rebuild.txt`). If it was cut short, rerun the same command: it skips worlds already in `worlds/`. Then take the new baseline from it (10.7). The vents and events shelf kinds are not rebuilt.
2. **Judge water stage 3:** a quick `ab.py` (3 seeds, 40,000 ticks) with `--sea 12` in both arms, the variant adding `--set FLAGELLUM_ON_LAND=0.3 --set LEGS_IN_WATER=0.3 --set BUOYANCY=0.5`. Look for swimmers and walkers splitting by medium and bigger bodies in water. Needs a test (`test_life`) and the chronicle's "crawls ashore" detectors.
3. Then the default sea level (Uday's call; 12 drowns a third of seed 42's map) and the rest of water (2.7).
Uday's preference: quick checks (short runs, minutes), not long sweeps.

Order matters more than dates. Each step ends with the usual checks (13) and Uday's go-ahead; steps 2 to 5 are each judged first on forks, then on six fresh seeds.

**Step 0. Make testing cheap and exact** (no rule changes; every run before and after must match, except at item 3, which changes the random stream once and so starts a new baseline).
1. Snapshot save and load, with the exact round-trip test (14.6.1). Move `grown` into state. *Built 2026-11-12; see "As built" at the end of 14.6.1.*
2. `Laws` and `--set` (14.6.2). *Built 2026-11-12; see "As built" at the end of 14.6.2.*
3. Counter-based randomness for every draw (14.10.4). It changes every run once, so it comes before any baseline or mature world is kept. *Built 2026-11-12; see "As built" at the end of 14.10.4 and the new baseline at the top of 10.7.*
4. `--fork-seed`, `--knockout`, `tools/ab.py`, the `--probe` ledgers, the sweep script in the repo, and the shelf of mature worlds (14.6.3 to 5): 30 seeds rather than six, kept at 100,000 ticks. *The 1,000,000-tick runs (14.10.1) are shelved (Uday, 2026-11-13): `tools/shelf.py --ticks 1000000` still extends them when wanted.* *Built 2026-11-12; see "As built" in 14.6.3 to 5. `ab.py` with several variants is the sweep script. The shelf is built by `tools/shelf.py`.*
5. The ablation pass (14.10.8): the first use of `Laws`, run once the harness of item 4 exists. Delete what changes nothing. *Run 2026-11-12; results in 14.10.8 (`docs/review-2026-11-12.md`). The three rules that changed nothing (neutral exchanges, `repair`, `mutability`) were deleted 2026-11-12 at Uday's go-ahead, with their switches `USE_NEUTRAL`, `USE_REPAIR`, `USE_MUTABILITY`; it changed every run once (mutation draws), so `--ticks 5000` now gives 986 creatures.*
6. Split this file (14.3, last row) and delete `luca/DESIGN.md`. *Done 2026-11-12: 260 KB to about 140 KB; the map is at the top of this file.*
*Done when:* one command answers "what does `TRANSFER_EFFICIENCY=0.5` do to six mature worlds" with a paired table.

**Step 0b. A thin playable loop** (14.10.3), right after step 0. *Built 2026-11-12: save and load in the viewer, and "First steps" (10.6). One scenario; no influence.* Save and load in the viewer (the headless half is built), one scenario from 4.4 with its goal checked by the chronicle detectors that already exist (for example "evolve something that moves in under 10,000 ticks"), and a screen that says whether it was met. No influence yet. *Done when:* someone other than Uday can start the scenario, play it and be told whether they won.

**Step 1. Pay the performance debts** *(2026-11-12: camouflage removed; item 1 built, exact; item 3 left, see 14.5; 30-seed baseline taken, see 10.7; the 10,000-creature measurement waits for a quiet machine)* (14.5 items 1 and 3; item 2 goes with camouflage, 14.8; item 4 is done in step 0 as part of counter-based randomness, 14.10.4), before any new per-cell field. Remove camouflage first, as its own change: it changes results, so take a new baseline before the refactors, which must reproduce runs exactly. *Done when:* an empty 512² world is under 0.1 ms/tick, camouflage is gone, and a 10,000-creature world is measured against the 7.2 budget on the Mac.

**Step 2. Make geometry consistent** (each is an existing rule made general, not a new feature).
1. Light over a body's footprint (14.2.1). *Judge:* the spread of radius widens; shaded understorey bodies exist; fronds are no longer nine plants in ten.
2. Mouths reach the dead as they reach the living (14.2.3). *Judge:* the share of corpse energy eaten rises from about a quarter; scavengers and windfalls start to matter.
3. Decide what the lost share of a meal is (14.2.2). *Judge:* a hunter's food from loose chemicals falls from about 20% to a few percent, and hunters persist anyway.
4. Light and temperature on separate axes (14.2.4). *Judge:* life in every quarter of the map; more established species at the same total light.
Then delete the dead weight of 14.3 that these make clearly dead (`light_while_moving`, `shape`, `phi`; `MOVER_SUN` went in step 0.5).

*Built 2026-11-12 as laws, each off by default until judged. `LIGHT_FOOTPRINT` and `DEAD_IN_REACH` adopted 2026-11-13 (now 1 by default; 10.4); `LIGHT_ACROSS` failed and stays off; `SPILL_LOST` kept as it was (1) for now, Uday 2026-11-13, see Known weak spots:*
- `LIGHT_FOOTPRINT` 1 (item 1): a body asks for light over the cells a disc of its radius covers, centred on its cell, in proportion (`core/footprint.hpp`: a stencil per radius in steps of 0.02, worked out once by sampling). In each cell the canopy rule shares the light among every body asking there, tallest first (`Simulation::share_light_by_footprint`). Light that would fall off the map's edge falls on nothing. About twice the cost of the old light rule per body.
- `DEAD_IN_REACH` 1 (item 2): a mouth also pulls on the corpses of the eight cells around it, when the nearest point of that cell lies within its reach (its skin, or a stalk that way, capped at 1.5), with the pull it has towards the cell's middle, at `CORPSE_RATE` × that pull. The cell's corpses are shared in proportion with everyone asking, own cell included. What it keeps goes to its store, the rest spills into its own cell.
- `SPILL_LOST` 0 (item 3): of a meal, what the eater could burn but did not keep is lost at the mouth as heat (`out_transfer`), burned to A in its cell. Only what it cannot burn spills. (Option one of 14.2.2.)
- `LIGHT_ACROSS` 1 (item 4): the light gradient runs from x = 0 across the map instead of along y with warmth. Only matters with `--light gradient` (the default); slope shade still faces y = 0.
- `test_geometry_laws` checks each: a corpse in the next cell is eaten only with `DEAD_IN_REACH`; with `SPILL_LOST` 0 transfer loss appears and less is given off; the gradient's direction; a radius-2 body with a big patch catches over 3 cells' light only with `LIGHT_FOOTPRINT` and shades a LUCA beside it; both books balance.
- *Screened on forks 2026-11-12, not yet adopted* (30 shelf worlds at tick 100,000, one fork each, 20,000 ticks, last fifth; build before the deletions of step 0.5; full table in [docs/review-2026-11-12.md](docs/review-2026-11-12.md), "Step 2 fork screen"):
  - `LIGHT_FOOTPRINT=1`: creatures +30%, species +35% (29 of 30), energy caught +41%, mixed feeders ×2.9, living eaten ×3.7, movers ×2. The richest change measured. Time a tick +129% (+75% per creature). Its own judges (spread of radius, understorey, fronds) are not in `ab.py`'s table yet.
  - `DEAD_IN_REACH=1`: corpses eaten against weathered 0.77 → 2.5 (30 of 30), scavengers +47%, hunters +29%, species +16%, population unchanged. Passes its judge.
  - `SPILL_LOST=0`: uptake −99%, hunters −43% (47 left), mixed feeders −72%, living eaten −58%, sun-fed +14%. Hunters persist, at half the number: the spill was a large part of what hunting paid.
  - `LIGHT_ACROSS=1`: everything about −45% (28 of 30). A fork is the wrong test here: mature lineages sit where light and warmth used to fall together. Judge on fresh seeds.
  - *Six fresh seeds, 2026-11-13* (42 / 7 / 99 / 1 / 2 / 3, 100,000 ticks, last fifth, on the build after the step-0.5 deletions; review, "Step 2 on fresh seeds"):
    - `LIGHT_FOOTPRINT=1` **passes**: species 60 → 100 (6 of 6), mixed feeders ×2.7, movers 9 → 57, population +2%, hunters −30%. Its own judges: the spread of radius more than doubles (sd 0.10 → 0.23; 15% of bodies above radius 1.5, none without), bodies under a taller one in their cell 14% → 18%, fronds 90% → 83% (still most). Life reaches the cold half (5% of bodies → 16%). Time a tick +44%.
    - `DEAD_IN_REACH=1` **passes**: corpses eaten against weathered 0.59 → 2.3 (6 of 6), scavengers +57%, hunters +33%, species +12%, population −12%.
    - `LIGHT_ACROSS=1` **fails**: population −26%, species −25% (6 of 6). The dead zone moves rather than going: life leaves the dark half (x ≥ 64 holds 11% of bodies) for the bright one, and the dark, cold quarter still holds 2%. Splitting light from warmth only helps if neither leaves a half of the map unlivable; a gentler gradient or warmth from height instead are the options.
  - **Adopted 2026-11-13 (Uday):** `LIGHT_FOOTPRINT` and `DEAD_IN_REACH` are on by default. `--ticks 5000` now gives 1,352 creatures (986 before), 0.090 ms/tick, audit 1.7e-7. Two tests assumed the one-cell light rule: `test_canopy` and the frond check now set `LIGHT_FOOTPRINT=0` explicitly, and the two-input test lists its B + C enzyme first so it burns before D. Still open: drop or redesign `LIGHT_ACROSS`; deleting the dead weight this makes clear (below).

**Step 3. Re-aim predation** (2.8). *Run 2026-11-13: A unsupported on six seeds, B screened as an upper bound with the law `GEAR_STANDING` (default 1) and not adopted (more hunters and movers, fewer sun feeders and species); C goes with step 5. Numbers in docs/design/predation.md.* Not satiation (14.4). Run 2.8's probes for causes 2 and 3 on the shelf of worlds (*probes built 2026-11-12, in `probe.csv`; see 2.8*); re-measure movers after step 2, since bigger, spaced-out plants and reachable corpses change what moving is worth. Then try B (resting against working costs) on forks.

**Step 4. Make leavings exist** (2.9, 14.2.7). Build the leavings tag first (the probe 2.9 already plans). *Tag built 2026-11-12 (10.4). *Baseline, 2026-11-13 (six shelf worlds, 6,000 ticks on from 100,000): 82 to 85% of uptake of tagged leavings is a species taking up its own, 15 to 18% another's; in five seeds no species lives on more than half from one other species, in one seed (7) a single species does. So almost no cross-feeding.** Design stoichiometry (14.10.2; *designed and built as laws, off by default, 2026-11-12: [docs/design/stoichiometry.md](docs/design/stoichiometry.md), 10.4*) and test it on forks (`--set P_BASE=0.05`, scanning `P_MEAN`) against uneven levels with one rung per enzyme, and against single-step enzymes with leaky stores (14.10.6); shelve neutral exchanges if they are still unused afterwards. Only then 2.9 B. *2.9 B built 2026-11-13 as the law `INHIBIT_K` (default 0, off; `INHIBIT_SCALE` 74): a downhill enzyme runs at 1 / (1 + loose product × e^(−step/scale) / K) of its rate. Fork screen, 30 shelf worlds, 20,000 ticks: K=1 changes nothing (population +3%, species -1%, loose B -13%, loose C -1%); K=0.1 likewise (loose C +8%, species -4%). As expected, B has no partial burners to act on (14.2.7); it needs uneven levels with one rung per enzyme first, which is not built. Law left off.* *Stoichiometry fork screen 2026-11-13 (30 shelf worlds, 20,000 ticks, no deletions of extinct forks): `P_BASE=0.05` at `P_MEAN` 0.3 population -5%, species -4%, hunters -7%, movers -22%; at `P_MEAN` 0.1 population -32%, species -22%, hunters -39%, loose B -60%, loose C -49%, uptake -47% (28 of 30 worlds). P is a brake on everything, and no sign of cross-feeding or diet splits on mature lineages: loose B and C fall, not rise. Fresh seeds, where lineages could adapt to it, are the real test and are not yet run.*

**Step 5. The sensor and motor brain** (14.9; *stages 1 to 3 built 2026-11-13 behind the law `SENSOR_BRAIN`, on by default; see 10.4 "Sensor brain" for what exists and how it fared*), in its three stages: sensors and motors; brains without motors and the mouth gate (2.8 C and 2.9 C); the sleep drive. Kin by likeness of hue instead of species id comes with stage 1. This unlocks signals (2.6) and the senses water and sex will need. It raises what behaviour can evolve; it does not make movers common, which is step 2's job.

**Step 6. Water, stages 1 and 3** (2.7; *stages 1 and 3 built 2026-11-13, see 10.4 "Sea"; stage 3 not judged*), then review the remaining stages against what steps 2 to 5 changed. If the matter under vents needs bounding by then, use the mantle reservoir (14.10.7) rather than stage 4's sediment sink. 2.9 D (adhesion from birth) fits anywhere after step 2.

**Step 7. The game.** Scenario mode, influence and goals (4.4, 4.6), fossils (4.8), then sex (9.4). To keep the player in view before then, each step above should end with one thing a player can see or do: step 0 gives save and load in the viewer; step 2 gives forests and a reason to raise a mountain; step 5 gives signals to watch.

**Not now:** 2.8 A; water stage 2 as a cure for cross-feeding; any new sense as its own enum entry; carry-and-deposit; more chemistry stages; sex.

**Three habits worth adopting** (proposed for section 13; Uday's call):
- **Probe before building.** A design that rests on "why X happens" gets a measurement of X first. 2.8's probe saved building satiation.
- **Use it or shelve it.** A mechanic that no lineage uses in a six-seed sweep is shelved with its patch, as grazing was, and listed with the reason.
- **One rule per sweep.** No baseline is taken on a build holding other unfinished work.

### 14.8 Removing camouflage (decided and done 2026-11-12)

**Decision (Uday, 2026-11-12):** remove camouflage, the rule that others' senses feel a body by how plainly it shows against its ground. Keep the `hue` gene and earthy skin colours, as drawing only. **Why, from this review:** almost nothing reads it (few bodies hunt by sense, so hue mostly drifts), yet it costs about an eighth of all sim time (14.5 item 2). Hunting by colour can come back later as an ordinary channel of the generic senses (14.2.8), paid for like any sense, not as a rule of its own.

**What goes (code, when built):**
- `sim.cpp`: `Simulation::shows`, `ground_colour_at`, the `seen_` and `shows_` buffers and their phase-0 fill. The lean towards the living reads `biomass_` again (`lean(biomass_, 0.5f, w[OUT_BIOMASS])`), and the danger / kin sense weighs a body by `q.mass` alone, no longer × `shows_[j]`. That is the rule as it stood before 2026-11-12. `SOCIAL_FAST` stays: the kin sense still reads how fast a neighbour moves.
- `sim.hpp`: the two accessors and two buffers.
- `colour.hpp`: `ground_colour`, `visibility`, `VISIBLE_MIN`, `CONTRAST_FULL`. `skin_colour`, `SKIN_CHROMA` and `SKIN_EARTH` stay, since the shader and the inspector draw them.
- `godot/luca_world.cpp`: the inspector's `shows` key; `viewer/main.gd`: the "blends into the ground / stands out here" words on the skin line.

**What stays:** the `hue` gene, its mutation (`HUE_STEP`), its line in creature files and snapshots, the designer's hue slider, skin drawing in `blob.gdshader`, the chronicle's "It is bluer", and the snapshot's `hue` column.

**Open (Uday's call):** with no use, hue is a neutral marker that drifts. `W_HUE` (1.0 per turn of the wheel) still counts it in the species distance, so colour drift alone can push a newborn past `SPECIES_THRESHOLD` or drive a split. Options: keep it (species then differ visibly, which helps the viewer), or lower `W_HUE` so species reflect the genes that matter.

**When:** the first change of roadmap step 1, before the performance refactors, because it changes results while they must not.

**Done 2026-11-12** as listed above (and `distance` in `colour.hpp`, which only camouflage used). `--ticks 5000` is unchanged at 978 creatures: nothing senses by sight that early. The six-seed baseline is still to take (step 1). `W_HUE` left as it is.

**Checks (as planned):** the usual ones (no warnings, `test_powers`, `test_life`, `--ticks 5000` against 10.7), then a six-seed 100,000-tick run as the new baseline for step 1. Expect `--ticks 5000` to move from 880 creatures and speed to improve by about an eighth. No test in `test_life` checks camouflage today. Update 10.4 (the colour bullet), 6.6 and 8.2 once it is built.

### 14.9 Sensor and motor brain: design (Uday's proposal, 2026-11-12; nothing built)

*In [docs/design/brain.md](docs/design/brain.md).* Uday's proposal, nothing built: sensors with a channel read a field at a point (bilinear); the brain gets per channel a level and a side contrast on a fixed bus (never naming a part); it drives thrust, turn, mouth and sleep directly; heading is state and turning is paid for. Three stages: sensors and motors; brains without motors and the mouth gate; the sleep drive. Roadmap step 5.

### 14.10 Points adopted from an outside review (2026-11-12)

*In [docs/review-2026-11-12.md](docs/review-2026-11-12.md).*

Eight points:
1. Generations, not just seeds: judge empirical negatives on long runs and many seeds (30 seeds, 1,000,000 ticks). *The 1,000,000-tick part is shelved for now (Uday, 2026-11-13); 30 seeds at 100,000 stays.*
2. Stoichiometry: several elements in fixed ratios, the deeper fix for 14.2.6 and 14.2.7. Design before 2.9. *Designed 2026-11-12 in [docs/design/stoichiometry.md](docs/design/stoichiometry.md): one conserved mineral, P, with no energy, needed per unit of body (more for armour and legs), taken up through the skin, carried by meals, freed from corpses mainly by eating; eaters give off P they can't hold, which feeds plants. Built the same day as laws, off by default (`P_BASE` 0; 10.4).*
3. A thin playable loop early (step 0b).
4. Counter-based randomness for every draw. *Built in step 0.3.*
5. The winner-take-all canopy (test passing light down by leaf, Beer–Lambert); build sex without the fertility curve first.
6. Single-step enzymes with leaky stores, a fork candidate for step 4 (three catches).
7. A mantle reservoir for the matter vents bring up, when it matters.
8. An ablation pass: switch each rule off over 30 fresh seeds; delete what changes nothing (step 0.5). *Run 2026-11-12 (30 seeds, 100,000 ticks): folding and gene transfer matter most (gene transfer holds hunters down by half); light lost while moving matters a little; bite and windfalls are borderline; neutral exchanges, `repair` and `mutability` change nothing measurable, and were deleted the same day. Full table in 14.10.8.*
