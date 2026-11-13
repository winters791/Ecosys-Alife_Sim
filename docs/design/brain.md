# Sensor and motor brain (14.9)

*Moved out of CLAUDE.md on 2026-11-12 (roadmap step 0.6), word for word. Section numbers are kept, so a reference like "2.7" or "14.2.7" means the same section wherever it now lives; CLAUDE.md lists where each one is.*

### 14.9 Sensor and motor brain: design (Uday's proposal, 2026-11-12; nothing built)

**Why:** finding 14.2.8. Today the brain sets gains on eight built-in steering rules (lean to light, the living, the dead, comfort, soil, kin, bigger, moving), plus wander, inertia and throttle. Every new thing to notice needs a new rule. Uday's proposal: the brain reads **physical sensors** and drives **effectors** directly, so steering is wiring that evolves, and the rules hold none of it. It replaces the eight lean outputs, `OUT_WANDER`, `OUT_INERTIA`, `OUT_THROTTLE`, the `WANDER_PATH` rule, and (in stage 3) the fixed dark-or-cold trigger for dormancy.

**Sensors.**
- A sensor part already has an angle (`theta`, which the sim ignores today: only the summed sensor size is used) and a size. It gains one gene, its **channel**: which field it reads. Mutation can change the channel as it can retype a part.
- It reads **one point**: out from the body's centre along its angle (turned by the body's heading), at a range of the body's reach plus `SENSE_CELLS_PER_SIZE` × size cells. A bigger sensor looks further, and pays more upkeep, as now.
- The reading is the field at that point by **bilinear interpolation** over the four nearest cell centres (clamped at the world's edge). *Trap 1 (Uday):* fields live on a 1 × 1 grid, so reading the cell under a sensor on the rim often puts the left and right sensors in the same cell: no difference, no steering. Interpolation makes the reading continuous, so any separation gives a difference; the projection range sets how wide the baseline is.
- A sensor part is a **mirrored pair**, as legs are drawn: one part reads at +theta and at −theta. A part on the midline (front or back) is a single eye.
- One sample per sensor per tick: four reads. The 4-sample gradient per field and the kin sense's walk over the spatial grid both go, so a mover costs no more than now and no longer costs more in a crowd.

**The bus: the brain never names a part.** *Trap 2 (Uday):* parts are added, copied and deleted, so a link wired to "part 3" is scrambled by the next deletion. Of the two cures (fixed bus channels, or NEAT-style ids on parts), take the bus: it needs no ids, the brain plan stays a fixed size, and losing a sensor weakens a signal instead of rewiring the brain. Ids can wait for 9.4, which needs them for recombination anyway.
- Per channel the brain gets two inputs: **level** (how much, all sensors on that channel together) and **side contrast** (left against right). A third, fore-and-aft contrast, is optional (open below).
- Each sensor adds its reading to the left or right sum by where it sits (weights from its angle, in a table built at birth, like `BodyPlan`).
- **Why level and contrast, not left and right:** with raw left and right inputs, turning towards something needs two placed sensors and two links of opposite sign before anything pays. Today one link is a working behaviour, and movers are already rare. With a contrast input, one pair and one link (contrast → turn) is taxis again.
- **Scale (a third trap):** light runs 0 to 2, corpse energy 0 to 250 and more, loose chemicals around 0.01, and weights stop at ±3 (`MAX_WEIGHT`). Raw readings would saturate or vanish. So each is normalised per channel, as receptors do: level = S / (S + k), contrast = (L − R) / (L + R + k), with one constant k per field. Contrast is 0 unless both sides have a sensor on that channel.

**The field table.** One row per field: its name, where its numbers live, its k. Adding a sense is adding a row.
- From the start: light; warmth against the body's own comfort (signed); the dead (corpse energy); soil A; loose B, C and D; moving mass; the size of bodies there against the reader's own; and living mass, split into a few **hue bands**.
- **Kin** is then "mass in my own hue band", so the kin sense no longer reads species ids, and species go back to being observation only (9.1). A band holds plain living mass, since camouflage is removed (14.8). If hunting by how a body shows against its ground comes back, it comes back here: a band would hold mass × how plainly it shows.
- Later, a row each: scent (signals, 2.6), depth and wetness (2.7), gametes (9.4).
- The three per-body fields (moving mass, size, hue bands) are laid down in phase 0, only for cells that hold a body (14.5 item 1 is a prerequisite).

**What the body feels without a sensor** stays as now: a constant, hunger, light on its skin, crowding, cold, own speed, eating, hurt. One is added: **noise**, a number from a hash of (creature id, tick), so a brain can be wired to wander. It draws nothing from the `Rng`, which is what frees phase 1 for threads (14.5 item 4; all draws go this way under 14.10.4).

**Effectors.**
- **Thrust,** −1 to 1 of what the motors can give along the heading (by where they sit, `thrust_facing`, as now).
- **Turn,** −1 to 1 of the turning the motors can give (by their lever arm: side-mounted parts turn best, a tail straight behind a little; numbers to pick).
- **Mouth** (stage 2), 0 to 1: scales the pull. No brain: 1, as today.
- **Sleep** (stage 3), 0 to 1: drives the `sleep` level, capped by the `dormancy` gene.
- **Heading becomes state:** an angle that the turn output changes. Today it is "the way it last moved". Mouth and armour arcs already follow the heading, so a body can now aim them.
- **Turning is paid for:** the distance its rim travels (radius × angle turned) counts as distance in `movement_cost` and in `moved`. So turning costs energy, falls under the power cap, gives a body away, and folds its leaves past `FOLD_STRIDE`. Otherwise rotation is free and rooted bodies spin.

**What one mutation buys** (the check on evolvability):
- A sensor pair and one link, contrast → turn: steering towards or away, by the sign.
- One midline eye, one slow neuron and the noise input: run-and-tumble (turn less while the level is above what the neuron remembers). A first real use for `tau`.
- Hurt → thrust: fleeing. Eating → less thrust: stopping on prey. Both exist today as links to the throttle.
- **The first brain,** when a motor appears on a body with none: constant → thrust and noise → turn, plus a few random links. It wanders as today's first movers do.

**Node addresses.** Hidden and output node numbers are worked out from `NUM_INPUTS` today, so adding one input renumbers every link in every saved brain. Links address a node as (kind, index), with the channel's own id for bus inputs. A new field row then changes no existing brain.

**Stages,** each judged on forks, then on six fresh seeds, with Uday's go-ahead:
1. **Sensors and motors.** Everything above except the mouth and sleep outputs. A brain still needs a motor. Creature files go to version 2; old brains are translated roughly (a lean link becomes contrast → turn on that field's channel, and the body's sensors are given the channels its brain used). Behaviour will not carry over exactly; with 5 and 2 movers a world at tick 100,000, little is lost. *Judge:* first movers appear no later than now; movers persist; the share of brains carrying a contrast → turn link grows; ms/tick per mover is no higher.
2. **Brains without motors, and the mouth gate** (2.8 C and 2.9 C). A brain exists where there is any effector for it to drive. Brain plans move to their own short array first (14.5 item 3), and only bodies with links think. *Risk:* thinking is nearly free, so brains may drift onto plants as sensors did (14.3). *Judge:* how many rooted bodies carry a brain, and whether the gate is wired to anything (eating, own hue band, hunger).
3. **The sleep drive.** The dark-or-cold trigger goes; the sleep output takes its place, with falling asleep and waking at today's rates. Kept apart because dormancy is the most used optional trait (mean 0.26 to 0.82, section 11) and works with no brain today: after this, every sleeper needs a link. Genomes with `dormancy` are given the two links (dark → sleep, cold → sleep) on loading. *Judge:* dormancy as common as before; new habits appear (sleeping off a meal, waiting in ambush). *Open:* whether a brainless body with the gene keeps the old reflex; decide on a fork.

**Needs first:** the harness (14.7 step 0) and 14.5 items 1 and 3.

**Outside the sim:** the designer's reflex cards and `brain_graph.gd` are built on the named urges (the right-hand column becomes four effectors, the left gains a row per channel); the inspector and species card wording; `probe_brain`; `test_touch` is rewritten and `test_wander_turns_by_distance` goes with the rule it tests.

**What it will not do:** make movers common. It raises what behaviour can evolve once something is worth chasing or fleeing; that is the ecology of roadmap step 2.

**Open before building:** how many hue bands (4 to 6); whether fore-and-aft contrast earns its inputs; turning strength per motor type; whether thrust may be negative (backing up); k for each field; whether `MAX_LINKS` (48) is still enough with more inputs.
