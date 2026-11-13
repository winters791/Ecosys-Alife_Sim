# Stoichiometry: one mineral that bodies are built from (design, 2026-11-12; built the same day as laws, off by default)

*Roadmap step 4 (CLAUDE.md 14.7): "design stoichiometry (14.10.2) and test it on forks against uneven levels with one rung per enzyme". This is that design. Built 2026-11-12 as laws, off by default; CLAUDE.md 10.4 describes it as built, and "As built" at the end says where it differs from what is below.*

## Why

Two findings in the review (14.2.6, 14.2.7) and one point from the outside review (14.10.2) say the same thing from three sides:

- **Matter limits nothing.** 91 to 95% of the world's matter sits in the soil as A. Chemicals A to D are one kind of matter at four energy levels, so the matter book repeats the energy book, and nothing that 2.1 wanted from matter (a niche for decomposers, poor ground worth fighting over, "what bodies can be built from") has anything to act on.
- **No leavings exist.** A partial burn is strictly worse, so nothing gives off B or C, and nobody can live on what another gives off (milestone 5).
- **Niches in real ecosystems come from elements needed in fixed ratios.** A body is limited by whichever element it is shortest of (Liebig's minimum), and what one lineage has to spare is what another lacks.

14.10.2 proposes stoichiometry as the deeper fix, but as stated it is a big change: "every `Mix`, store, corpse and per-cell field grows". This design is the smallest version that still does the job.

## The idea in one paragraph

Energy stays in chemicals A to D, exactly as now. Add **one conserved element that carries no energy**, a mineral, called **P** here (after phosphorus, the element that most often limits life on land and in lakes). Every unit of body needs some P, so a body can grow and breed only as far as the P it holds allows. P comes from the soil through the skin, from meals, and from corpses as they are eaten or weather. When an eater takes in more P than its body needs, it gives the excess off into its cell, where plants take it up. That is consumer-driven nutrient recycling, the best-measured kind of cross-feeding in real ecosystems, and here it needs no rule of its own: it follows from conservation plus "a body holds only what it can use".

## Why one element and not a column on every chemical

- **What gets cheaper.** P is a scalar beside each `Mix`, not a fifth chemical: there are no enzymes for it, no levels, and no change to `burn`, `charge`, `power` or `convert`. That is two floats per body (body P and stored P) and two per cell (soil P and corpse P), plus one more book.
- **Why a fifth chemical would be wrong.** It would have an energy level, so enzymes could turn it into food, and it would fall back into the ladder.
- **Why not two energy carriers in a ratio** (a carbon-like and a nitrogen-like chain). That doubles the chemistry and gives the same effect as one non-energy element, for more code.

## The rules

1. **P is conserved.** It lives in five places, and each has its own number:
   - soil P per cell (`Soil::mineral`), which is both free and loose P (one field, not two);
   - corpse P per cell (`Soil::corpse_p`);
   - body P per creature (`Creatures::tissue_p`), the P its body is built of;
   - stored P per creature (`Creatures::pool_p`);
   - P in transit in a meal, which settles the same tick.

   The P book is exact, like the matter book: total P changes only by what vents bring up (rule 9).
2. **A body's P need comes from its parts, not a gene.** Each unit of tissue needs `P_BASE` P. Each part adds `P_PER_SIZE[type]` × size, taken from a table beside `MASS_PER_SIZE`, as a guess:

   | Parts | P per unit of size | Notes |
   | --- | --- | --- |
   | Armour, legs | High | Shells and bone are mineral |
   | Absorber, flagellum, sensor | Middling | |
   | Photo patch, storage, poison | Low | Leaves and fat are mostly carbon |

   `Phenotype::p_need` is the body's total at full size. This is the general rule standing in for an "armour needs calcium" gene: armour is dear on poor ground and cheap on rich ground, and nothing says so outright.
3. **Building is limited by P (Liebig).** Growing up (`GROW_STEP`) and breeding (the child's body) build tissue only as far as P allows. A step of growth that adds Δm of mass needs Δm × `p_need` / mass of stored P. If stored P falls short, the step is as big as stored P allows. A parent hands its child the child's body P, plus `offspring_share` of its stored P (as now for chemicals). A body full of energy but short of P cannot breed. That is the "poor ground worth fighting over" of 2.1.
4. **Uptake through the skin.** Each tick a body asks for `P_UPTAKE` × skin × the soil P of its cell, shared out if the cell's bodies ask for more than all of it (as loose chemicals are now). It takes up only what fits under its P store cap, which is `P_STORE` × `p_need`: a body keeps a few bodies' worth at most. The skin carries the cost, so no new upkeep is needed: a body that takes up P does so with the skin it already has.
5. **Meals carry P.** Draining a living body takes its stored P in proportion to the stored chemicals taken. Eating a corpse takes its P in proportion to the corpse mass taken. The eater keeps what fits under its cap and **gives the rest off into its cell's soil P**. Nothing is lost at the mouth: P is never burned.
   - This is the cross-feeding rule. A hunter whose prey is richer in P than it needs gives P off where it sits, and the plants around it take it up. A hunter living on armoured prey fertilises its neighbourhood.
6. **Death.** A body's P (body and store) goes into its cell's corpse P. Corpse P is freed into soil P only as the corpse is eaten (rule 5) or weathers. Weathering frees P at `P_WEATHER_SHARE` × the corpse's own weathering rate, a fraction under 1, so **eating the dead is the fast way P goes back to the soil**. That gives decomposers the niche 2.1 promised: a plant on a graveyard is held back until something eats the dead. Today corpses free their own matter by weathering, so no plant depends on a decomposer (14.2.6).
7. **Washing.** Soil P washes downhill with soil A (`wash_rate`), so hollows grow rich and slopes poor. No new rule: the same pass carries one more field.
8. **Drift.** Soil P does not drift like loose chemicals (mineral in soil stays put). Only washing and bodies move it, so a patch stays a patch.
9. **Vents bring up P,** at `VENT_P` per unit of vent C. That is the one way P enters, booked in a `p_in` column. If the mantle reservoir (14.10.7) is built, vents draw P from it too, and the P book is closed.
10. **The world starts patchy.** Soil P starts at `P_MEAN` per cell, laid out by the same noise as `nutrient_patchiness` but with its own seed. It is scarce enough that about half the map limits breeding before light does (to tune: the judge below says how).

## What should emerge, and how to tell

None of these is coded. Each needs a measurement, all observation only and with no `Rng`:

| What should appear | How to see it |
| --- | --- |
| Poor ground and rich ground | Share of growth steps and breeding tries held back by P against those held back by energy, by region. The metrics get `p_limited` and `energy_limited`. |
| Decomposers matter | Share of corpse P freed by eating against weathering (today about a quarter of corpse energy is eaten). Scavengers should rise where P is scarce. |
| Cross-feeding by recycling | P given off by eaters and taken up by other species (the leavings tag, extended to P: `leavings.csv` gets a `p` column). Plants beside hunters should breed faster than plants far from them. |
| Armour follows the ground | Mean armour against soil P where a body lives. |
| Sugar leavings, maybe | A P-limited plant with a full store stops catching light. If leaky stores (14.10.6) are built as well, it leaks D, and that is the root exudate that feeds soil microbes in real soil. Nothing here requires it. |

## Two catches

- **P limits only if it is scarce.** If `P_MEAN` is too high, nothing changes except two more floats a body. If it is too low, the world empties. The first fork sweep should scan `P_MEAN` across a factor of ten and keep the level where `p_limited` and `energy_limited` are comparable.
- **Kin hoard P.** Children land beside their parents and inherit stored P, so a lineage sitting on a rich patch keeps it. That is fine (it is a territory), but it means P alone will not make the far, poor ground livable. Light over the footprint (step 2) and movers matter more there.

## How it fits with the other step-4 candidates

These two are complements, not rivals: one could be built with the other.

| | Uneven levels with one rung per enzyme (14.2.7) | This design |
| --- | --- | --- |
| Makes leavings of | Energy chemicals (B, C) | P, and maybe D |
| Leans on | A cost curve and a cap on enzyme rate | Conservation alone |
| Changes | `burn`, `charge`, `LEVEL`, LUCA's genome | Growth, breeding, meals, death, uptake; no change to the chemistry |

Fork-test both and compare them on the same judges.

## Building it

- **Off by default.** `P_BASE` 0 means no body needs P. Building is then never limited, uptake and meals still move P (observation), and runs are unchanged. Turning it on is `--set P_BASE=...`, so it can be tested on forks like the step-2 laws.
- **Forking a shelf world.** A shelf snapshot has no P. On loading, soil P is laid out from the world's seed, and each body is given exactly the body P it needs and an empty store, as if it had always lived under the rule. That needs a snapshot version bump, at which point `Part::phi` can be dropped as well (it is waiting for one).
- **Code touched:**
  - `chem.hpp` is untouched;
  - `soil.hpp` gets two fields and the corpse bit;
  - `creatures.hpp` gets two fields;
  - `phenotype.*` gets `p_need` and the part table;
  - `sim.cpp`: phase 2b gets uptake, meals, growth, breeding and death, plus `wash_soil`, `run_vents` and `total_p()`;
  - `metrics.*` gets the P book (exact), `p_limited`, `energy_limited`, `soil_p_mean` and `corpse_p_freed_eaten`;
  - `snapshot.*` and `creature_file.*`: P is state, not genome, so creature files are unchanged;
  - the viewer gets a soil-P overlay and an inspector line ("held back by P").
- **Cost per tick:** one more uptake share per body (the loose-chemicals pass already walks every body's cell) and a few multiplies. Cheap.
- **Tests:**
  - the P book is exact over a run with vents;
  - a body with energy but no P does not breed;
  - an eater of P-rich prey gives P off into its cell;
  - with `P_BASE` 0, runs are identical to the build before.

## Open (Uday's calls)

- **The name.** P, or something plainer in the game ("mineral", "salt").
- **The part table**: which parts are mineral-heavy, and by how much.
- **Breeding limits.** Whether a child that would be born short of P is not made at all, or made smaller (birth size scaled down).
- **Uptake in the dark.** Whether P uptake runs at night and asleep (uptake of loose chemicals is scaled by 1 − sleep; P could be the same).
- **The sweep.** Whether to test this before or after step 2's laws are adopted. Light over the footprint changes where bodies sit, and so where P is drawn down.

## As built (2026-11-12)

How the open calls were settled, all by default and easy to change:
- **The name** is P.
- **The part table** is as guessed above: armour 2, legs 1.5, mouth, tail and eye 0.5, poison 0.2, sun patch and storage 0, times `P_PART_SCALE`.
- **A child short of P** is not made: the parent waits.
- **P uptake** is scaled by 1 − sleep.

Where it differs from the design:
- **No snapshot version bump.** P sits in sections of its own, which old files lack, so `Part::phi` is still waiting for its bump.
- **The leavings tag has no P column yet.**
- **`p_limited` and `energy_limited` count breeding tries only,** not growth steps.

**First measurements** (on a machine loaded by the ablation pass, so only a first look):

| `P_MEAN` | Creatures at tick 20,000 |
| --- | --- |
| P off | 3,504 |
| 0.05 | 169 |
| 0.2 | 1,916 |
| 0.5 | 2,899 |
| 1 | 3,074 |
| 2 | 3,538 |

These are seed 42 with `P_BASE` 0.05. At 0.05 the soil still holds 97% of the P: a body draws only on its own cell, and a cell's P builds about one body, so local supply is what limits.

The default `P_MEAN` went to 0.3, between the strong and mild settings. Forked from the shelf world `plain_s42_100000` at that level, 6,000 ticks end at 2,798 creatures against 3,971 without P, with P holding back 22% of breeding tries. Corpses hold more P than living bodies (they free it slowly), which is the hook for decomposers.

Next: forks of the whole shelf at a few `P_MEAN` values, judged on the table in "What should emerge".
