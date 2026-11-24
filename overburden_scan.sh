#!/usr/bin/env bash
# overburden_scan.sh: vertical rate without and with the concrete floors
#
# Runs the vacuum reference and one run per slab thickness from
# macros/overburden_vertical_template.mac. All runs use the same generation
# plane and the same seed, so EcoMug generates the same muons in every run and
# analysis/overburden_analysis.py takes the ratio event by event.
#
#   cd build
#   ./overburden_scan.sh                    # defaults below
#   NEVENTS=4000000 SEED=777 ./overburden_scan.sh
#   NSLABS=5 THICKNESSES="20 25" ./overburden_scan.sh
#
# Independent repetitions (to add statistics) need a different SEED; files of
# different seeds can be combined at the analysis stage.
set -euo pipefail

EXE="./Muon_Simulation"
TEMPLATE="macros/overburden_vertical_template.mac"
OUTDIR="${OUTDIR:-Overburden}"
NEVENTS="${NEVENTS:-1000000}"
SEED="${SEED:-20261108}"
NSLABS="${NSLABS:-5}"
THICKNESSES="${THICKNESSES:-15 20 25 30}"   # cm per slab; the plane in the template sits above 5 x 30 cm
JOBS="${JOBS:-1}"                           # parallel processes

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
mkdir -p "$OUTDIR"

run_one() {   # name roof thick
    local name=$1 roof=$2 thick=$3
    local macro="$TMPDIR/${name}.mac"
    sed -e "s/ROOF/$roof/" -e "s/NSLABS/$NSLABS/" -e "s/THICK/$thick/" \
        -e "s/NEVENTS/$NEVENTS/" "$TEMPLATE" > "$macro"
    echo " ${name}: roof=${roof} slabs=${NSLABS} x ${thick} cm, events=${NEVENTS}, seed=${SEED}"
    local t0=$SECONDS
    if "$EXE" -m "$macro" -o "$OUTDIR/${name}" -r "$SEED" > "$OUTDIR/${name}.log" 2>&1; then
        echo "  ${name} finished in $((SECONDS - t0)) s"
    else
        echo "  ${name} FAILED, see $OUTDIR/${name}.log"
    fi
}

# The thickness in the vacuum run is ignored (roof false).
run_one vacuum false 25 &
for T in $THICKNESSES; do
    while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do sleep 5; done
    run_one "slabs${NSLABS}_t${T}" true "$T" &
done
wait

echo
echo "Scan finished. Output in $OUTDIR/"
ls -lh "$OUTDIR"/*.root
