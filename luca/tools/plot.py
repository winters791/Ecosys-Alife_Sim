#!/usr/bin/env python3
"""Plot a headless run.

    python tools/plot.py out/

Reads out/metrics.csv (and out/creatures.csv + out/world.csv if present)
and writes out/run.png (+ out/map.png).
"""

import csv
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402


def read_csv(path):
    with open(path, newline="") as f:
        rows = list(csv.DictReader(f))
    return {k: [float(r[k]) for r in rows] for k in rows[0]} if rows else {}


def plot_run(out_dir: Path):
    m = read_csv(out_dir / "metrics.csv")
    if not m:
        sys.exit("metrics.csv is empty")
    t = m["tick"]

    fig, ax = plt.subplots(3, 2, figsize=(12, 12), constrained_layout=True)

    a = ax[0][0]
    a.plot(t, m["population"], color="#2a6f97")
    a.set_title("Population")
    a.set_xlabel("tick")
    a.grid(alpha=0.3)

    a = ax[0][1]
    a.plot(t, m["births"], label="births / interval", color="#3a7d44")
    a.plot(t, m["deaths"], label="deaths / interval", color="#b5443b")
    a.set_title("Births and deaths")
    a.set_xlabel("tick")
    a.legend()
    a.grid(alpha=0.3)

    a = ax[1][0]
    a.plot(t, m["photo_bright"], label="photo size, sunny half", color="#e0a100")
    a.plot(t, m["photo_dim"], label="photo size, dim half", color="#5b5f97")
    a.plot(t, m["mean_radius"], label="core radius (all)", color="#888888", linestyle="--")
    a.set_title("Traits: sunny half vs dim half")
    a.set_xlabel("tick")
    a.legend()
    a.grid(alpha=0.3)

    a = ax[1][1]
    a.plot(t, m["audit_rel_error"], color="#b5443b", label="energy audit error")
    a.set_title("Energy audit (should hug 0) and speed")
    a.set_xlabel("tick")
    a.set_ylabel("relative error")
    a.grid(alpha=0.3)
    b = a.twinx()
    b.plot(t, m["ms_per_tick"], color="#2a6f97", alpha=0.6, label="ms / tick")
    b.set_ylabel("ms / tick")

    # Who lives how: counted from where each creature's energy really came from.
    if "producers" in m:
        a = ax[2][0]
        a.stackplot(t, m["producers"], m["mixed"], m["hunters"], m["scavengers"],
                    labels=["sun-fed", "mixed diet", "fed on the living", "fed on the dead"],
                    colors=["#3a7d44", "#c9a227", "#b5443b", "#6c5b7b"])
        a.plot(t, m["movers"], color="black", linewidth=1, label="able to move")
        a.set_title("Ways of life (nothing here is coded as a class)")
        a.set_xlabel("tick")
        a.legend(loc="upper left", fontsize=8)
        a.grid(alpha=0.3)

        a = ax[2][1]
        a.plot(t, m["energy_in"], label="from the sun", color="#e0a100")
        a.plot(t, m["eaten_living"], label="taken from the living", color="#b5443b")
        a.plot(t, m["eaten_dead"], label="taken from the dead", color="#6c5b7b")
        a.set_yscale("symlog", linthresh=10)
        a.set_title("Energy flows per interval")
        a.set_xlabel("tick")
        a.legend(fontsize=8)
        a.grid(alpha=0.3)

    path = out_dir / "run.png"
    fig.savefig(path, dpi=120)
    print(f"wrote {path}")


def plot_map(out_dir: Path):
    cpath, wpath = out_dir / "creatures.csv", out_dir / "world.csv"
    if not (cpath.exists() and wpath.exists()):
        return
    heights = []
    with open(wpath) as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            heights.append([float(v) for v in line.split(",")])
    c = read_csv(cpath)
    size = len(heights)

    fig, ax = plt.subplots(figsize=(8, 8), constrained_layout=True)
    ax.imshow(heights, cmap="terrain", origin="lower", extent=(0, size, 0, size))
    if c:
        sc = ax.scatter(c["x"], c["y"], c=c["photo"], s=2, cmap="viridis", alpha=0.8)
        fig.colorbar(sc, ax=ax, shrink=0.7, label="photo patch size")
    ax.set_title(f"End of run: {len(c.get('x', []))} creatures (sun strongest at y = 0)")
    path = out_dir / "map.png"
    fig.savefig(path, dpi=120)
    print(f"wrote {path}")


if __name__ == "__main__":
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "out")
    plot_run(out)
    plot_map(out)
