#!/usr/bin/env python3
"""The shelf of mature worlds (design doc 14.6.5, 14.10.1).

Runs fresh worlds from LUCA and keeps snapshots of them at chosen ticks, for
tools/ab.py to fork from. A world already on the shelf is continued from its
latest snapshot, never started again, so the shelf can be extended to longer
runs as it is needed.

  tools/shelf.py                          # 30 seeds to 1,000,000 ticks (long)
  tools/shelf.py --ticks 100000           # the same seeds, only to 100,000
  tools/shelf.py --kind vents             # six seeds with the four test vents, to 100,000
  tools/shelf.py --kind events            # six seeds with world events at rate 1, to 100,000

Worlds are written as worlds/<kind>_s<seed>_<tick>.snap ("plain" for none), a
run's metrics and chronicle under worlds/runs/. The folder is git-ignored;
rebuild it whenever the snapshot format changes in a way old files can't be read.
"""

import argparse
import concurrent.futures as cf
import os
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
BINARY = ROOT / "build" / "luca_headless"

USUAL = [42, 7, 99, 1, 2, 3]
THIRTY = USUAL + [s for s in range(4, 29) if s != 7]   # 4 to 28, less 7 (already usual)

KINDS = {
    # kind: (default seeds, default ticks, ticks to keep,
    #        flags for a new world only, flags for every leg of the run)
    # A script is cast once, at the start: cast again on a loaded world it would happen twice.
    "plain": (THIRTY, 1_000_000, "100000,200000,500000,1000000", [], []),
    "vents": (USUAL, 100_000, "100000", ["--script", str(ROOT / "scripts" / "four_vents.txt")], []),
    "events": (USUAL, 100_000, "100000", [], ["--events", "1"]),
}


def snapshots(folder, kind, seed):
    """{tick: path} for one world already on the shelf."""
    pat = re.compile(rf"^{kind}_s{seed}_(\d+)\.snap$")
    found = {}
    for p in folder.glob(f"{kind}_s{seed}_*.snap"):
        m = pat.match(p.name)
        if m:
            found[int(m.group(1))] = p
    return found


def build(binary, folder, kind, seed, ticks, keep, every, at_start, always):
    have = snapshots(folder, kind, seed)
    done = [t for t in have if t <= ticks]
    start = max(done) if done else 0
    if start >= ticks:
        return seed, f"already at {start:,}"
    save = folder / f"{kind}_s{seed}.snap"
    run_dir = folder / "runs" / f"{kind}_s{seed}_from{start}"
    run_dir.mkdir(parents=True, exist_ok=True)
    cmd = [str(binary), "--ticks", str(ticks - start), "--quiet", "--no-snapshot", "--interval", "1000",
           "--save", str(save), "--save-every", str(every), "--out", str(run_dir)]
    cmd += ["--load", str(have[start])] if start else ["--seed", str(seed), *at_start]
    cmd += always
    with open(run_dir / "stdout.txt", "w") as out:
        rc = subprocess.run(cmd, stdout=out, stderr=subprocess.STDOUT).returncode
    if rc != 0:
        return seed, f"failed ({run_dir / 'stdout.txt'})"
    # The last save has no tick in its name; give it one, then keep only the chosen ticks.
    final_tick = ticks
    for line in (run_dir / "stdout.txt").read_text().splitlines():
        m = re.match(r"done: (\d+) ticks", line)
        if m:
            final_tick = int(m.group(1))
    if save.exists():
        save.rename(folder / f"{kind}_s{seed}_{final_tick}.snap")
    for t, p in snapshots(folder, kind, seed).items():
        if t not in keep and t != final_tick:
            p.unlink()
    note = f"{start:,} -> {final_tick:,}"
    if final_tick < ticks:
        note += " (extinct)"
    return seed, note


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--kind", choices=sorted(KINDS), default="plain")
    ap.add_argument("--seeds", help="comma-separated seeds (default: 30 for plain, the usual six otherwise)")
    ap.add_argument("--ticks", type=int, help="run each world to this tick")
    ap.add_argument("--keep", help="ticks to keep snapshots of, comma-separated (the last is always kept)")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--folder", default=str(ROOT / "worlds"))
    ap.add_argument("--binary", default=str(BINARY))
    a = ap.parse_args()

    seeds0, ticks0, keep0, at_start, always = KINDS[a.kind]
    seeds = [int(s) for s in a.seeds.split(",")] if a.seeds else seeds0
    ticks = a.ticks or ticks0
    keep = {int(t) for t in (a.keep or keep0).split(",")}
    keep.add(ticks)
    every = min(keep) if keep else ticks
    for t in keep:   # save often enough to land on every kept tick
        every = gcd(every, t)
    if not Path(a.binary).exists():
        sys.exit(f"{a.binary} not found: build first (cmake --build build -j)")
    folder = Path(a.folder)
    folder.mkdir(parents=True, exist_ok=True)

    print(f"{a.kind}: {len(seeds)} worlds to {ticks:,} ticks, keeping {sorted(keep)}, {a.jobs} at a time")
    with cf.ThreadPoolExecutor(max_workers=a.jobs) as pool:
        futs = [pool.submit(build, a.binary, folder, a.kind, s, ticks, keep, every, at_start, always) for s in seeds]
        for f in cf.as_completed(futs):
            seed, note = f.result()
            print(f"  seed {seed}: {note}", flush=True)


def gcd(x, y):
    while y:
        x, y = y, x % y
    return x


if __name__ == "__main__":
    main()
