#!/usr/bin/env python3
"""Paired comparisons and sweeps (design doc 14.6.4).

Runs a control and one or more variants over the same set of pairs and says,
for each metric, how far the variant moved it, how much that varies between
pairs, and in how many pairs it moved the same way.

A pair is either a fork of a saved world (--worlds: each world times each of
--forks fork seeds, so control and variant share the world and the dice) or a
fresh world from LUCA (--seeds). Forks are the fast screen: what a rule does to
a mature ecosystem. Fresh seeds are the check before adopting: what evolves
under it. Several --variant flags make a sweep, each against the same control.

  # what does a smaller meal share do to the shelf of mature worlds?
  tools/ab.py --worlds worlds/plain_s*_100000.snap --forks 2 --ticks 20000 \\
      --variant "--set TRANSFER_EFFICIENCY=0.5"

  # a sweep of one law over six fresh seeds
  tools/ab.py --seeds 42,7,99,1,2,3 --ticks 100000 \\
      --variant "--set UNFOLD_TICKS=200" --variant "--set UNFOLD_TICKS=1200"

  # a knockout: who depends on species Maral?
  tools/ab.py --worlds worlds/plain_s42_100000.snap --forks 6 --ticks 10000 \\
      --variant "--knockout Maral"

Metrics are means over the last --window of each run (default the last
fifth). Every run's own output stays under --out, so a surprise can be read
run by run. The pairs run in parallel, one process a core (--jobs).
"""

import argparse
import concurrent.futures as cf
import csv
import math
import os
import shlex
import statistics
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
BINARY = HERE.parent / "build" / "luca_headless"

# (column, label, how): "mean" of the rows in the window, or "rate" (an
# interval sum, given per tick).
METRICS = [
    ("population", "population", "mean"),
    ("species_established", "established species", "mean"),
    ("producers", "sun-fed", "mean"),
    ("hunters", "hunters", "mean"),
    ("eyed", "movers with eyes", "mean"),
    ("steering", "steered by side contrast", "mean"),
    ("scavengers", "scavengers", "mean"),
    ("cleaners", "cleaners", "mean"),
    ("chemotrophs", "vent-fed", "mean"),
    ("mixed", "mixed feeders", "mean"),
    ("movers", "movers", "mean"),
    ("brains", "running brains", "mean"),
    ("loose_B", "loose B (units)", "mean"),
    ("loose_C", "loose C (units)", "mean"),
    ("energy_in", "energy in /tick", "rate"),
    ("eaten_living", "eaten living /tick", "rate"),
    ("eaten_dead", "eaten dead /tick", "rate"),
    ("out_decay", "corpses weathered /tick", "rate"),
    ("eaten_waste", "taken up /tick", "rate"),
    ("ms_per_tick", "ms/tick", "mean"),
]

# From probe.csv (with --probe): shares, as ratios of sums over the window.
PROBES = [
    ("kept_living", "meal_living", "living meal kept"),
    ("spill_room", "meal_living", "living meal spilled, no room"),
    ("meal_by_full", "meal_living", "living meal taken by full stores"),
    ("kept_dead", "meal_dead", "dead meal kept"),
    ("meal_dead", "corpse_decay", "corpse eaten / weathered"),
    ("deaths_drained", "deaths", "deaths while drained"),
    ("deaths_could_pay", "deaths", "deaths with enough in store"),
    ("deaths_poison", "deaths", "deaths mostly poison"),
]


def read_rows(path):
    try:
        with open(path, newline="") as f:
            return list(csv.DictReader(f))
    except FileNotFoundError:
        return []


def summarise(run_dir, start_tick, ticks, window):
    """Means over the last `window` of the run. Returns (values, note)."""
    rows = read_rows(run_dir / "metrics.csv")
    end = start_tick + ticks
    if not rows:
        return {}, "no metrics"
    last = int(rows[-1]["tick"])
    note = ""
    values = {}
    if last < end:   # extinct before the end: everything counted is 0
        note = f"extinct at tick {last}"
        for col, _, _ in METRICS:
            values[col] = 0.0 if col != "ms_per_tick" else float("nan")
        return values, note
    lo = end - window * ticks
    prev_tick = None
    picked = []   # (row, ticks it covers)
    for r in rows:
        t = int(r["tick"])
        if prev_tick is not None and t > lo:
            picked.append((r, t - prev_tick))
        prev_tick = t
    if not picked:
        picked = [(rows[-1], 1)]
    span = sum(dt for _, dt in picked)
    for col, _, how in METRICS:
        xs = [(float(r[col]), dt) for r, dt in picked if r.get(col) not in (None, "")]
        if not xs:
            continue
        if how == "rate":
            values[col] = sum(x for x, _ in xs) / span
        else:
            values[col] = sum(x for x, _ in xs) / len(xs)
    values["audit"] = max(abs(float(r["audit_rel_error"])) for r in rows)
    probe = read_rows(run_dir / "probe.csv")
    if probe:
        sums = {}
        for r in probe:
            if int(r["tick"]) > lo:
                for k, v in r.items():
                    if k != "tick":
                        sums[k] = sums.get(k, 0.0) + float(v)
        for num, den, label in PROBES:
            if sums.get(den):
                values["probe:" + label] = sums.get(num, 0.0) / sums[den]
    return values, note


def start_tick_of(snap):
    with open(snap, "rb") as f:
        for _ in range(4):
            line = f.readline().decode("ascii", "replace").strip()
            if line.startswith("tick "):
                return int(line.split()[1])
    sys.exit(f"{snap}: not a snapshot (no tick line in its header)")


def run(cmd, run_dir):
    run_dir.mkdir(parents=True, exist_ok=True)
    with open(run_dir / "stdout.txt", "w") as out:
        out.write(" ".join(shlex.quote(c) for c in cmd) + "\n")
        out.flush()
        return subprocess.run(cmd, stdout=out, stderr=subprocess.STDOUT).returncode


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--worlds", nargs="+", help="saved worlds to fork from")
    src.add_argument("--seeds", help="fresh worlds instead: comma-separated seeds")
    ap.add_argument("--forks", type=int, default=2, help="fork seeds per world (default 2)")
    ap.add_argument("--ticks", type=int, required=True, help="ticks to run each world")
    ap.add_argument("--variant", action="append", required=True,
                    help="extra flags for a variant (repeat for a sweep), e.g. \"--set X=1\"")
    ap.add_argument("--control", default="", help="extra flags for the control (default none)")
    ap.add_argument("--common", default="", help="extra flags for every run (e.g. \"--events 1\")")
    ap.add_argument("--window", type=float, default=0.2, help="share of each run averaged (default 0.2)")
    ap.add_argument("--probe", action="store_true", help="also run the probe ledgers and report them")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--out", default="ab_out", help="where every run's output goes (default ab_out)")
    ap.add_argument("--binary", default=str(BINARY))
    a = ap.parse_args()

    if not Path(a.binary).exists():
        sys.exit(f"{a.binary} not found: build first (cmake --build build -j)")
    out = Path(a.out)

    # Pairs: (name, flags that set up the world, start tick)
    pairs = []
    if a.worlds:
        for w in a.worlds:
            start = start_tick_of(w)
            for k in range(1, a.forks + 1):
                pairs.append((f"{Path(w).stem}_f{k}", ["--load", w, "--fork-seed", str(k)], start))
    else:
        for s in a.seeds.split(","):
            pairs.append((f"seed{s.strip()}", ["--seed", s.strip()], 0))

    names = [p[0] for p in pairs]
    if len(set(names)) != len(names):
        sys.exit("the same world or seed is listed twice: " + ", ".join(sorted({n for n in names if names.count(n) > 1})))

    arms = [("control", shlex.split(a.control))] + [(f"v{i + 1}", shlex.split(v)) for i, v in enumerate(a.variant)]
    jobs = []
    for name, setup, start in pairs:
        for arm, flags in arms:
            cmd = [a.binary, *setup, "--ticks", str(a.ticks), "--quiet", "--no-snapshot",
                   "--out", str(out / arm / name), *shlex.split(a.common), *flags]
            if a.probe:
                cmd.append("--probe")
            jobs.append((arm, name, start, cmd))

    print(f"{len(pairs)} pairs x {len(arms)} arms = {len(jobs)} runs of {a.ticks:,} ticks, {a.jobs} at a time")
    failed = []
    with cf.ThreadPoolExecutor(max_workers=a.jobs) as pool:
        futs = {pool.submit(run, cmd, out / arm / name): (arm, name) for arm, name, _, cmd in jobs}
        for done_n, f in enumerate(cf.as_completed(futs), 1):
            arm, name = futs[f]
            if f.result() != 0:
                failed.append(f"{arm}/{name}")
            print(f"\r  {done_n}/{len(jobs)} done", end="", flush=True)
    print()
    if failed:
        sys.exit("failed (see stdout.txt in each): " + ", ".join(failed))

    results = {}   # (arm, pair) -> values
    for arm, name, start, _ in jobs:
        values, note = summarise(out / arm / name, start, a.ticks, a.window)
        results[(arm, name)] = values
        if note:
            print(f"  {arm}/{name}: {note}")

    names = [p[0] for p in pairs]
    keys = [(col, label) for col, label, _ in METRICS] + [("probe:" + l, l) for _, _, l in PROBES]
    rows_out = []
    for arm, flags in arms[1:]:
        print(f"\n{arm}: {' '.join(flags)}   ({len(names)} pairs, last {a.window:.0%} of {a.ticks:,} ticks)")
        print(f"  {'metric':<34}{'control':>11}{'variant':>11}{'change':>10}{'sd':>10}{'same sign':>11}")
        for col, label in keys:
            c = [results[("control", n)].get(col) for n in names]
            v = [results[(arm, n)].get(col) for n in names]
            d = [y - x for x, y in zip(c, v) if x is not None and y is not None and not math.isnan(x) and not math.isnan(y)]
            if not d:
                continue
            cm = statistics.fmean(x for x in c if x is not None and not math.isnan(x))
            vm = statistics.fmean(y for y in v if y is not None and not math.isnan(y))
            dm = statistics.fmean(d)
            sd = statistics.stdev(d) if len(d) > 1 else 0.0
            sign = 1 if dm > 0 else -1 if dm < 0 else 0
            same = sum(1 for x in d if (x > 0 and sign > 0) or (x < 0 and sign < 0))
            pct = f"{dm / cm:+.0%}" if cm else ""
            fmt = (lambda x: f"{x:,.3g}") if abs(cm) < 10 else (lambda x: f"{x:,.0f}")
            print(f"  {label:<34}{fmt(cm):>11}{fmt(vm):>11}{pct:>10}{fmt(sd):>10}{same:>6}/{len(d)}")
            rows_out.append({"arm": arm, "flags": " ".join(flags), "metric": label, "control": cm, "variant": vm,
                             "change": dm, "sd": sd, "same_sign": same, "pairs": len(d)})
        worst = max(results[(arm, n)].get("audit", 0.0) for n in names)
        print(f"  worst audit error {worst:.1e}")
    with open(out / "summary.csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows_out[0].keys()) if rows_out else ["arm"])
        w.writeheader()
        w.writerows(rows_out)
    print(f"\nsummary: {out / 'summary.csv'}")


if __name__ == "__main__":
    main()
