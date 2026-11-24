#!/usr/bin/env bash
# optical_scan.sh: position scan of the light collection (optical mode)
#
# Usage:  ./optical_scan.sh [wrapping] [pmmaGrade] [nEvents] [nRef]
#         wrapping  : alfoil | tyvek | black      (default alfoil)
#         pmmaGrade : standard | uvt              (default standard)
#         nEvents   : events per point            (default 100)
#         nRef      : events of the two reference runs with stored photons
#                     (default 20; about 25k photon rows per event; 0 skips
#                     them, e.g. to rerun part of a grid)
#
# One output file per beam position: <outdir>/panel_x<X>_y<Y>.root for the
# scintillator grid and <outdir>/lg_x<X>_y<Y>.root for the light-guide grid,
# plus ref_panel.root and ref_lg.root with the PhotonHits ntuple. The beam
# position of every event is also in the Primaries ntuple (x0, y0).
# A panel-centre event takes about 18 s of CPU, a guide crossing well below
# 1 s, so the panel grid dominates: 49 points x 100 events is about one
# day of CPU. JOBS=<n> in the environment runs n points in parallel (on a
# 4-core machine 4 jobs gave only about 1.6 times the throughput of one).
set -euo pipefail

EXE="./Muon_Simulation"
TEMPLATE="macros/optical_scan_template.mac"
WRAPPING="${1:-alfoil}"
GRADE="${2:-standard}"
N="${3:-100}"
NREF="${4:-20}"
JOBS="${JOBS:-1}"
OUTDIR="Optical_Scan_${WRAPPING}_${GRADE}"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

# Scintillator panel grid (mm); the panel is 280 x 280 mm. The grids can be
# overridden from the environment, e.g. PANEL_X="-120 -40 40 120" for a
# coarser systematics run.
PANEL_X="${PANEL_X:--120 -80 -40 0 40 80 120}"
PANEL_Y="${PANEL_Y:--120 -80 -40 0 40 80 120}"
# Light-guide grid (mm); the guide spans x = -140 .. -210 mm and its width
# at x = -205 mm is still 76 mm, so y = +-20 mm stays inside at every x.
LG_X="${LG_X:--145 -155 -165 -175 -185 -195 -205}"
LG_Y="${LG_Y:--20 0 20}"

mkdir -p "$OUTDIR"

# run_point <name> <x> <y> <nEvents> <storePhotons>
run_point() {
    local name=$1 x=$2 y=$3 n=$4 store=$5
    # Distinct seed per point, far apart (see zenith_scan.sh).
    local seed=$((1000003 + (x + 300) * 7919 + (y + 300) * 104729))
    local macro="$TMPDIR/${name}.mac"
    sed -e "s/XPOS/$x/" -e "s/YPOS/$y/" -e "s/NEVENTS/$n/" \
        -e "s/WRAPPING/$WRAPPING/" -e "s/PMMAGRADE/$GRADE/" \
        -e "s/storePhotons false/storePhotons $store/" "$TEMPLATE" > "$macro"
    echo "=== $name | events = $n | seed = $seed"
    while [ "$(jobs -r | wc -l)" -ge "$JOBS" ]; do wait -n; done
    "$EXE" -m "$macro" -o "$OUTDIR/$name" -g beam -p -r "$seed" > "$OUTDIR/$name.log" 2>&1 &
}

# Reference runs with the PhotonHits ntuple (wavelengths and arrival times
# for the quantum efficiency and the pulse shapes in the analysis).
if [ "$NREF" -gt 0 ]; then
    run_point ref_panel 0 0 "$NREF" true
    run_point ref_lg -175 0 "$NREF" true
fi

for x in $PANEL_X; do
    for y in $PANEL_Y; do run_point "panel_x${x}_y${y}" "$x" "$y" "$N" false; done
done
for x in $LG_X; do
    for y in $LG_Y; do run_point "lg_x${x}_y${y}" "$x" "$y" "$N" false; done
done
wait

echo
echo "Scan finished. Output in $OUTDIR/"
