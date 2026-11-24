#!/usr/bin/env python3
"""Map of the light that reaches the PMT, from the optical position scan (optical_scan.sh).

One plot in the plane of a module: scintillator grid (panel_x*_y*.root) and
light-guide grid (lg_x*_y*.root) together, with the outline of the panel, the
guide and the PMT window. Per beam position (vertical muon):
    left : detected photons per muon (scintillation + Cherenkov at the window)
    right: collection efficiency = detected / produced
and a profile against the distance from the PMT window.

Usage:  python3 photon_map.py Optical_Scan_alfoil_standard [--module all|0|1|2|3] [--usetex]
"""
import argparse
import glob
import os
import re

import numpy as np
import uproot
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt                     # noqa: E402
from matplotlib.colors import LogNorm               # noqa: E402

X_WIN = -210.0                                      # mm, PMT window (end of the guide)


def point(path):
    m = re.search(r"_x(-?\d+)_y(-?\d+)\.root$", path)
    return float(m.group(1)), float(m.group(2))


def load(scan_dir, prefix, modules):
    """[(x, y, detected per muon, efficiency, n_events)] for one grid."""
    out = []
    for path in sorted(glob.glob(os.path.join(scan_dir, f"{prefix}_x*_y*.root"))):
        t = uproot.open(path)["OpticalSummary"].arrays(library="np")
        det = sum(t[f"detScint_sc{m}"] + t[f"detCher_sc{m}"] + t[f"detCher_lg{m}"] for m in modules)
        prod = sum(t[f"prodScint_sc{m}"] + t[f"prodCher_sc{m}"] + t[f"prodCher_lg{m}"] for m in modules)
        n = len(det)
        out.append((*point(path), det.sum() / n / len(modules), det.sum() / max(prod.sum(), 1), n))
    return np.array(out)


def outline(ax):
    ax.plot([-140, 140, 140, -140, -140], [-140, -140, 140, 140, -140], c="k", lw=0.8)       # panel
    ax.plot([-140, X_WIN, X_WIN, -140], [140, 30, -30, -140], c="k", lw=0.8)                 # guide
    ax.add_patch(plt.Rectangle((X_WIN - 8, -25), 8, 50, color="0.3"))                       # PMT window
    ax.text(X_WIN - 10, 32, "PMT", fontsize=7, ha="center")


def tile(ax, d, values, norm, cmap="viridis"):
    """Each beam position as a square of the grid spacing."""
    for (x, y), v in zip(d[:, :2], values):
        s = 40.0 if x > -140 else 10.0                                     # panel / guide grid spacing (x)
        h = 40.0 if x > -140 else 20.0
        ax.add_patch(plt.Rectangle((x - s / 2, y - h / 2), s, h, color=plt.get_cmap(cmap)(norm(v)), lw=0))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("scan_dir")
    ap.add_argument("--module", default="all")
    ap.add_argument("--usetex", action="store_true")
    ap.add_argument("-o", default=None)
    a = ap.parse_args()
    plt.rcParams.update({"text.usetex": a.usetex, "font.family": "serif", "font.size": 9,
                         "xtick.direction": "in", "ytick.direction": "in"})
    modules = range(4) if a.module == "all" else [int(a.module)]
    sc, lg = load(a.scan_dir, "panel", modules), load(a.scan_dir, "lg", modules)
    d = np.vstack([g for g in (sc, lg) if len(g)])

    fig, axs = plt.subplots(1, 2, figsize=(7.2, 3.2), sharey=True)
    for ax, col, lab, norm in (
            (axs[0], 2, "Detected photons per muon", LogNorm(d[:, 2].min(), d[:, 2].max())),
            (axs[1], 3, "Collection efficiency", plt.Normalize(0, d[:, 3].max()))):
        tile(ax, d, d[:, col], norm)
        outline(ax)
        sm = plt.cm.ScalarMappable(norm=norm, cmap="viridis")
        fig.colorbar(sm, ax=ax, fraction=0.05, pad=0.02, label=lab)
        ax.set_xlim(-225, 145); ax.set_ylim(-150, 150); ax.set_aspect("equal")
        ax.set_xlabel("$x$ (mm)")
    axs[0].set_ylabel("$y$ (mm)")
    fig.tight_layout()
    out = a.o or a.scan_dir
    fig.savefig(os.path.join(out, f"photon_map_module_{a.module}.pdf"))

    fig, ax = plt.subplots(figsize=(3.6, 2.6))
    for g, mk, lab in ((sc, "o", "scintillator (scint. + Cherenkov)"), (lg, "s", "light guide (Cherenkov)")):
        if len(g):
            dist = g[:, 0] - X_WIN
            for x in np.unique(dist):
                ax.plot(x, g[dist == x, 2].mean(), mk, ms=4, c="k" if mk == "o" else "C3")
            ax.plot([], [], mk, c="k" if mk == "o" else "C3", label=lab)
    ax.set_yscale("log"); ax.set_xlabel("Distance of the muon from the PMT window (mm)")
    ax.set_ylabel("Detected photons per muon"); ax.legend(frameon=False, fontsize=7)
    fig.tight_layout(); fig.savefig(os.path.join(out, f"photon_profile_module_{a.module}.pdf"))
    print(f"{len(sc)} panel points, {len(lg)} guide points -> {out}")
    for name, g in (("panel", sc), ("guide", lg)):
        if len(g):
            print(f"{name}: detected per muon {g[:, 2].min():.0f}-{g[:, 2].max():.0f}, "
                  f"efficiency {g[:, 3].min():.3f}-{g[:, 3].max():.3f}")


if __name__ == "__main__":
    main()