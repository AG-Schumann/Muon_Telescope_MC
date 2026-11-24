"""Four-fold events by the kind of primary particle that caused them.

Reads the output of soft_component_scan.sh (one file per configuration and
seed, e.g. cryall_vac_s1.root), adds the seeds of each configuration and
prints, for two thresholds, the four-fold rate and its split into

    muon      every panel above threshold from a muon primary alone
              (including its delta electrons: hits are assigned to the
              primary they descend from)
    non-muon  no deposit from any muon primary in any panel
    mixed     everything else (a muon plus shower particles, or a muon that
              fires only some panels)

Non-muon events are further split by the primary that deposited the most
energy (e, gamma, n, p, pi).

    python analysis/soft_component.py SoftComponent
    python analysis/soft_component.py SoftComponent --thr 0.5 0.13

Live time: CRY  -> RunInfo live_time_s (CRY timeSimulated);
           EcoMug -> N / (exact flux x plane area), as in overburden_analysis.
"""
import argparse
import glob
import os
import re
from collections import defaultdict

import numpy as np
import uproot

try:
    from overburden_analysis import ecomug_horizontal_flux, E_MPV
except ImportError:  # run from another directory
    import sys
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from overburden_analysis import ecomug_horizontal_flux, E_MPV

CLASSES = ["mu", "e", "gamma", "n", "p", "pi", "other"]
NC = len(CLASSES)


def pdg_class(pdg):
    a = np.abs(pdg)
    return np.select([a == 13, a == 11, a == 22, a == 2112, a == 2212, a == 211],
                     [0, 1, 2, 3, 4, 5], 6)


def live_time(info):
    gen = str(info["generator"][0])
    n = int(info["n_generated"][0])
    area = float(info["gen_area_m2"][0])
    if gen.lower() == "cry":
        return float(info["live_time_s"][0]), gen, n
    if gen.lower().startswith("ecomug"):
        if str(info["surface"][0]) != "sky":
            raise ValueError("EcoMug live time below assumes the horizontal sky plane")
        return n / (ecomug_horizontal_flux(0.1, 1000.) * area), gen, n
    raise ValueError(f"generator {gen}: no live time defined here")


def analyse_file(path, thresholds):
    """Returns live time, n_gen and, per threshold, counts per category."""
    with uproot.open(path) as f:
        info = f["RunInfo"].arrays(library="np")
        T, gen, n_gen = live_time(info)

        hits = [f[f"Panel{i}Hits"].arrays(["eventID", "edep", "primaryID"], library="np")
                for i in range(4)]
        # candidates: events with a deposit in all four panels
        cand = np.unique(hits[0]["eventID"])
        for h in hits[1:]:
            cand = np.intersect1d(cand, h["eventID"], assume_unique=False)
        nc_ev = cand.size
        if nc_ev == 0:
            return T, n_gen, {t: defaultdict(int) for t in thresholds}

        # primary pdg for (eventID, primaryID) of the candidate events
        keys, pdgs = [], []
        for chunk in f["Primaries"].iterate(["eventID", "primaryID", "pdg"],
                                            step_size="200 MB", library="np"):
            m = np.isin(chunk["eventID"], cand)
            keys.append(chunk["eventID"][m].astype(np.int64) * 100000 + chunk["primaryID"][m])
            pdgs.append(chunk["pdg"][m])
        keys = np.concatenate(keys); pdgs = np.concatenate(pdgs)
        order = np.argsort(keys); keys = keys[order]; pdgs = pdgs[order]

    # edep[event, panel, class]
    edep = np.zeros((nc_ev, 4, NC))
    for i, h in enumerate(hits):
        m = np.isin(h["eventID"], cand)
        ev = np.searchsorted(cand, h["eventID"][m])
        k = h["eventID"][m].astype(np.int64) * 100000 + h["primaryID"][m]
        j = np.clip(np.searchsorted(keys, k), 0, max(keys.size - 1, 0))
        found = keys.size > 0
        cls = np.where(found & (keys[j] == k), pdg_class(pdgs[j]), 6)
        np.add.at(edep, (ev, i, cls), h["edep"][m])

    tot = edep.sum(axis=2)                         # event x panel
    mu = edep[:, :, 0]
    out = {}
    for t in thresholds:
        thr = t * E_MPV
        sel = np.all(tot > thr, axis=1)
        muon = sel & np.all(mu > thr, axis=1)
        nonmu = sel & np.all(mu == 0, axis=1)
        mixed = sel & ~muon & ~nonmu
        c = defaultdict(int)
        c["4fold"] = int(sel.sum()); c["muon"] = int(muon.sum())
        c["mixed"] = int(mixed.sum()); c["non-muon"] = int(nonmu.sum())
        dom = np.argmax(edep[nonmu].sum(axis=1), axis=1)   # class with the largest deposit
        for d in dom:
            c["nm_" + CLASSES[d]] += 1
        out[t] = c
    return T, n_gen, out


def config_of(path):
    return re.sub(r"_s\d+\.root$", "", os.path.basename(path))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("outdir")
    ap.add_argument("--thr", nargs="+", type=float, default=[0.5, 0.13],
                    help="thresholds in units of E_MPV (0.13 ~ -30 mV, 0.5 = operating)")
    args = ap.parse_args()

    files = sorted(glob.glob(os.path.join(args.outdir, "*_s*.root")))
    configs = defaultdict(list)
    for p in files:
        configs[config_of(p)].append(p)

    results = {}
    for cfg, paths in configs.items():
        T = 0.; n = 0; cnt = {t: defaultdict(int) for t in args.thr}
        for p in paths:
            t_, n_, c_ = analyse_file(p, args.thr)
            T += t_; n += n_
            for t in args.thr:
                for k, v in c_[t].items():
                    cnt[t][k] += v
        results[cfg] = (T, n, len(paths), cnt)

    for t in args.thr:
        print(f"\n=== threshold {t:.2f} x E_MPV ===")
        print(f"{'configuration':<22}{'seeds':>5}{'T [s]':>9}{'N4':>8}{'R4 [Hz]':>16}"
              f"{'muon':>8}{'mixed':>8}{'non-mu':>8}   non-muon by primary")
        for cfg, (T, n, ns, cnt) in sorted(results.items()):
            c = cnt[t]; N = c["4fold"]
            if T <= 0:
                print(f"{cfg:<22}{ns:>5}   no live time"); continue
            R, dR = N / T, np.sqrt(N) / T
            fr = lambda k: (100. * c[k] / N) if N else 0.
            nm = ", ".join(f"{k[3:]} {c[k]}" for k in sorted(c) if k.startswith("nm_"))
            print(f"{cfg:<22}{ns:>5}{T:>9.0f}{N:>8d}{R:>9.4f} ± {dR:.4f}"
                  f"{fr('muon'):>7.1f}%{fr('mixed'):>7.1f}%{fr('non-muon'):>7.1f}%   {nm}")
    print("\nPercentages are of the four-fold events of that configuration.")


if __name__ == "__main__":
    main()