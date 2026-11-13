# Sexual reproduction (9.4)

*Moved out of CLAUDE.md on 2026-11-12 (roadmap step 0.6), word for word. Section numbers are kept, so a reference like "2.7" or "14.2.7" means the same section wherever it now lives; CLAUDE.md lists where each one is.*

### 9.4 Sexual reproduction: design (proposed 2026-11-12; nothing built; after water, 2.7)

**Why:** 9.1's rule (fertility falls with genetic distance) is what makes species real, so that a population split by a barrier either merges again or stays two species, and nothing scripts which. Gene transfer on contact (10.4) is a stand-in. Sex is **a trait evolution may take up or not**, never a law: LUCA is asexual, and if sex doesn't spread, that is a finding about the world.

**The rule:** a gamete is loose stuff. It is a small packet a body gives off into its cell, carrying a copy of its parent's genome and a little energy and matter. It moves by the rules loose chemicals already obey: it barely spreads on land, spreads fast in water (2.7 stage 2) and rides currents (2.7 stage 5). That is why water comes first: broadcast spawning in the sea is the cheap first form of sex, as in real life.

**Rules for every stage:**
- **Both books stay exact.** A gamete's energy and matter come out of its parent's store (as chemicals, like a child's body). When it decays (within a few ticks to a day) or is wasted on an unviable union, they go back to the cell as loose chemicals.
- **Linear and bounded without caps.** Each body holds **one gamete slot**. A newly met gamete replaces the stored one with a 1/n chance (reservoir sampling over the gametes it has met since it last bred), so there is no per-cell list and no population cap. Gamete genomes live off the hot path, like `BodyPlan`.
- **Deterministic.** Every draw is keyed (`draw_key`, 10.4 "Determinism"): by the receiving body's id and the tick, so order does not matter.
- **Every new gene has a cost** in the phenotype compiler.
- Build **one stage at a time**, each ending with the usual checks, a six-seed 100,000-tick run (42 / 7 / 99 / 1 / 2 / 3) and Uday's go-ahead before the next.

**Stages, in order:**

1. **Recombination, fertility, and mating by contact.** This stage needs no water, so it could be built before 2.7 if wanted.
   - **`recombine(a, b, rng)`**, run before `mutate()` and clamped once by `clamp_genome`:
     - number genes are taken from either parent (perhaps with a little blending);
     - parts are matched by type and nearest angle, with matched ones taken from either parent and unmatched ones kept at 50%;
     - enzymes are matched by reaction (`canonical_enzyme`), with the rate from either parent;
     - brain links are matched by their (from, to) endpoints.
   - **Neurons need stable ids:** hidden neurons get an id at birth that is inherited (like NEAT innovation numbers), because neuron 3 in one brain means nothing in another. This is the largest change in the stage.
   - *Review 2026-11-12: build this stage without the fertility curve first and add it only if nothing isolates (14.10.5).*
   - **Fertility:** viability = f(species distance) is 1 for close mates and falls to 0 past `FERTILITY_LIMIT`. An unviable union wastes the gamete.
   - **The `sex` gene** (an optional trait like `bite`, 0 for LUCA) is the share of breedings that use the stored gamete instead of a copy of the parent alone. Its cost comes from the physics: gametes cost energy and matter, and a sexual parent passes on only half its genes. A small upkeep may be added if needed.
   - **Mating by contact:** in this stage the only way to fill the slot is touching. Each tick two bodies are in contact (reusing the contact walk), each may take a copy of the other's genome.
   - *Judge:* does `sex` rise in any seed? Do sexual lineages carry fewer harmful mutations, or combine inventions faster? Do the two halves of a split species stop interbreeding?
2. **Gametes as a drifting field (broadcast spawning).**
   - A body with `sex` > 0 gives off gametes into its cell at a rate tied to `sex`, paid for from its store.
   - Gametes spread and decay like loose chemicals, scaled by wetness, and ride currents.
   - A body fills its slot from the gametes in its own cell, by the same uptake as chemicals.
   - *Judge:* sex is more common in the sea than on land.
3. **Gametes stuck to skin (pollination).**
   - A gamete in a cell can stick to the skin of a body there and rub off on the next body that body touches (one carried gamete per body, the same reservoir rule).
   - No pollinator is coded: a mover wandering between plants carries their gametes.
   - *Judge:* sexual plants on land do better near movers; look for plants that feed the movers that carry their gametes.
4. **Gamete size: anisogamy.**
   - A `gamete_size` gene (priced like `birth_size`) sets how much each gamete is provisioned, and so how many a body can afford.
   - The classic result is disruptive selection: many tiny gametes against few large ones (sperm and eggs), without a coded sex.
   - *Judge:* a two-peaked spread of `gamete_size` inside one species.
5. **Mate preference, and species as interbreeding groups.**
   - A preference gene, or brain outputs reading existing senses, makes a body accept a gamete only from mates of a preferred hue or size. That opens sympatric speciation (9.2).
   - A preference for showy mates would have pulled against camouflage; with camouflage removed (14.8), showiness has no cost unless a hue sense for hunters comes back with 14.2.8.
   - Probably needs signals (2.6) or a hue sense first.
   - The species registry learns hybrids: a species can have two parents.
   - *Judge:* reproductive isolation without geography; bright lineages where hunters are few.

**Observation:**
- The tree of life becomes a network (hybrids drawn with two parents); `species.csv` gains a second parent column.
- New chronicle detectors: first sexual species, a hybrid zone forming, two species that can no longer interbreed, and anisogamy.
- The inspector shows a body's stored gamete and whose it is; the designer gets `sex` and `gamete_size` sliders.
- Creature files already carry about 20 genomes, which gives variation for sex to work on.

**Open before building:**
- The shape of the fertility curve, and whether `SPECIES_THRESHOLD` and the distance weights need retuning once they shape life rather than labels.
- Whether gene transfer on contact stays as well (horizontal transfer, the bacterial route), or is folded into mating.
- Whether selfing (a body's own gamete) is allowed or counts as asexual.
- How a mated child's species is assigned when its parents belong to different species.

---
