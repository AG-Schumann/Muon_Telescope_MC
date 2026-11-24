#!/usr/bin/env python3
"""Light-collection analysis of the optical position scans (optical_scan.sh).

Input: a scan directory with one ROOT file per beam position,
    panel_x<X>_y<Y>.root   vertical muon through the scintillator panel
    lg_x<X>_y<Y>.root      vertical muon through the light guide only
    ref_panel.root         panel centre, PhotonHits stored (wavelengths, times)
    ref_lg.root            guide at x = -175 mm, PhotonHits stored

Computed:
    1. eps_sc(x, y) = detected / produced scintillation photons per module,
       as maps and as the area average over the grid.
    2. eps_LG(x, y) for the Cherenkov photons produced in the light guide.
    3. c = eps_LG / eps_sc per module and averaged.
    4. With a quantum-efficiency curve: photoelectrons of a panel muon and
       of a guide crossing (medians), their ratio as a pulse height in units
       of the muon signal, and the fraction of guide crossings above 0.11
       and 0.39 of the muon signal (the -30 mV and -110 mV thresholds).
       The QE is sampled per photon (binomial) where the wavelengths are
       stored (ref files); for the scan points the spectrum-averaged QE of
       the reference run is used, which gives the same photoelectron
       statistics to very good approximation.
    5. Pulse shapes from the arrival times of the reference runs: sum of
       single-photoelectron pulses (Gaussian, FWHM 4.5 ns), and the pulse
       amplitude per photoelectron of the scintillation and Cherenkov signal.

Usage:
    python3 analysis/light_collection.py Optical_Scan_alfoil_standard [-o outdir] [--usetex]
"""

import argparse
import glob
import os
import re
import sys

import numpy as np
import pandas as pd
import uproot
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

# ── Constants ────────────────────────────────────────────────────
N_MODULES = 4
THRESHOLDS = (0.11, 0.39)        # -30 mV and -110 mV, in units of the muon signal
SPE_FWHM_NS = 4.5                # single-photoelectron pulse width
RNG = np.random.default_rng(12345)

# Approximate bialkali QE of the ET 9214B (to be replaced by the data-sheet
# curve). Wavelength in nm, QE as a fraction.
QE_TABLE = np.array([
    [300, 0.15], [320, 0.24], [350, 0.28], [380, 0.28], [400, 0.27],
    [420, 0.24], [450, 0.20], [480, 0.15], [500, 0.12], [550, 0.06],
    [600, 0.02], [650, 0.005],
])


def qe(wavelength_nm):
    """Quantum efficiency, linear interpolation, zero outside the table."""
    return np.interp(wavelength_nm, QE_TABLE[:, 0], QE_TABLE[:, 1], left=0.0, right=0.0)


# ── Figure style ─────────────────────────────────────────────────
def set_style(usetex=False):
    plt.rcParams.update({
        "font.family": "serif",
        "font.size": 9,
        "axes.labelsize": 9,
        "legend.fontsize": 8,
        "xtick.direction": "in",
        "ytick.direction": "in",
        "xtick.top": True,
        "ytick.right": True,
        "xtick.minor.visible": True,
        "ytick.minor.visible": True,
        "figure.dpi": 150,
        "savefig.bbox": "tight",
        "text.usetex": usetex,
    })


# ── Input ────────────────────────────────────────────────────────
def point_from_name(path):
    m = re.search(r"_x(-?\d+)_y(-?\d+)\.root$", os.path.basename(path))
    return int(m.group(1)), int(m.group(2))


def load_summary(path):
    with uproot.open(path) as f:
        return f["OpticalSummary"].arrays(library="pd")


def load_photons(path):
    with uproot.open(path) as f:
        if "PhotonHits" not in f:
            return None
        ph = f["PhotonHits"].arrays(library="pd")
    return ph if len(ph) else None


def load_grid(scan_dir, prefix, prod_col, det_col, extra_col=None):
    """Per point and module: produced, detected and the per-event counts.
    extra_col: another detected-count column kept per event (panel Cherenkov)."""
    rows, events = [], []
    for path in sorted(glob.glob(os.path.join(scan_dir, f"{prefix}_x*_y*.root"))):
        x, y = point_from_name(path)
        o = load_summary(path)
        for m in range(N_MODULES):
            prod = o[f"{prod_col}{m}"].to_numpy()
            det = o[f"{det_col}{m}"].to_numpy()
            extra = o[f"{extra_col}{m}"].to_numpy() if extra_col else np.zeros_like(det)
            rows.append(dict(x=x, y=y, module=m, nev=len(o),
                             produced=prod.sum(), detected=det.sum(),
                             eps=det.sum() / prod.sum() if prod.sum() else np.nan))
            events.append(pd.DataFrame(dict(x=x, y=y, module=m, event=o["eventID"],
                                            produced=prod, detected=det, extra=extra)))
    if not rows:
        sys.exit(f"no {prefix}_x*_y*.root files in {scan_dir}")
    return pd.DataFrame(rows), pd.concat(events, ignore_index=True)


# ── Efficiency maps ──────────────────────────────────────────────
def plot_maps(df, title, fname, outdir):
    xs, ys = sorted(df.x.unique()), sorted(df.y.unique())
    fig, axes = plt.subplots(1, N_MODULES, figsize=(7.0, 2.1), sharey=True)
    vmin, vmax = df.eps.min(), df.eps.max()
    for m, ax in enumerate(axes):
        grid = df[df.module == m].pivot(index="y", columns="x", values="eps")
        grid = grid.reindex(index=ys, columns=xs)
        im = ax.pcolormesh(xs, ys, grid.to_numpy(), shading="nearest",
                           vmin=vmin, vmax=vmax, cmap="viridis")
        ax.set_title(f"module {m}", fontsize=9)
        ax.set_xlabel("x [mm]")
        ax.set_aspect("equal")
    axes[0].set_ylabel("y [mm]")
    cb = fig.colorbar(im, ax=axes, shrink=0.9, pad=0.02)
    cb.set_label(title)
    fig.savefig(os.path.join(outdir, fname))
    plt.close(fig)


def plot_profile(df, xlabel, ylabel, fname, outdir):
    """Efficiency against x, averaged over y, one line per module."""
    fig, ax = plt.subplots(figsize=(3.4, 2.4))
    for m in range(N_MODULES):
        d = df[df.module == m].groupby("x").eps.mean()
        ax.plot(d.index, d.values, "o-", ms=3, lw=1, label=f"module {m}")
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.legend(frameon=False)
    fig.savefig(os.path.join(outdir, fname))
    plt.close(fig)


# ── Photoelectrons ───────────────────────────────────────────────
def mean_qe(photons, origin, process):
    sel = photons[(photons.origin == origin) & (photons.process == process)]
    if not len(sel):
        return np.nan, 0
    return qe(sel.wavelength_nm.to_numpy()).mean(), len(sel)


def npe_per_event(photons, origin, process):
    """Photoelectrons per (event, module), binomial per photon."""
    sel = photons[(photons.origin == origin) & (photons.process == process)]
    hit = RNG.random(len(sel)) < qe(sel.wavelength_nm.to_numpy())
    return sel.assign(pe=hit.astype(int)).groupby(["eventID", "module"]).pe.sum()


def npe_from_counts(events, qe_mean):
    """Binomial photoelectrons from the detected counts of the scan points."""
    return RNG.binomial(events.detected.to_numpy(), qe_mean)


# ── Pulse shapes ─────────────────────────────────────────────────
def pulse_shape(times_ns, t_grid):
    """Sum of single-photoelectron pulses, unit area each, Gaussian."""
    sigma = SPE_FWHM_NS / 2.355
    pulse = np.zeros_like(t_grid)
    for t in times_ns:
        pulse += np.exp(-0.5 * ((t_grid - t) / sigma) ** 2)
    return pulse / (sigma * np.sqrt(2 * np.pi))


def amplitude_per_pe(photons, origin, process, outdir, label):
    """Average pulse of the reference run and its peak amplitude per
    photoelectron, after QE sampling. Times are relative to the first
    photoelectron of each event and module."""
    sel = photons[(photons.origin == origin) & (photons.process == process)].copy()
    sel = sel[RNG.random(len(sel)) < qe(sel.wavelength_nm.to_numpy())]
    if not len(sel):
        return np.nan
    sel["t_rel"] = sel.time_ns - sel.groupby(["eventID", "module"]).time_ns.transform("min")
    t_grid = np.linspace(-10, 100, 1101)
    amps, mean_pulse = [], np.zeros_like(t_grid)
    groups = list(sel.groupby(["eventID", "module"]))
    for _, g in groups:
        p = pulse_shape(g.t_rel.to_numpy(), t_grid)
        amps.append(p.max() / len(g))
        mean_pulse += p / len(groups)
    fig, ax = plt.subplots(figsize=(3.4, 2.4))
    ax.plot(t_grid, mean_pulse, lw=1)
    ax.set_xlabel("time after first photoelectron [ns]")
    ax.set_ylabel("pulse [p.e. / ns]")
    ax.set_title(label, fontsize=9)
    fig.savefig(os.path.join(outdir, f"pulse_{label}.pdf"))
    plt.close(fig)
    return float(np.mean(amps))


# ── Main ─────────────────────────────────────────────────────────
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("scan_dir")
    ap.add_argument("-o", "--outdir", default=None, help="figures and tables (default: <scan_dir>/analysis)")
    ap.add_argument("--usetex", action="store_true")
    args = ap.parse_args()
    set_style(args.usetex)
    outdir = args.outdir or os.path.join(args.scan_dir, "analysis")
    os.makedirs(outdir, exist_ok=True)

    # 1. eps_sc
    sc, sc_events = load_grid(args.scan_dir, "panel", "prodScint_sc", "detScint_sc", "detCher_sc")
    eps_sc_mod = sc.groupby("module").eps.mean()          # area average per module
    eps_sc = eps_sc_mod.mean()
    plot_maps(sc, r"$\epsilon_{\rm sc}$", "eps_sc_maps.pdf", outdir)
    plot_profile(sc, "x [mm]", r"$\epsilon_{\rm sc}$ (mean over $y$)", "eps_sc_profile.pdf", outdir)

    # 2. eps_LG
    lg, lg_events = load_grid(args.scan_dir, "lg", "prodCher_lg", "detCher_lg")
    eps_lg_mod = lg.groupby("module").eps.mean()
    eps_lg = eps_lg_mod.mean()
    plot_maps(lg, r"$\epsilon_{\rm LG}$", "eps_lg_maps.pdf", outdir)
    plot_profile(lg, "x [mm]", r"$\epsilon_{\rm LG}$ (mean over $y$)", "eps_lg_profile.pdf", outdir)

    # 3. c
    c_mod = eps_lg_mod / eps_sc_mod
    c = eps_lg / eps_sc

    # 4. Photoelectrons
    ref_panel = load_photons(os.path.join(args.scan_dir, "ref_panel.root"))
    ref_lg = load_photons(os.path.join(args.scan_dir, "ref_lg.root"))
    if ref_panel is None or ref_lg is None:
        sys.exit("ref_panel.root / ref_lg.root with PhotonHits are needed for the QE step")
    qe_sc, n_sc = mean_qe(ref_panel, 0, 0)       # scintillation photons from the panel
    qe_lg, n_lg = mean_qe(ref_lg, 1, 1)          # Cherenkov photons from the guide
    # Muon in the panel: scintillation plus the Cherenkov light of the panel,
    # which is a few per cent and arrives with the same signal.
    qe_sc_cher, _ = mean_qe(ref_panel, 0, 1)
    npe_mu = (npe_from_counts(sc_events, qe_sc)
              + RNG.binomial(sc_events.extra.to_numpy(), np.nan_to_num(qe_sc_cher)))
    npe_lg = npe_from_counts(lg_events, qe_lg)
    npe_mu_med = float(np.median(npe_mu))
    npe_lg_med = float(np.median(npe_lg))
    ratio = npe_lg_med / npe_mu_med
    frac_above = {thr: float(np.mean(npe_lg > thr * npe_mu_med)) for thr in THRESHOLDS}

    # per-photon sampling check on the reference runs
    ref_mu = npe_per_event(ref_panel, 0, 0)
    ref_cher = npe_per_event(ref_lg, 1, 1)

    fig, ax = plt.subplots(figsize=(3.4, 2.4))
    bins = np.linspace(0, max(npe_mu.max(), 1) * 1.05, 60)
    ax.hist(npe_mu, bins=bins, histtype="step", lw=1, label="panel muon")
    ax.hist(npe_lg, bins=bins, histtype="step", lw=1, label="guide crossing")
    for thr in THRESHOLDS:
        ax.axvline(thr * npe_mu_med, color="0.5", lw=0.8, ls="--")
    ax.set_xlabel("photoelectrons")
    ax.set_ylabel("events x modules")
    ax.set_yscale("log")
    ax.legend(frameon=False)
    fig.savefig(os.path.join(outdir, "photoelectrons.pdf"))
    plt.close(fig)

    # detected spectra
    fig, ax = plt.subplots(figsize=(3.4, 2.4))
    bins = np.arange(300, 652, 5)
    ax.hist(ref_panel[(ref_panel.origin == 0) & (ref_panel.process == 0)].wavelength_nm,
            bins=bins, histtype="step", lw=1, density=True, label="scintillation, panel")
    ax.hist(ref_lg[(ref_lg.origin == 1) & (ref_lg.process == 1)].wavelength_nm,
            bins=bins, histtype="step", lw=1, density=True, label="Cherenkov, guide")
    ax2 = ax.twinx()
    ax2.plot(QE_TABLE[:, 0], QE_TABLE[:, 1], "k:", lw=1, label="QE (approx.)")
    ax2.set_ylabel("QE")
    ax.set_xlabel("wavelength at the window [nm]")
    ax.set_ylabel("detected photons [a.u.]")
    ax.legend(frameon=False, loc="upper right")
    fig.savefig(os.path.join(outdir, "spectra.pdf"))
    plt.close(fig)

    # 5. Pulse shapes
    amp_sc = amplitude_per_pe(ref_panel, 0, 0, outdir, "scintillation")
    amp_ch = amplitude_per_pe(ref_lg, 1, 1, outdir, "cherenkov")

    # ── Output ───────────────────────────────────────────────────
    sc.to_csv(os.path.join(outdir, "eps_sc_points.csv"), index=False)
    lg.to_csv(os.path.join(outdir, "eps_lg_points.csv"), index=False)
    per_module = pd.DataFrame({"eps_sc": eps_sc_mod, "eps_LG": eps_lg_mod, "c": c_mod})
    per_module.to_csv(os.path.join(outdir, "per_module.csv"))
    summary = pd.Series({
        "eps_sc": eps_sc, "eps_LG": eps_lg, "c": c,
        "QE_mean_scint": qe_sc, "QE_mean_cherenkov_lg": qe_lg,
        "npe_muon_median": npe_mu_med, "npe_guide_median": npe_lg_med,
        "pulse_height_ratio": ratio,
        "frac_guide_above_0.11": frac_above[0.11], "frac_guide_above_0.39": frac_above[0.39],
        "amp_per_pe_scint": amp_sc, "amp_per_pe_cherenkov": amp_ch,
        "amplitude_ratio": ratio * amp_ch / amp_sc if amp_sc else np.nan,
        "n_panel_points": sc.groupby(["x", "y"]).ngroups, "n_lg_points": lg.groupby(["x", "y"]).ngroups,
    })
    summary.to_csv(os.path.join(outdir, "summary.csv"), header=False)

    print(f"scan: {args.scan_dir}")
    print(per_module.round(4).to_string())
    print(f"\neps_sc (area average)       = {eps_sc:.4f}")
    print(f"eps_LG (guide grid average) = {eps_lg:.4f}")
    print(f"c = eps_LG / eps_sc         = {c:.3f}")
    print(f"\n<QE> scintillation (panel) = {qe_sc:.3f}  ({n_sc} photons)")
    print(f"<QE> Cherenkov (guide)     = {qe_lg:.3f}  ({n_lg} photons)")
    print(f"photoelectrons, panel muon  median = {npe_mu_med:.0f}  (ref run, per-photon sampling: {np.median(ref_mu):.0f})")
    print(f"photoelectrons, guide       median = {npe_lg_med:.0f}  (ref run, per-photon sampling: {np.median(ref_cher):.0f})")
    print(f"pulse height of a guide crossing   = {ratio:.3f} of the muon signal (charge)")
    for thr in THRESHOLDS:
        print(f"fraction of guide crossings above {thr:.2f} x muon = {frac_above[thr]:.3f}")
    print(f"amplitude per p.e.: scintillation {amp_sc:.4f}, Cherenkov {amp_ch:.4f} (p.e./ns, FWHM {SPE_FWHM_NS} ns)")
    print(f"amplitude ratio guide/muon         = {summary['amplitude_ratio']:.3f}")
    print(f"\noutput in {outdir}")


if __name__ == "__main__":
    main()
