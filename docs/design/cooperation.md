# Cooperation and symbiosis (2.9)

*Moved out of CLAUDE.md on 2026-11-12 (roadmap step 0.6), word for word. Section numbers are kept, so a reference like "2.7" or "14.2.7" means the same section wherever it now lives; CLAUDE.md lists where each one is.*

### 2.9 Cooperation and symbiosis: design (proposed 2026-11-12; nothing built)

**Why (Uday, 2026-11-12):** milestone 5 asks for cross-feeding and symbiosis, and neither has appeared. Nothing about cooperation is to be built in. The aim is rules under which it *can* pay, plus tools to see it when it does. Cooperation needs two things: a benefit one body gives another as a side effect of living, and structure that keeps the same partners together. The world already has the structure (children land touching their parent, so neighbours are kin). It lacks the benefit.

**Why nothing cross-feeds today (from the code):** enzyme upkeep is `ENZYME_UPKEEP` × rate (`phenotype.cpp`), whatever the reaction's energy step, and the power cap is rate × step. So D → B (releases 74) costs the same and moves less power than D → A (222) at the same rate: a partial burn is strictly worse, nobody gives off B or C, and there is nothing for anyone to live on. Loose B and C staying at a few units a world is this rule, not chance.

**Ideas, most fundamental first.** Each is one physical rule; none is a gene per niche.

- **A. Rate against yield.** An enzyme's upkeep grows faster than the energy step it spans, e.g. upkeep ∝ rate × (step / 222)². A short step then gives the most power for its upkeep; a full step the most energy per unit of food. Where food is plentiful, fast partial burners (fermenters) win and give off B or C; slow full burners live on those leavings. The textbook route to cross-feeding (Pfeiffer, Schuster and Bonhoeffer 2001; seen in Lenski's long-term *E. coli*). *Watch:* the same enzymes run uphill for light, so this retunes plants too (a plant making B or C becomes cheaper than one making D). The exponent is a lever. *Review 2026-11-12: tried on a private copy with the exponent at 2 (seeds 42 and 7, 60,000 ticks): loose B and C did not rise, and nearly every body still carries A ↔ D. Under this rule a chain of short steps in one body beats a partial burn; see 14.2.7.*
- **B. Products slow their reaction (thermodynamics).** A downhill reaction runs slower as its product piles up in the body's cell, and more so the smaller its step: e.g. rate × 1 / (1 + product_in_cell × exp(−step / `INHIBIT_SCALE`) / `INHIBIT_K`). D → A barely notices soil A; D → C stalls in a cell full of C. A neighbour that eats C speeds its partner up: syntrophy, as between fermenters and methanogens. It fits the neutral exchanges, which already run only towards balance. With fouling (a body is harmed by loose chemicals it can't burn), a cleaner neighbour is both food and relief. Build with A: A makes partial burners exist, B makes their partners matter. *Open:* whether soil A counts as a product (it would slow plants in rich soil), or only loose chemicals.
- **C. A mouth the brain can close.** A brain output that scales pull from 0 to 1 (no gene, no new cost; a body without a brain pulls fully, as today). Lets evolution wire "don't drain kin" (with the kin sense), "keep a slow leak from this host" (a parasite becomes a farmer) or "stop when full". Partner choice and sanctions become possible. Overlaps with 2.8 A (satiation) and C (rest); decide together.
- **D. Adhesion from birth (2.6).** Build the clonal form first: a child may stay joined to its parent, and chemicals both can burn flow across the join (at a rate, towards equal fullness). Clonal partners share every gene, so there is no conflict: the route to multicellularity, and division of labour (a sun-catcher joined to a mouth). Joining strangers on contact (aggregation, cheater-prone) second.
- **E. Enzymes that act outside the body.** An enzyme may be secreted: it turns over its cell's loose chemicals instead of the store, and anyone there can take up the product. A public good with freeloaders; low dispersal decides whether it holds. A property of an enzyme (a flag mutation can flip), not a niche gene. Needs A or B first, or there is nothing worth secreting for.
- **F. Inherited partners (endosymbiosis), last.** Once D exists: when a host breeds, a smaller joined partner of another species can go with the child. Passing partners down aligns the two lineages' interests, as with mitochondria.

**Water** (2.7 stage 2) cuts both ways: some mixing lets leavings reach a partner; too much spreads them to everyone and favours cheaters. Shallow, still water may be the sweet spot.

**Seeing it (observation only, no `Rng`, sim logic never reads it):**
- *(Built 2026-11-12 as one tag per cell: CLAUDE.md 10.4, "The leavings tag"; `--probe` writes `leavings.csv`.)* **Leavings tagged by who gave them off,** like `Soil::vent_made`: each cell keeps the species that gave off most of its loose chemicals (or a few species and shares), and uptake credits the taker with eating that species' leavings. Gives a species × species table of who lives on whose leavings: the probe "Measuring milestone 5" (2.1.1) asks for.
- **Knockout experiments:** the sim is deterministic, so copy the world, remove species X from the copy, run some thousands of ticks and compare species Y. Y falls: Y depends on X. Both fall in each other's knockout: mutualists. Classifies every pair (+/+, +/−, −/−) without labels. Also a god-game tool ("what if they vanished?") that never touches the real world.
- **Chronicle:** "Y lives on what X gives off", "X and Y need each other", first body joined to its child, first inherited partner.

**Order of work:**
1. **Probe first:** the leavings tag on the current build, six seeds, 100,000 ticks, as the baseline.
2. **Build A and B together**, with the usual checks (no warnings, `--ticks 5000` against 10.7, `test_powers`, `test_life`) and a six-seed run. *Judge:* loose B and C rise; partial burners exist; the table shows a species living mostly on another's leavings (milestone 5).
3. **C**, decided with 2.8. *Judge:* brains wire the gate to the kin sense or to eating.
4. **D**, then **E**, then **F**, one at a time, each judged on six seeds with Uday's go-ahead.

**Open before building:** the shape of A's cost curve and whether it touches uphill (light) running; whether B's inhibition reads the cell, the store or both; whether a knockout copy is cheap enough to run from the viewer, or headless only.

---
