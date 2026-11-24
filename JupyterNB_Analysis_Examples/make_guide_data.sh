#!/usr/bin/env bash
# make_guide_data.sh -- small example runs for Simulation_Analysis_Guide.ipynb
#
# Run it from the build directory:
#
#     cd build
#     bash ../JupyterNB_Analysis_Examples/make_guide_data.sh
#
# The files go to build/Guide_Example/. The event numbers are small, so the
# whole set takes a few minutes. They are meant for learning the analysis,
# not for results: increase N_* for real numbers.
#
# Optional settings (environment variables):
#   N_FLAT=200000 N_SCAN=200000 N_ISO=2000000 N_OPT=10 ./make_guide_data.sh
#   SKIP_OPTICAL=1    skip the optical run (it needs about 20 s per event)

set -euo pipefail

EXE="./Muon_Simulation"
RUNNER=${RUNNER:-}      # optional prefix, e.g. "micromamba run -n geant4"
OUT="Guide_Example"
N_FLAT=${N_FLAT:-200000}
N_SCAN=${N_SCAN:-200000}
N_ISO=${N_ISO:-2000000}
N_OPT=${N_OPT:-10}

if [ ! -x "$EXE" ]; then
    echo "Muon_Simulation not found. Run this script from the build directory."
    exit 1
fi

# Geant4 reads numbers with the C locale (see README: Turkish locale problem)
export LC_ALL=C

mkdir -p "$OUT/macros"

# ---------------------------------------------------------------------------
# 1. EcoMug, flat sky, vertical telescope, scintillators only (mode 0)
# ---------------------------------------------------------------------------
cat > "$OUT/macros/flat_mode0.mac" <<MAC
/run/initialize
/muonSource/ecomug/surface   sky
/muonSource/ecomug/skySizeX  2 m
/muonSource/ecomug/skySizeY  2 m
/muonSource/ecomug/skyCenter 0 0 2.0 m
/muonSource/ecomug/minMomentum 0.1 GeV
/muonSource/ecomug/maxMomentum 1000 GeV
/run/beamOn $N_FLAT
MAC
echo ">>> flat_mode0"
$RUNNER "$EXE" -m "$OUT/macros/flat_mode0.mac" -o "$OUT/flat_mode0" -r 12345 > "$OUT/flat_mode0.log"

# ---------------------------------------------------------------------------
# 2. The same muons (same seed), scintillators + light guides (mode 2)
# ---------------------------------------------------------------------------
cat > "$OUT/macros/flat_mode2.mac" <<MAC
/run/initialize
/detector/geometryMode 2
/detector/rebuild
/muonSource/ecomug/surface   sky
/muonSource/ecomug/skySizeX  2 m
/muonSource/ecomug/skySizeY  2 m
/muonSource/ecomug/skyCenter 0 0 2.0 m
/muonSource/ecomug/minMomentum 0.1 GeV
/muonSource/ecomug/maxMomentum 1000 GeV
/run/beamOn $N_FLAT
MAC
echo ">>> flat_mode2"
$RUNNER "$EXE" -m "$OUT/macros/flat_mode2.mac" -o "$OUT/flat_mode2" -r 12345 > "$OUT/flat_mode2.log"

# ---------------------------------------------------------------------------
# 3. Small zenith scan, EcoMug hemisphere (same layout as zenith_scan.sh)
# ---------------------------------------------------------------------------
mkdir -p "$OUT/Scan"
for THETA in 0 10 20 30 45 60; do
    SEED=$((1000003 + THETA * 7919))
    cat > "$OUT/macros/scan_${THETA}.mac" <<MAC
/run/initialize
/detector/zenith $THETA deg
/detector/rebuild
/muonSource/ecomug/surface hsphere
/muonSource/ecomug/hsphereRadius 1.40 m
/muonSource/ecomug/hsphereCenter 0 0 0 m
/muonSource/ecomug/minMomentum 0.1 GeV
/muonSource/ecomug/maxMomentum 1000 GeV
/run/beamOn $N_SCAN
MAC
    echo ">>> scan theta = $THETA"
    $RUNNER "$EXE" -m "$OUT/macros/scan_${THETA}.mac" -o "$OUT/Scan/theta_${THETA}" -r "$SEED" \
        > "$OUT/Scan/theta_${THETA}.log"
done

# ---------------------------------------------------------------------------
# 4. Isotropic (cosine-law) source for the geometric factor G0
# ---------------------------------------------------------------------------
cat > "$OUT/macros/isotropic_plane.mac" <<MAC
/run/initialize
/muonSource/isotropic/surface plane
/muonSource/isotropic/size 2 m
/muonSource/isotropic/height 1.3 m
/muonSource/isotropic/momentum 2 GeV
/run/beamOn $N_ISO
MAC
echo ">>> isotropic_plane"
$RUNNER "$EXE" -m "$OUT/macros/isotropic_plane.mac" -o "$OUT/isotropic_plane" -g isotropic -r 777 \
    > "$OUT/isotropic_plane.log"

# ---------------------------------------------------------------------------
# 5. Optical mode: vertical 2 GeV/c mu- through the panel centre, photons stored
# ---------------------------------------------------------------------------
if [ "${SKIP_OPTICAL:-0}" != "1" ]; then
    cat > "$OUT/macros/optical_beam.mac" <<MAC
/run/initialize
/detector/geometryMode 2
/detector/optics/wrapping alfoil
/detector/optics/pmmaGrade standard
/detector/optics/storePhotons true
/detector/rebuild
/muonSource/beam/particle  mu-
/muonSource/beam/momentum  2 GeV
/muonSource/beam/position  0 0 1150 mm
/muonSource/beam/direction 0 0 -1
/run/beamOn $N_OPT
MAC
    echo ">>> optical_beam (about 20 s per event)"
    $RUNNER "$EXE" -m "$OUT/macros/optical_beam.mac" -o "$OUT/optical_beam" -g beam -p -r 1 \
        > "$OUT/optical_beam.log"
fi

echo
echo "Done. Files in $OUT/:"
find "$OUT" -name "*.root" | sort
