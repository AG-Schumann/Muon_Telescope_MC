# Muon Telescope MC

Geant4 application to simulate the geometric acceptance of a four-panel
plastic scintillator muon telescope, using EcoMug as the atmospheric muon
generator.

## Compilation

The project targets Geant4 11.3.2. The official installation instructions are
available on the [Geant4 download page](https://geant4.web.cern.ch/download/11.3.2.html).
Alternatively, Geant4 can be installed through
[conda-forge](https://anaconda.org/conda-forge/geant4) on any operating system.

ROOT is required as well, since the output is written through
`G4AnalysisManager` in ROOT ntuple format.

Once the installation has been verified, run:

```bash
git clone https://github.com/AG-Schumann/Muon_Telescope_MC
cd Muon_Telescope_MC
mkdir build && cd build   # by convention, Geant4 projects are built out-of-source
cmake ..
make -j                   # "-j" enables parallel compilation on all available cores
```

The executable is then built as `build/Muon_Simulation`.

### Optional: CRY air-shower generator

CRY (LLNL, v1.7) generates complete cosmic-ray showers rather than single
muons, so it can be used to study the contribution of the electromagnetic
and hadronic components. It is optional: without it the project builds and
runs exactly as before.

CRY is distributed from the
[LLNL simulation page](https://nuclear.llnl.gov/simulation/). Build it with
its own Makefile:

```bash
cd ~/software && tar xzf cry_v1.7.tar.gz && cd cry_v1.7
make                  # produces lib/libCRY.a
cd test && make       # prints "Files testMain.out and testMain.ref are identical"
export CRYHOME=~/software/cry_v1.7
```

Then configure this project with CRY enabled:

```bash
cmake -DWITH_CRY=ON -DCRY_DIR=$CRYHOME ..
make -j
```

`CRY_DIR` must point at the directory containing `src/` and `lib/libCRY.a`;
if the `CRYHOME` environment variable is set, `-DCRY_DIR` can be omitted. The
data tables are located through the `CRY_DATA_DIR` compile definition, so no
environment variable is needed at run time.

## Execution

Run the executable with:

```bash
<path/to/Muon_Telescope_MC>/build/Muon_Simulation [OPTIONS]
```

| Option | Description |
| --- | --- |
| `-m <macro>` | Macro file to execute. If omitted, the interactive visualization mode is started. |
| `-g <gen>` | Muon generator to use (default: `ecomug`). See the table below. |
| `-s <file>` | Muon source file. Required by the file-based generators, ignored by `ecomug`. |
| `-o <name>` | Basename of the output ROOT file (default: `MuonTelescope`). |
| `-r <seed>` | Random seed. If omitted, a seed is derived from the system clock and printed at startup. |
| `-i` | Keep an interactive session open after the macro has finished. |
| `-p` | Optical photon transport: see [Optical light collection](#optical-light-collection). |
| `-h` | Print the usage message and exit. |

### Generators (`-g`)

| Value | Source class | Description |
| --- | --- | --- |
| `ecomug` | `EcoMugDirectSource` | Muons are sampled by EcoMug at runtime, event by event. Default. |
| `cry` | `CRYSource` | Full air showers sampled by CRY at runtime. Requires `-DWITH_CRY=ON`. |
| `ecomug-file` | `EcoMugFileSource` | Muons are read from a pre-generated EcoMug file. Requires `-s`. |
| `mute+` | `MuteFileSource(+1)` | mu+ events from a MUTE-based file. Requires `-s`. |
| `mute-` | `MuteFileSource(-1)` | mu- events from a MUTE-based file. Requires `-s`. |
| `isotropic` | `IsotropicSource` | Cosine-law source for the geometric acceptance. |
| `beam` | `BeamSource` | Pencil beam with a fixed particle, momentum, start point and direction (`/muonSource/beam/`), for position scans. |

Since the MUTE files are produced separately for each charge, the charge is
taken from the `-g` flag rather than from the file. A mu+ run and a mu- run are
therefore two separate executions, and the two output files are combined at the
analysis stage, weighted by the muon charge ratio.

Unlike the other generators, CRY returns several particles per event: one CRY
shower is mapped onto one Geant4 event with one primary vertex per particle.
The particle types included in the shower are selected through the macro
commands below.
Examples:

```bash
# EcoMug, generated at runtime
./Muon_Simulation -m macros/run.mac -o results

# MUTE mu+ file, with a fixed seed for reproducibility
./Muon_Simulation -m macros/run.mac -o results_muplus -g mute+ -s data/mute_plus.dat -r 12345

# Pre-generated EcoMug file
./Muon_Simulation -m macros/run.mac -o results -g ecomug-file -s data/muons.dat

# Interactive / VRML visualization mode
./Muon_Simulation
```

### Reproducibility

Every run prints the random seed it was started with:
```bash
Random seed: 1757318400 (reproduce this run with -r 1757318400)
```

Passing that value back through `-r` repeats the run identically. Without `-r`,
each run uses a different seed, so several runs can be combined to increase the
statistics.
### Vendored EcoMug
`include/EcoMug.h` is EcoMug 3.0, unmodified. Versions up to 2.1 seeded the
charge generator from `std::random_device` independently of `SetSeed()`, which
made the charge sequence irreproducible; this was reported upstream and fixed
in 3.0, so the local patch is no longer needed.

### Macro files

The `macros/` directory holds the run macros: EcoMug, CRY and isotropic
sources, telescope tilt, concrete roof and lead shielding, the optical beam,
the scan templates and the visualisation macros.

Geometry can be modified at runtime through the `DetectorMessenger` commands:
```bash
/detector/zenith <angle> deg # telescope zenith angle
/detector/azimuth <angle> deg # telescope azimuth angle
/detector/rebuild # rebuild the geometry with the new values
```


`/detector/rebuild` must be issued before `/vis/drawVolume`, otherwise the
visualization will show the previous geometry.

### Muon source

Two generation modes are available:

* **`EcoMugDirectSource`** (default): muons are sampled by EcoMug at runtime,
  event by event.
* **`EcoMugFileSource`** (`-s`): muons are read from a pre-generated ASCII file.
  When using this mode, make sure the generation surface (flat sky vs.
  hemisphere, size, height) of the file is known, since the rate normalization
  depends on it.



### Output

Results are written to `<name>.root` in the current working directory, where
`<name>` is set by `-o`. If `-o` is not given, the file is named
`MuonTelescope.root`.

## Output format

The ROOT file contains the ntuples written by `G4AnalysisManager`
(`RunAction.cc`):

| ID | Ntuple | Content |
| --- | --- | --- |
| 0-3 | `Panel0Hits` ... `Panel3Hits` | One row per Geant4 **step** with an energy deposit in the corresponding panel (in geometry mode 2 also in its light guide). |
| 4 | `Coincidence` | One row per event, coincidence flags. |
| 5 | `Primaries` | One row per generated primary particle. |
| 6 | `RunInfo` | One row per run, generator and normalisation. |
| 7, 8 | `OpticalSummary`, `PhotonHits` | Optical mode only (`-p`), see [Optical light collection](#optical-light-collection). |

### Panel ntuples

| Column | Unit | Description |
| --- | --- | --- |
| `eventID` | - | Geant4 event identifier |
| `trackID`, `parentID` | - | Track and parent track identifiers; primaries have `parentID == 0` |
| `pdg` | - | PDG code of the particle |
| `edep` | MeV | Energy deposited in the step |
| `x`, `y`, `z` | mm | Pre-step position |
| `time` | ns | Pre-step global time |
| `kineticEnergy` | MeV | Pre-step kinetic energy |
| `primaryID` | - | trackID of the primary this track descends from |

Because each row corresponds to a step and not to a track or an event,
secondary particles (delta electrons, bremsstrahlung photons) contribute many
rows but only a small part of the deposited energy.

### Coincidence ntuple

| Column | Description |
| --- | --- |
| `eventID` | Geant4 event identifier |
| `coin_4fold` | All four panels hit |
| `coin_top` | Panel 0 AND Panel 1 |
| `coin_bottom` | Panel 2 AND Panel 3 |
| `coin_outer` | Panel 0 AND Panel 3 |
| `coincidence` | Not filled (always 0) |

"Hit" means any energy deposit in the panel volume, without threshold.

### Primaries ntuple

| Column | Unit | Description |
| --- | --- | --- |
| `eventID` | - | Geant4 event identifier |
| `primaryID` | - | Primary track number, 1..N in vertex order |
| `pdg` | - | PDG code |
| `energy` | MeV | Kinetic energy |
| `px`, `py`, `pz` | GeV/c | Momentum components |
| `x0`, `y0`, `z0` | mm | Generation position |
| `theta_deg`, `phi_deg` | deg | Zenith angle (from the downward vertical) and azimuth |
| `t_ns` | ns | Arrival time relative to the earliest particle of the same shower |

With a shower generator, `Primaries` holds several rows per event, one per
generated particle; `t_ns` is zero for the single-particle sources.

### RunInfo ntuple

| Column | Unit | Description |
| --- | --- | --- |
| `n_generated` | - | Number of events of the run |
| `generator`, `surface`, `source_file` | - | Generator label, generation surface, input file |
| `gen_area_m2` | m^2 | Generation area |
| `flux_hz_m2`, `flux_err` | Hz/m^2 | Normalised flux from the generator and its uncertainty |
| `seed` | - | Random seed |
| `live_time_s` | s | Live time from the generator (EcoMug estimate, CRY clock); 0 for the other sources |
| `overburden_gcm2` | g/cm^2 | Vertical areal density of the concrete slabs, 0 without roof |

## Rate normalization

EcoMug returns an average generation rate in Hz/m², not a total rate. The
detector rate is obtained as 
$$R_{\text{det}} = \Phi_{\text{norm}} \cdot A_{\text{gen}} \cdot \left(\frac{N_{\text{det}}}{N_{\text{gen}}}\right)$$

where `Phi_norm` is the value returned by `GetAverageGenRateAndError()`,
`A_gen` is the generation surface area (which must be supplied externally),
`N_gen` is the number of generated muons and `N_det` the number of events
satisfying the requested coincidence condition.
### CRY

CRY keeps its own clock: every primary it samples advances `timeSimulated()`
by an exponentially distributed waiting time, including the primaries that
leave no particle inside the box. That value is the live time of the run and
is written to `RunInfo.live_time_s`, so the detector rate is simply

$$R_{\text{det}} = \frac{N_{\text{det}}}{t_{\text{live}}}$$

Note that `n_generated` counts showers, not particles: in muon-only mode only
about 35 % of the sampled primaries produce a muon inside the box, and a
shower may contain more than one muon. Particle-level quantities such as the
muon flux should therefore be computed from the `Primaries` ntuple rather
than from `n_generated`.
## Analysis

The scripts in `analysis/` read the ROOT ntuples with `uproot` (plus `numpy`,
`pandas`, `scipy`, `matplotlib`, `uncertainties`):

| Script | Content |
| --- | --- |
| `light_collection.py`, `compare_scans.py` | Light-collection efficiencies of the optical scans |
| `photon_map.py` | Map of the detected light from the optical scans |
| `overburden_analysis.py` | Vertical rate with and without the concrete floors |
| `soft_component.py` | Four-fold events by the kind of primary particle |
| `sim_tables.py` | Per-panel numbers and LaTeX tables for the thesis |

## Physics
The physics list is the reference list FTFP_BERT with the electromagnetic
part replaced by `G4EmStandardPhysics_option4` (`PhysicsList.cc`), with the
default production cut of 0.7 mm. It therefore contains muon decay, muon
nuclear interactions and the capture of stopped mu-, which matter for muons
stopping in the concrete floors (see below).

## Concrete floors above the telescope (high-rise building)

The roof commands build a stack of horizontal `G4_CONCRETE` slabs
(2.3 g/cm3) above the telescope:

```bash
/detector/roof          true
/detector/roofSlabs     5        # floors above the room (default 1)
/detector/roofThickness 25 cm    # one slab
/detector/roofSlabGap   0 m      # clear gap between slabs (default 0)
/detector/roofHeight    1.30 m   # lower surface of the lowest slab
/detector/roofHalfSize  2.0 m    # half side of the square slabs
/detector/rebuild
```

The generation surface has to lie above the highest slab. Every primary
vertex is checked; a vertex below the upper surface of the slabs stops the run
(`VertexBelowRoof`), since such a muon would bypass the concrete. In
particular the 1.4 m hemisphere cannot be used with slabs; use a horizontal
plane (`sky`) above them. The total areal density is written to
`RunInfo.overburden_gcm2`.

For slabs that are wide compared with the acceptance and a flux that is
uniform over the plane, neither the height of the slabs nor the gaps between
them change the rate: a muon crosses t/cos(theta) of concrete wherever the
slab is, and tracks shifted sideways by multiple scattering are replaced by
tracks shifted in from the neighbourhood. The slabs are therefore stacked
without gaps just above the frame (lower surface 1.30 m; the telescope world
ends at 1.27 m), which keeps the plane small.

`overburden_scan.sh` runs the vacuum reference and one run per slab
thickness from `macros/overburden_vertical_template.mac` (vertical telescope,
mode 0, EcoMug 0.1-1000 GeV/c, plane 2.4 m x 2.4 m at z = 2.81 m, above
5 x 30 cm). All runs use the same plane and seed, so they contain the same
muons:

```bash
cd build
NEVENTS=1000000 THICKNESSES="15 20 25 30" JOBS=2 ./overburden_scan.sh
```

```python
import overburden_analysis as ob          # analysis/overburden_analysis.py
vac = ob.load_run("Overburden/vacuum.root")
con = ob.load_run("Overburden/slabs5_t25.root")
ob.summary(vac, con)                      # rates at 0.5 E_MPV and the paired ratio
```

The live time follows from the exact EcoMug flux through a horizontal
plane, 126.7 m^-2 s^-1 for 0.1-1000 GeV/c, i.e. T = 1369.7 s per 10^6 muons
on the 5.76 m^2 plane. CPU time: about 2.2 ms per muon with 5 x 25 cm of
concrete, 0.015 ms in vacuum.

## Zenith angle scans

The geometric acceptance of the telescope falls off quickly with zenith angle,
so characterising it means running the same simulation at several
orientations. `zenith_scan.sh` automates this for hemisphere generation. It is
copied into the build directory by CMake, so it can be called directly from
there:

```bash
cd build
./zenith_scan.sh # make sure that it's executable, if not run "chmod +x zenith_scan.sh"
```
with the log: 

```bash
./zenith_scan.sh 2>&1 | tee Lab_Hemisphere_Scanning/scan.log
```

For every angle in the scan the script generates a macro from
`macros/zenith_scan_template.mac`, picks a reproducible seed, and writes one
ROOT file per angle:

```
Hemisphere_Scanning/
  theta_0.root   theta_0.wrl
  theta_5.root   theta_5.wrl
  theta_10.root  theta_10.wrl
  ...
```

Each `.wrl` file contains the geometry only, with no trajectories, so it stays
small and can be used to verify visually that the telescope is tilted as
intended at that angle.

### Configuring a scan

The angles and the number of events per angle are set at the top of the script:

```bash
declare -A NEVENTS=(
  [0]=2000000   [5]=2000000   [10]=3000000  [15]=5000000
  [20]=10000000 [25]=10000000 [30]=10000000 [40]=20000000
)
```

More statistics are needed at large angles because the four-fold acceptance
drops sharply: the outer panel pair (P0 and P3) is separated by 964 mm and each
panel is 280 mm wide, so a straight track can only cross both up to about
16 degrees. Beyond roughly 20 degrees the four-fold rate is consistent with
zero and a scan point there mainly serves to demonstrate the cut-off rather
than to measure it.

Seeds are derived from the angle rather than from the clock:

```bash
SEED=$((1000003 + THETA * 7919))
```

This makes the whole scan reproducible, and the prime multiplier keeps
consecutive seeds far apart. That matters because EcoMug's `SetSeed()` writes
the same value into both words of the xoroshiro128+ state, so nearby seeds
start from a nearly degenerate point and their first outputs can be
correlated.

### Other configurations

`zenith_scan.sh` covers the hemisphere case only. For anything else - flat sky
generation, file-based sources, isotropic tests, alternative visualisation
setups - see the macros in `macros/`. The script is a convenience wrapper, not the only way to
run the simulation; a single run is always just

```bash

./Muon_Simulation -m macros/<some>.mac -o <output> -r <seed>
```
### Analysis

Each output file carries its own normalisation metadata in the `RunInfo`
ntuple, so the analysis does not need to know how the scan was configured:

```python
info  = run["RunInfo"].arrays(library="np")
n_gen = int(info["n_generated"][0])
area  = float(info["gen_area_m2"][0])
flux  = float(info["flux_hz_m2"][0])

coin  = run["Coincidence"].arrays(library="np")
n_det = int(coin["coin_4fold"].sum())

rate = flux * area * n_det / n_gen
```

Note that `flux` is recomputed by a Monte Carlo integration in every run and
therefore varies by about 1 % between angles. Since the generation surface does
not change during a scan, this variation is numerical rather than physical.
Using a single reference value across all angles keeps that uncertainty fully
correlated, so it affects the overall scale but not the shape of the acceptance
curve.

## Optical light collection

Optical photon transport in geometry mode 2 (scintillator panels, light
guides and PMT windows) is used to compute
the light-collection efficiencies of the modules as a function of the
crossing position:

* `eps_sc`: fraction of the scintillation photons produced in the panel that
  reach the photocathode,
* `eps_LG`: the same for the Cherenkov photons produced in the light guide,
* `c = eps_LG / eps_sc`, and from these the pulse height of a muon that
  crosses only the light guide, in units of the muon signal in the panel.

The question behind it is a population of small pulses (below about 0.2-0.3
of the most probable muon signal) that disappears when the discriminator
threshold is raised from -30 mV to -110 mV; the hypothesis is Cherenkov light
of muons that cross only the light guide.

### The `-p` flag

Optical transport is switched on with `-p`. Without it the program is
unchanged: for the same seed the `PanelXHits`, `Coincidence` and `Primaries`
ntuples are identical to those of the version without the optical code
(checked with 10^4 EcoMug events in geometry modes 0 and 2).

With `-p`:

* `PhysicsList` registers `G4OpticalPhysics` (Cherenkov, scintillation, bulk
  absorption, boundary process) and sets `G4OpticalParameters`: at most 100
  Cherenkov photons per step, maximum beta change 10 %, secondaries tracked
  first, finite scintillation rise time. `G4OpticalPhysics` attaches
  `G4EmSaturation` to `G4Scintillation`, so the Birks constant of the
  scintillator is applied (checked in the 11.3.2 source,
  `G4OpticalPhysics::ConstructProcess()`).
* `DetectorConstruction` gives the scintillator, the PMMA and the PMT window
  glass their optical properties in the **first** `Construct()` call, whatever
  the geometry mode: the Cherenkov and scintillation tables are built at
  initialisation from the materials that carry a property table at that
  moment, and mode 2 is only reached afterwards with `/detector/rebuild`.
  The PMT windows and the optical surfaces are built in mode 2 only; mode 1
  with `-p` issues a warning. On every `Construct()` the logical surface
  tables are cleared (`G4LogicalSkinSurface::CleanSurfaceTable()`,
  `G4LogicalBorderSurface::CleanSurfaceTable()`), so a rebuild does not keep
  surfaces of old volumes.
* `SensitiveDetector` ignores optical photons explicitly.
* The world and the pair boxes (vacuum) have no `RINDEX`: photons never
  travel in the vacuum, they are reflected or absorbed by the wrapping
  surfaces.

Example, one vertical 2 GeV/c mu- through the panel centre:

```bash
./Muon_Simulation -m macros/optical_beam.mac -o beam -g beam -p -r 1
```

### Commands

| Command | Values | Description |
| --- | --- | --- |
| `/detector/optics/wrapping` | `alfoil` (default), `tyvek`, `black` | Reflector under the black tape of the panels and light guides. Applied at the next `/detector/rebuild`. |
| `/detector/optics/pmmaGrade` | `standard` (default), `uvt` | Bulk absorption of the light-guide PMMA. Applied at the next `/detector/rebuild`. |
| `/detector/optics/storePhotons` | `true`, `false` (default) | Fill the `PhotonHits` ntuple, one row per detected photon (about 3000 rows per module and muon). |
| `/muonSource/beam/particle` | e.g. `mu-` | Beam particle (`-g beam`). |
| `/muonSource/beam/momentum` | e.g. `2 GeV` | Beam momentum. |
| `/muonSource/beam/position` | `x y z unit` | Start point. The panels sit at z = 50-108 cm, the inner top bar of the frame at 118-122 cm, so a vertical beam is started at z = 115 cm. |
| `/muonSource/beam/direction` | `dx dy dz` | Direction, normalised internally. |

Above 420 nm all three transmission curves sit on the 91 % plateau, which is
the Fresnel limit (1-R)^2 = 92 % within the reading error, so the absorption
length there is only known to be longer than about 1 m; it is capped at
200 cm. For a 7 cm guide with 10-20 cm of photon path this cap is a 5-10 %
effect on `eps_LG`.

The PMMA grade only replaces the `ABSLENGTH` entry of the light-guide
material table, which `G4OpAbsorption` reads at every step; the `RINDEX`
tables, from which the Cherenkov and scintillation integrals are built once
at initialisation, are never changed. This is why the grade can be an
ordinary macro command although the material table is created once, before
the macro runs.

### Geometry additions

The only new volumes are the four PMT windows `PMTWindow0..3`: glass
cylinders of radius 23 mm (useful photocathode diameter 46 mm of the
ET 9214B, to be checked against the Scionix test sheet) and length 10 mm,
axis along x, flush against the 60 mm x 50 mm end face of the light guide,
at `x = kLGSide * (panelHX + 2*kLGHalfLength + 5 mm) = -215 mm`. The part of
the end face outside the 46 mm circle stays under the wrapping. The overlap
check passes with the windows at zenith 0 and 90 deg; the pair boxes did not
need to be enlarged.

### Assumptions

| Item | Value | Source / status |
| --- | --- | --- |
| Photon energy range | 300-650 nm (1.91-4.13 eV) for all `RINDEX` tables | Cherenkov photons are generated only in this range, so it is part of the result (about 450 per cm in PMMA for a 2 GeV/c muon). |
| EJ-200 | n = 1.58, bulk attenuation length 380 cm, 10000 photons/MeV, rise 0.9 ns, decay 2.1 ns, resolution scale 1 | Eljen data sheet. |
| EJ-200 emission spectrum | peak 425 nm, table in `DetectorConstruction.cc` | Approximate, digitised by eye; to be replaced by a proper digitisation of the data sheet. |
| Birks constant | 0.126 mm/MeV | Typical value for polyvinyltoluene. |
| PMMA | n = 1.502 (436 nm), 1.492 (589 nm); Cauchy end points 1.527 (300 nm), 1.490 (650 nm) | Eljen / Scionix "Light guides and acrylic plastic" data sheet. |
| PMMA absorption, `standard` | 380 nm: 1 mm, 385: 6 mm, 390: 1.6 cm, 395: 4 cm, 400: 10 cm, 405: 22 cm, 410: 46 cm, 420: 66 cm, 440: 150 cm, >= 460 nm: 200 cm; 0.5 mm below 380 nm | From the 19 mm commercial-sheet transmission curve of the same data sheet, L = -d / ln(T / 0.923), digitised (about 20 %). Scionix lists this grade as "PMMA, clear acrylic, light guides" (BC-802). |
| PMMA absorption, `uvt` | 300 nm: 2 cm, 310: 6 cm, 320: 10 cm, 330: 15 cm, 340: 22 cm, 350: 35 cm, 360: 58 cm, 370: 82 cm, 380: 120 cm, 390: 170 cm, >= 400 nm: 200 cm | From the 105 mm UVT curve above 330 nm and the 18 mm UVT curve below; the two samples differ by a factor 1.5 where they overlap. Scionix: "UVT, UV transmitting acrylic, light guides" (BC-800). Which grade the guides are made of is not known. |
| PMT window | `G4_Pyrex_Glass`, n = 1.49, absorption length 100 cm | Borosilicate. |
| Wrapping `alfoil` | unified, `dielectric_dielectric`, `polishedbackpainted`, air gap (surface `RINDEX` 1.0), reflectivity 0.90, `SPECULARSPIKECONSTANT` 1 | Default: specular reflector (aluminium foil or aluminised Mylar) with an air gap. |
| Wrapping `tyvek` | `groundbackpainted`, reflectivity 0.95, sigma_alpha 0.1 rad, `SPECULARLOBECONSTANT` 1 | Diffuse reflector. |
| Wrapping `black` | `polishedbackpainted`, reflectivity 0.05 | Black tape over an air gap; only total internal reflection survives. |
| Panel-guide, guide-window couplings | `G4LogicalBorderSurface`, both directions, `dielectric_dielectric`, `polished` | Optical cement and grease; Fresnel from the bulk indices only. Border surfaces take precedence over the skin surfaces, so the wrapping does not act on the glued faces. |
| Photocathode | ideal, radius 23 mm; every photon entering the window is counted and killed | The quantum efficiency is applied in the analysis. |
| Quantum efficiency | 300 nm 0.15, 320 0.24, 350 0.28, 380 0.28, 400 0.27, 420 0.24, 450 0.20, 480 0.15, 500 0.12, 550 0.06, 600 0.02, 650 0.005 | Approximate bialkali curve for the ET 9214B; to be replaced by the data-sheet curve. |
| Single-photoelectron pulse | Gaussian, FWHM 4.5 ns | For the pulse shapes only. |

Two details of `G4OpBoundaryProcess` matter for the wrapping. With the
back-painted finishes the photon is refracted into the air gap of the surface
table and reflected (probability `REFLECTIVITY`) or absorbed there; it never
enters the vacuum volume, so the vacuum needs no `RINDEX`. And in the unified
model every reflection at a back-painted surface, total internal reflection
included, goes through `ChooseReflection()`: without the unified constants
it would be Lambertian, which turns a polished panel into a diffuser. The
constants are therefore set explicitly (spike for the polished finishes, lobe
for the ground one).

### Output

With `-p` two ntuples are appended; the existing ones keep their IDs.

`OpticalSummary` (ntuple 7), one row per event: `eventID` and, for each
module `i`, the produced and detected photon numbers by origin and process,
the arrival time of the first photon and the losses:

| Column | Content |
| --- | --- |
| `prodScint_sc<i>`, `prodCher_sc<i>` | scintillation / Cherenkov photons created in the scintillator |
| `prodCher_lg<i>`, `prodCher_win<i>` | Cherenkov photons created in the light guide / PMT window |
| `detScint_sc<i>`, `detCher_sc<i>`, `detCher_lg<i>`, `detCher_win<i>` | the same, detected at the window |
| `tFirst<i>` | arrival time of the first detected photon (ns, -1 if none) |
| `absBulk<i>` | photons lost by bulk absorption (`OpAbsorption`) |
| `absSurf<i>` | photons absorbed by the wrapping |
| `noRindex<i>` | photons killed because the next material has no `RINDEX` (must be 0) |

For every event and module `produced = detected + absBulk + absSurf +
noRindex` holds exactly; `noRindex` is zero.

`PhotonHits` (ntuple 8), one row per detected photon, only with
`/detector/optics/storePhotons true`: `eventID`, `module`, `origin`
(0 scintillator, 1 light guide, 2 PMT window), `process` (0 scintillation,
1 Cherenkov), `energy_eV`, `wavelength_nm`, `time_ns` (arrival at the window),
`x0_mm`, `y0_mm`, `z0_mm` (creation point).

### Position scans

`optical_scan.sh` writes and runs the scan macros from
`macros/optical_scan_template.mac`, one process and one output file per beam
position (the beam position is also in `Primaries`, `x0`, `y0`):

```bash
cd build
JOBS=4 ./optical_scan.sh alfoil standard 100     # wrapping, PMMA grade, events per point
```

* scintillator grid: x, y in {-120, -80, -40, 0, 40, 80, 120} mm,
  `panel_x<X>_y<Y>.root`;
* light-guide grid: x in {-145, ..., -205} mm (10 mm steps), y in
  {-20, 0, 20} mm, `lg_x<X>_y<Y>.root`;
* two reference runs with `PhotonHits` stored, `ref_panel.root` (panel
  centre) and `ref_lg.root` (x = -175 mm), used for the wavelengths and the
  arrival times.

A vertical beam crosses all four modules at the same (x, y), so one event
gives four measurements. A panel event costs about 18 s of CPU (four modules,
about 75000 photons), a guide crossing well below 1 s and an EcoMug event
0.2 s on average (5 s when a panel is hit). With 100 events per point the
panel grid takes about a CPU day; since the per-event efficiency fluctuates
by only 2 % (photon statistics), 20 events per point are enough for the
maps and 10 for the systematics, which is what was run here.

### Analysis

```bash
python3 analysis/light_collection.py build/Optical_Scan_alfoil_standard [--usetex]
```

writes to `<scan_dir>/analysis/` the efficiency maps and profiles, the
photoelectron distributions, the detected spectra with the QE curve, the
average pulses, `per_module.csv` and `summary.csv`. The quantum efficiency is
sampled per photon (binomial) where the wavelengths are stored; for the scan
points the spectrum-averaged QE of the reference runs is used with the same
binomial sampling, so the photoelectron statistics are included. Panel
Cherenkov photons are added to the muon signal.

### Validation

Checks made when the optical mode was added (Geant4 11.3.2 from conda-forge,
serial run manager):

| Check | Result |
| --- | --- |
| Default unchanged | 10^4 EcoMug events (hemisphere source, `-r 12345`), modes 0 and 2, without `-p`: `Panel0..3Hits`, `Coincidence` and `Primaries` with and without the optical code are identical (compared with uproot, column by column). |
| Overlaps | Overlap check passes in mode 2 with `-p`, `PMTWindow0..3` included, at zenith 0 and 90 deg. |
| Scintillation yield | Vertical 2 GeV/c mu- through the panel centre: produced scintillation photons / (10^4 x deposited MeV) = 0.975-0.978 per module (Birks). |
| Cherenkov yield | Beam through the guide at x = -175 mm (30 mm of PMMA): 1380-1410 Cherenkov photons per crossing (median), 468-490 per cm against the expected 450 per cm for 300-650 nm; the excess comes from delta electrons. |
| Photon bookkeeping | For every event and module `produced = detected + absBulk + absSurf + noRindex` with `noRindex = 0`, checked on all scan files. |
| CPU time | Initialisation 4 s. Panel-centre muon 18 s per event (four modules, about 75000 photons, 1 process on 1 core; 45 s per event with four processes on this 4-core machine). EcoMug hemisphere event 0.2 s on average (11 of 300 events reach a panel, about 5 s each). Guide crossing well below 1 s. |

Proposed event numbers: the per-event efficiency fluctuates by about 2 %
(photon statistics), so 20 events per panel point (49 points, about 3 h with
4 processes) are enough for the maps; the guide grid is cheap and can take
100 events per point; the systematics were run on a 4 x 4 panel grid with
6 events per point. An EcoMug run with `-p` costs about 5 s per panel hit.

### Results, default configuration (alfoil, standard PMMA)

Panel grid 49 points x 10 events, guide grid 21 points x 10 events, PMMA
absorption from the data-sheet curves:

| Quantity | Value |
| --- | --- |
| `eps_sc`, area average | 0.136 (0.13 at the far corners, 0.15 next to the guide; the four modules agree to 0.001) |
| `eps_LG`, guide grid average | 0.207 (0.17 at x = -145 mm, 0.26 at x = -205 mm) |
| `c = eps_LG / eps_sc` | 1.53 |
| `<QE>` scintillation / guide Cherenkov | 0.212 / 0.125 (the commercial PMMA cuts the Cherenkov light below 390 nm) |
| Photoelectrons, panel muon (median) | 508 |
| Photoelectrons, guide crossing (median) | 36 |
| Guide crossing in units of the muon signal | 0.071 (charge), 0.130 (amplitude: the Cherenkov pulse is 1.8 times narrower than the scintillation pulse for the same charge) |
| Guide crossings above 0.11 / 0.39 of the muon signal | 0.26 / 0.00 (charge) |

With the default optical assumptions the Cherenkov pulses of guide
crossings sit at 7 % (charge) to 13 % (amplitude) of the muon signal, with
`c` about 1.5 rather than 3. The systematics below show how far the
wrapping and the PMMA grade move this.

### Systematics

All six combinations of wrapping and PMMA grade, same analysis. The
systematics were run on a 4 x 4 panel grid (x, y in {-120, -40, 40, 120} mm)
with 6 events per point and the full guide grid with 6 events per point;
the first row is the default scan above. `ratio` is the median guide
crossing in units of the median panel muon, in charge and in pulse
amplitude; `> 0.11` and `> 0.39` are the fractions of guide crossings
above the two thresholds (charge).

| wrapping | PMMA | eps_sc | eps_LG | c | p.e. muon | p.e. guide | ratio (charge) | ratio (amplitude) | > 0.11 | > 0.39 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| alfoil | standard | 0.136 | 0.207 | 1.53 | 508 | 36 | 0.071 | 0.130 | 0.26 | 0.00 |
| tyvek | standard | 0.156 | 0.231 | 1.49 | 589 | 40 | 0.068 | 0.125 | 0.25 | 0.00 |
| black | standard | 0.052 | 0.087 | 1.66 | 200 | 14 | 0.070 | 0.112 | 0.29 | 0.01 |
| alfoil | uvt | 0.164 | 0.327 | 1.99 | 642 | 82 | 0.128 | 0.245 | 0.57 | 0.01 |
| tyvek | uvt | 0.187 | 0.364 | 1.95 | 725 | 92 | 0.127 | 0.250 | 0.57 | 0.00 |
| black | uvt | 0.058 | 0.146 | 2.53 | 229 | 32 | 0.140 | 0.228 | 0.61 | 0.11 |

The wrapping scales `eps_sc` and `eps_LG` together, so `c` depends on it
only weakly (1.5 to 1.7 with the commercial PMMA); it mostly sets the
absolute photoelectron numbers. The PMMA grade is the decisive assumption:
the Cherenkov spectrum is weighted towards the UV where the QE is highest,
and UV-transmitting acrylic lets that light reach the cathode, which raises
`c` to 2.0-2.5 and the guide crossing to 0.13-0.14 of the muon signal in
charge and 0.23-0.25 in amplitude. With `uvt` 57-61 % of the guide
crossings lie above 0.11 and about 1 % above 0.39 of the muon signal, i.e.
a population present at the -30 mV threshold and gone at -110 mV, as
measured; `c` comes within 20-30 % of the value of about 3 inferred from
the measurements. With the commercial PMMA the crossings stay at or below
0.13 of the muon signal even in amplitude. The guide material (BC-802 or
BC-800 equivalent, both listed by Scionix for light guides) and the QE
curve below 400 nm are therefore the first things to pin down; the wrapping
matters less for this question. The 200 cm cap of the absorption length
above 420 nm and the 20 % digitisation error of the curves are the main
remaining uncertainties of the simulation side.

## References

* D. Pagano et al., *EcoMug: An Efficient COsmic MUon Generator for cosmic-ray
  muon applications*, JAIS 290 (2022).
* Particle Data Group, *Review of Particle Physics*.
* C. Hagmann, D. Lange, D. Wright, *Cosmic-ray shower generator (CRY) for
  Monte Carlo transport codes*, IEEE NSS Conference Record (2007).
