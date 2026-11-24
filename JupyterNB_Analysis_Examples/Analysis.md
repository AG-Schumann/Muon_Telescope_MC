# Analysis Commands (uproot)

```python
import uproot, numpy as np, matplotlib.pyplot as plt

run = uproot.open("MuonTelescope.root")
```

### Inspecting the file

```python
run.keys()                              # tree list
run["Panel0Hits"].keys()                # branch list
run["Panel0Hits"].num_entries           # entry count
run["Panel0Hits"].show()                # branch names + types (≈ Print())
```

### Loading a tree

```python
# Whole tree as a dict of numpy arrays
p0 = run["Panel0Hits"].arrays(library="np")

# Only the branches you need: much faster on large files
p0 = run["Panel0Hits"].arrays(["edep", "x", "y", "trackID"], library="np")

# Cut applied at read time, so only matching rows are decompressed
prim = run["Panel0Hits"].arrays(["edep", "x", "y"], cut="trackID == 1", library="np")

# As a pandas DataFrame
import pandas as pd
df = run["Panel0Hits"].arrays(library="pd")
```

The `cut` argument is the direct equivalent of ROOT's selection string, and
using it is preferable to filtering afterwards: uproot pushes the cut down to
the read step instead of loading every row into memory first.

### Energy deposition

```python
p0 = run["Panel0Hits"].arrays(["edep", "trackID"], library="np")

# All hits
plt.hist(p0["edep"], bins=100, histtype="step")

# Primary muon only: clean Landau
mask = p0["trackID"] == 1
plt.hist(p0["edep"][mask], bins=100, range=(0, 10), histtype="step")
plt.xlabel(r"$E_{dep}$ [MeV]"); plt.ylabel("Counts")
plt.title("Edep (MIP muon)")
```

### Spatial hit distribution

```python
p0 = run["Panel0Hits"].arrays(["x", "y", "z", "trackID"], cut="trackID == 1", library="np")

# XY distribution
plt.hist2d(p0["x"], p0["y"], bins=[56, 56],
           range=[[-140, 140], [-140, 140]], cmap="viridis")
plt.colorbar(label="Counts")
plt.xlabel("x [mm]"); plt.ylabel("y [mm]")

# Z distribution across the panel thickness
plt.hist(p0["z"], bins=50, histtype="step")
```

### Track multiplicity

```python
p0 = run["Panel0Hits"].arrays(["trackID", "eventID"], library="np")

n_primary   = np.sum(p0["trackID"] == 1)
n_secondary = np.sum(p0["trackID"] > 1)
n_total     = len(p0["trackID"])
n_events    = len(np.unique(p0["eventID"]))

print(f"Primary hits   (trackID=1): {n_primary}")
print(f"Secondary hits (trackID>1): {n_secondary}")
print(f"Total hits:                 {n_total}")
print(f"Events with hits:           {n_events}")
```

`np.unique` replaces the histogram-bin-counting workaround needed in ROOT.

### Particle identity (PDG)

```python
p0 = run["Panel0Hits"].arrays(["pdg", "trackID"], library="np")

# 13 (mu-), -13 (mu+), 11 (e-), -11 (e+), 22 (gamma)
values, counts = np.unique(p0["pdg"], return_counts=True)
for v, c in zip(values, counts):
    print(f"  PDG {v:>5}: {c}")
```

A table is usually more readable than a histogram here, since PDG codes are
scattered and non-contiguous.

### Primary muon spectrum (source validation)

```python
pri = run["Primaries"].arrays(library="np")

# Energy spectrum, log-log
plt.hist(pri["energy"], bins=np.logspace(1, 6, 100), histtype="step")
plt.xscale("log"); plt.yscale("log")
plt.xlabel("Kinetic energy [MeV]")

# Generation surface
plt.hist2d(pri["x0"], pri["y0"], bins=100)

# Direction: pz must be negative (downward going)
plt.hist(pri["pz"], bins=100, histtype="step")
```

Note that log-spaced bins have to be given explicitly; there is no equivalent
of just calling `gPad->SetLogx()` on a linearly binned histogram.

### Coincidence rates

```python
coin = run["Coincidence"].arrays(library="np")

for key in ["coin_4fold", "coin_top", "coin_bottom", "coin_outer"]:
    print(f"{key:12s}: {np.sum(coin[key])}")
```

### Run metadata

```python
info = run["RunInfo"].arrays(library="np")

n_gen = int(info["n_generated"][0])
area  = float(info["gen_area_m2"][0])
flux  = float(info["flux_hz_m2"][0])

n_det = int(np.sum(coin["coin_4fold"]))
rate  = flux * area * n_det / n_gen
print(f"R_4fold = {rate:.4f} Hz")
```

## For ROOT Users

### Open File
```bash
root -l MuonTelescope.root --web=off
```

### Inspect the Tree Structure
```cpp
// File contents
.ls
// Tree branches
Panel_0->Print()
Primaries->Print()
// Number of entries
Panel_0->GetEntries()
Primaries->GetEntries()
```

### Energy Deposition Analysis
```cpp
// Energy deposition of all hits
Panel_0->Draw("edep")
// Only primary muon (track=1): clean Landau distribution
Panel_0->Draw("edep", "trackID==1")
// Histogram with customized binning
TH1F* h_edep = new TH1F("h_edep", "Edep (MIP muon);E_{dep} [MeV];Counts", 100, 0, 10);
Panel_0->Project("h_edep", "edep", "trackID==1");
h_edep->Draw();
```

### Spatial Hit Distribution
```cpp
// XY hit distribution
Panel_0->Draw("y:x", "trackID==1", "colz")
// Z distribution (across the panel thickness)
Panel_0->Draw("z", "trackID==1")
// Fine-binned 2D histogram
TH2F* h_xy = new TH2F("h_xy", "Hit positions;x [mm];y [mm]", 56, -140, 140, 56, -140, 140);
Panel_0->Project("h_xy", "y:x", "trackID==1");
h_xy->Draw("colz");
```

### Track Multiplicity Analysis
```cpp
// TrackID distribution of hits
Panel_0->Draw("trackID")
// Primary vs. secondary counts
cout << "Primary hits  (trackID=1): " << Panel_0->GetEntries("trackID==1") << endl;
cout << "Secondary hits (trackID>1): " << Panel_0->GetEntries("trackID>1") << endl;
cout << "Total hits: "                  << Panel_0->GetEntries() << endl;
// Number of unique events (how many events have at least 1 hit)
TH1I* h_events = new TH1I("h_events", "Events with hits", 1000, 0, 1000);
Panel_0->Project("h_events", "eventID");
cout << "Events with hits: " << h_events->GetEntries() << endl;
// Alternatively, using unique bins:
cout << "Unique events: ";
int nUnique = 0;
for (int i = 1; i <= h_events->GetNbinsX(); ++i) {
    if (h_events->GetBinContent(i) > 0) nUnique++;
}
cout Draw("pdg")
// PDG distribution, primary tracks only
Panel_0->Draw("pdg", "trackID==1")
```
### Particle Identity (PDG Code)
```cpp
// PDG distribution: 13 (μ-), -13 (μ+), 11 (e-), -11 (e+), 22 (γ)
Panel_0->Draw("pdg")
// PDG distribution, primary tracks only
Panel_0->Draw("pdg", "trackID==1")
```

### Primary Muon Spectrum (Source Validation)
```cpp
// Generated muon energy spectrum (kinetic energy)
Primaries->Draw("energy")
// Log scale (better for visualizing the atmospheric muon spectrum)
Primaries->Draw("energy")
gPad->SetLogx()
gPad->SetLogy()
// Position distribution (on the sky surface)
Primaries->Draw("y0:x0", "", "colz")
// Direction (pz must be negative, downward direction)
Primaries->Draw("pz")
```
