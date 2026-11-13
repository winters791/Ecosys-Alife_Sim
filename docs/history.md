# History: older measurements and finished lists

*Moved out of CLAUDE.md on 2026-11-12 (roadmap step 0.6), word for word. Section numbers are kept, so a reference like "2.7" or "14.2.7" means the same section wherever it now lives; CLAUDE.md lists where each one is.*

### 10.4 Rules as they were (replaced since)

Wander, before it turned by distance (from the bullet "Wander drifts"):

  - **History of the "spinning in circles" (2026-11-11):** first the angle was a fresh random one every tick (jitter on the spot). Then it drifted by a fixed 0.5 rad every tick (`WANDER_TURN`), which looked fine for fast bodies but not for real movers: nearly all of them creep at 0.01 to 0.03 a tick, so they turned ~23° a tick while their loops were a tenth of a cell across, and the sprite rotated in place. Measured with a probe over 300 ticks: mean turn 23° a tick, path straightness 0.19. With turning by distance: 5 to 7° a tick, straightness 0.72 to 0.78.
  - **The spin was holding the world together.** With movers able to wander straight, slow sun-fed drifters (plants with a tiny motor) roamed onto fresh, unshaded, unfouled ground and took over: seed 7 fell to 800 creatures, 600 of them movers, 7 species. Lowering `FULL_STRIDE` from 0.02 to 0.005 (a creeper now loses most of its sunlight) and `WANDER_PATH` from 3 to 1 brought the world back (numbers in 10.7). `test_wander_turns_by_distance` checks that a slow wanderer does not spin.

- **Movers catch half the sun (2026-11-12, replaced by folding the same day; `MOVER_SUN` deleted in step 0.5):** a body that can move (a motor and a brain) has its `light_capture` halved, moving or not (`MOVER_SUN` 0.5, `phenotype.cpp`). Light-while-moving still cuts it further on the ticks it moves. The proportional rule alone could not stop part-time creepers: most moved on 5 to 50% of ticks and kept about two-thirds of their sunlight. A motor with no brain, which cannot move, is not affected.
- **Waste (2026-11-11; replaced by loose chemicals in chemistry stage 2, kept here as history):** `WASTE_SHARE` (20%) of every body's upkeep leaves it not as heat but as waste in its cell (`Soil::waste`), still holding that energy. Every body in the cell, its maker included, loses `WASTE_HARM` (0.0025) × waste × skin area a tick to it, as heat (`out_fouling`), asleep or not. Every 8 ticks each cell hands `WASTE_SPREAD` (20%) of its waste to its four neighbours and `WASTE_DECAY` (4%) of it breaks down to heat (`out_waste_decay`). A body with the `detox` gene (an optional trait like `bite`) takes up to 0.3 × detox a tick of its cell's waste, shared like corpses, and keeps its usual `efficiency` of it. Waste is energy only: the matter books do not change. "Stored" energy now includes waste. The census counts **cleaners** (over 60% of food from waste), the chronicle logs the first ten, and `fed_waste` is tracked per creature. Rules: `fouling_cost`, `waste_demand` (`energy.cpp`), `Simulation::spread_waste`. One look at the cell in phase 2 does all three things: take up, pay for fouling, excrete.

### 10.7 Baseline numbers: older measurements

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

**Later (deliberately not now):** water (designed as nine stages in 2.7, none built) and seasons (the bright band drifting north and south over a long year).

Suggested order when building: canopy and dispersal first (biggest effect, smallest change), then corpse energy and the size rule for mouths, then the terrain pair, then the three that add genes or senses.

### Next scaffolding

*Superseded by the roadmap in 14.7 (2026-11-12); kept as the list it grew from.*

- Speed: keep creatures roughly sorted by cell, then multithread phase 1 by world tiles
- World events that move matter: volcanoes that bring up nutrients, or erosion. Today's events change heat, height, vents, sun and climate only.
- Water as a world layer, one stage at a time: see 2.7 (start with stage 1, sea level and light under water)
- Rewarding predation: see 2.8 (probe hunters' budgets first, then satiation at the mouth)
- Cooperation and symbiosis: see 2.9 (tag leavings by giver first, then rate against yield and product inhibition)

- Species follow-ups: fossils and the museum (4.8); following a lineage across its daughter species (today a follow stays on one species)
- More than one nutrient; matter washing downhill
- Niche construction (2.6): shade from photo patches is the cheapest next step
- Viewer follow-ups: tree-of-life and food-web panels, interpolation between ticks

---
