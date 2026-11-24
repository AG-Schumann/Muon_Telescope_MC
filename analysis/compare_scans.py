#!/usr/bin/env python3
"""Table of eps_sc, eps_LG and c for several scans (wrapping, PMMA grade).

Runs light_collection.py on every scan directory that has no summary.csv yet
and prints a Markdown table from the summaries.

Usage:
    python3 analysis/compare_scans.py build/Optical_Scan_*
"""

import os
import subprocess
import sys

import pandas as pd

HERE = os.path.dirname(os.path.abspath(__file__))
COLUMNS = ["eps_sc", "eps_LG", "c", "npe_muon_median", "npe_guide_median",
           "pulse_height_ratio", "amplitude_ratio", "frac_guide_above_0.11", "frac_guide_above_0.39"]


def main():
    rows = []
    for scan_dir in sys.argv[1:]:
        summary = os.path.join(scan_dir, "analysis", "summary.csv")
        if not os.path.exists(summary):
            subprocess.run([sys.executable, os.path.join(HERE, "light_collection.py"), scan_dir],
                           check=True, stdout=subprocess.DEVNULL)
        s = pd.read_csv(summary, header=None, index_col=0).iloc[:, 0]
        name = os.path.basename(os.path.normpath(scan_dir)).replace("Optical_Scan_", "")
        wrapping, _, grade = name.partition("_")
        rows.append(dict(wrapping=wrapping, pmma=grade, **{c: float(s[c]) for c in COLUMNS}))
    df = pd.DataFrame(rows)
    cols = list(df.columns)
    print("| " + " | ".join(cols) + " |")
    print("|" + "|".join(["---"] * len(cols)) + "|")
    for _, r in df.iterrows():
        print("| " + " | ".join(f"{v:.3f}" if isinstance(v, float) else str(v) for v in r) + " |")


if __name__ == "__main__":
    main()
