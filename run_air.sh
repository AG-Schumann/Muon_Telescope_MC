#!/usr/bin/env bash
# run_air.sh: vertical runs with the world filled with vacuum or air, from <proje_dir>/build_light
#
#   ./run_air.sh            four runs: ecomug_vacuum, cry_vacuum, ecomug_air, cry_air
#
# Environment (optional):
#   NEV=1000000      EcoMug muons (0.1-1000 GeV/c, hemisphere R = 1.4 m)
#   CRY_N=1000000    CRY showers (all particles, 2 m box at z = 1.35 m)
#   MODE=2           geometry mode (2 = scintillators + light guides)
#   JOBS=4           runs in parallel
#   OUT=air          output directory
#
# Output: <OUT>/<run>/theta_0.root (+ .mac, .log), readable with scan_analysis.load_events.
# The same seed is used with and without air, so the two EcoMug runs (and the two CRY runs)
# contain the same primaries.
set -euo pipefail

EXE=./Muon_Simulation
OUT=${OUT:-air}
NEV=${NEV:-1000000}
CRY_N=${CRY_N:-1000000}
MODE=${MODE:-2}
JOBS=${JOBS:-4}
SEED_ECO=1000003          # = seed of theta_0 in run_scans.sh
SEED_CRY=$(( SEED_ECO + 31 ))

[ -x "$EXE" ] || { echo "Muon_Simulation not found in $(pwd)"; exit 1; }
grep -q "worldMaterialIsAir" "$EXE" || { echo "this build has no /detector/worldMaterialIsAir"; exit 1; }
grep -q "/muonSource/cry/" "$EXE" || { echo "built without CRY (cmake -DWITH_CRY=ON)"; exit 1; }
mkdir -p "$OUT"

header() {   # $1 air true|false
    cat <<EOF
/control/verbose 0
/run/verbose 0
/event/verbose 0
/tracking/verbose 0
/run/initialize
/detector/geometryMode $MODE
/detector/worldMaterialIsAir $1
/detector/zenith 0 deg
/detector/rebuild
EOF
}

ecomug_source() {
    cat <<EOF
/muonSource/ecomug/surface hsphere
/muonSource/ecomug/hsphereRadius 1.4 m
/muonSource/ecomug/hsphereCenter 0 0 0 m
/muonSource/ecomug/minMomentum 0.1 GeV
/muonSource/ecomug/maxMomentum 1000 GeV
/run/beamOn $NEV
EOF
}

cry_source() {
    cat <<EOF
/muonSource/cry/boxLength 2 m
/muonSource/cry/planeZ 1.35 m
/muonSource/cry/altitude 0 m
/muonSource/cry/latitude 48.0
/muonSource/cry/date 10-1-2026
/muonSource/cry/returnMuons true
/muonSource/cry/returnElectrons true
/muonSource/cry/returnGammas true
/muonSource/cry/returnNeutrons true
/muonSource/cry/returnProtons true
/muonSource/cry/returnPions true
/run/beamOn $CRY_N
EOF
}

run_one() {   # $1 name, $2 generator, $3 seed, $4 air
    local dir="$OUT/$1" base
    mkdir -p "$dir"; base="$dir/theta_0"
    if [ -f "$base.root" ]; then echo "  skip   $1 (done)"; return 0; fi
    { header "$4"; if [ "$2" = ecomug ]; then ecomug_source; else cry_source; fi; } > "$base.mac"
    local t0=$SECONDS
    if "$EXE" -m "$base.mac" -g "$2" -o "${base}_tmp" -r "$3" > "$base.log" 2>&1 && [ -f "${base}_tmp.root" ]; then
        mv "${base}_tmp.root" "$base.root"
        echo "  done   $1  ($(( SECONDS - t0 )) s)"
    else
        echo "  FAILED $1 -> see $base.log"
    fi
}

run_one ecomug_vacuum ecomug "$SEED_ECO" false &
[ "$JOBS" -ge 2 ] || wait
run_one cry_vacuum    cry    "$SEED_CRY" false &
[ "$JOBS" -ge 3 ] || wait
run_one ecomug_air    ecomug "$SEED_ECO" true  &
[ "$JOBS" -ge 4 ] || wait
run_one cry_air       cry    "$SEED_CRY" true  &
wait
echo "All runs finished. Output in $OUT/"