#!/usr/bin/env bash
# run_scans.sh: zenith scans for the thesis, run from <proje_dir>/build_light
#
#   ./run_scans.sh                 everything (EcoMug: mode 0, mode 2, flipped; CRY: muons, all)
#   ./run_scans.sh ecomug          only the three EcoMug scans
#   ./run_scans.sh cry             only the two CRY scans
#
# Environment (optional):
#   JOBS=6          processes in parallel (default: cores - 1)
#   ANGLES="0 30"   zenith angles in deg (default 0 10 ... 90)
#   NEV=1000000     EcoMug muons per angle
#   CRY_N=1000000   CRY showers per angle for a 2 m box (scaled with the box area, see below)
#   CRY_MODE=2      geometry mode of the CRY runs (2 = scintillators + light guides, 0 = only scintillators)
#   FLIP_PANEL=1    panel whose light guide is flipped (1 = second from the top, rotation test)
#   OUT=scans       output directory
#
# Output: <OUT>/<scan>/theta_<deg>.root, with the macro (.mac) and the log (.log) of every run.
# A finished run is skipped when the script is started again, so it can be interrupted.
#
# Seeds: EcoMug runs use seed = 1000003 + 7919*theta in all three EcoMug scans. EcoMug has its own
# random generator, so mode 0, mode 2 and flipped see exactly the same muons at a given angle.
set -euo pipefail

EXE=./Muon_Simulation
OUT=${OUT:-scans}
NCPU=$(nproc 2>/dev/null || echo 2)
JOBS=${JOBS:-$(( NCPU > 1 ? NCPU - 1 : 1 ))}
ANGLES=${ANGLES:-"0 10 20 30 40 50 60 70 80 90"}
NEV=${NEV:-1000000}
CRY_N=${CRY_N:-1000000}
CRY_MODE=${CRY_MODE:-2}
FLIP_PANEL=${FLIP_PANEL:-1}
WHAT=${1:-all}

[ -x "$EXE" ] || { echo "Muon_Simulation not found in $(pwd)"; exit 1; }

# what the binary supports
DO_FLIP=yes; DO_CRY=yes
grep -q "flipLightGuide" "$EXE" || { DO_FLIP=no; echo "WARNING: /detector/flipLightGuide not in this build -> flipped scan skipped"; }
grep -q "/muonSource/cry/" "$EXE" || { DO_CRY=no;  echo "WARNING: built without CRY (cmake -DWITH_CRY=ON) -> CRY scans skipped"; }

mkdir -p "$OUT"
JOBLIST="$OUT/jobs_$$.txt"
: > "$JOBLIST"
trap 'rm -f "$JOBLIST"' EXIT

# macro pieces
geometry() {   # $1 mode, $2 flipped panel (-1 = none), $3 zenith
    cat <<EOF
/control/verbose 0
/run/verbose 0
/event/verbose 0
/tracking/verbose 0
/run/initialize
/detector/geometryMode $1
EOF
    [ "$2" -ge 0 ] && echo "/detector/flipLightGuide $2"
    cat <<EOF
/detector/zenith $3 deg
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
EOF
}

# CRY starts every particle on a horizontal square of side L at z = 1.35 m (above the frame at
# every tilt). An inclined telescope sees muons that cross this plane far from the axis, so the
# square grows with the angle. The number of showers grows with the area, so that the live time
# stays about the same at every angle.
cry_box() {   # $1 zenith -> box length in m
    if   [ "$1" -le 20 ]; then echo 2
    elif [ "$1" -le 40 ]; then echo 4
    else                       echo 8
    fi
}

cry_source() {   # $1 true|false (all particles), $2 box length
    cat <<EOF
/muonSource/cry/boxLength $2 m
/muonSource/cry/planeZ 1.35 m
/muonSource/cry/altitude 0 m
/muonSource/cry/latitude 48.0
/muonSource/cry/date 10-1-2026
/muonSource/cry/returnMuons true
/muonSource/cry/returnElectrons $1
/muonSource/cry/returnGammas $1
/muonSource/cry/returnNeutrons $1
/muonSource/cry/returnProtons $1
/muonSource/cry/returnPions $1
EOF
}

add_job() {   # $1 scan name, $2 generator, $3 zenith, $4 seed, $5 events, $6 macro body
    local dir="$OUT/$1" base
    mkdir -p "$dir"
    base="$dir/theta_$3"
    [ -f "$base.root" ] && return 0                     # finished earlier
    { echo "$6"; echo "/run/beamOn $5"; } > "$base.mac"
    echo "$2|$base|$4" >> "$JOBLIST"
}

# job list
for th in $ANGLES; do
    seed=$(( 1000003 + 7919 * th ))
    if [ "$WHAT" = all ] || [ "$WHAT" = ecomug ]; then
        add_job ecomug_mode0  ecomug "$th" "$seed" "$NEV" "$(geometry 0 -1 "$th"; ecomug_source)"
        add_job ecomug_mode2  ecomug "$th" "$seed" "$NEV" "$(geometry 2 -1 "$th"; ecomug_source)"
        [ "$DO_FLIP" = yes ] && \
        add_job ecomug_flip${FLIP_PANEL} ecomug "$th" "$seed" "$NEV" "$(geometry 2 "$FLIP_PANEL" "$th"; ecomug_source)"
    fi
    if { [ "$WHAT" = all ] || [ "$WHAT" = cry ]; } && [ "$DO_CRY" = yes ]; then
        L=$(cry_box "$th")
        n=$(( CRY_N * L * L / 4 ))
        add_job cry_muons cry "$th" "$(( seed + 17 ))" "$n" "$(geometry "$CRY_MODE" -1 "$th"; cry_source false "$L")"
        add_job cry_all   cry "$th" "$(( seed + 31 ))" "$n" "$(geometry "$CRY_MODE" -1 "$th"; cry_source true  "$L")"
    fi
done

N=$(wc -l < "$JOBLIST" | tr -d ' ')
echo "$N runs to do, $JOBS in parallel, output in $OUT/"
[ "$N" -gt 0 ] || exit 0

run_one() {   # line "gen|base|seed"
    IFS='|' read -r gen base seed <<< "$1"
    local t0=$SECONDS
    if "$EXE" -m "$base.mac" -g "$gen" -o "${base}_tmp" -r "$seed" > "$base.log" 2>&1 \
       && [ -f "${base}_tmp.root" ]; then
        mv "${base}_tmp.root" "$base.root"
        echo "  done   ${base#./}  ($(( SECONDS - t0 )) s)"
    else
        echo "  FAILED ${base#./}  -> see $base.log"
    fi
}
export -f run_one
export EXE

xargs -P "$JOBS" -I{} bash -c 'run_one "$@"' _ {} < "$JOBLIST"
echo "All runs finished. Failed runs have no .root file; start the script again to repeat them."