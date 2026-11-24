#!/usr/bin/env bash
# soft_component_scan.sh: does the telescope count anything but muons?
#
# Vertical telescope (scintillators only, geometry mode 0), three sources:
#   ecomug   EcoMug muons 0.1-1000 GeV/c          (reference, as overburden_scan.sh)
#   crymu    CRY, muons only
#   cryall   CRY, full shower: mu, e+-, gamma, n, p, pi+-
# each without roof ("vac") and/or below the concrete floors ("slabs").
# CRY needs a build with  cmake -DWITH_CRY=ON -DCRY_DIR=$CRYHOME ..
#
#   cd build
#   ./soft_component_scan.sh                          # defaults below
#   RUNS="crymu:vac cryall:vac" SEEDS="1 2" ./soft_component_scan.sh
#   THICK=15 RUNS="crymu:slabs cryall:slabs ecomug:slabs" ./soft_component_scan.sh
#   N_CRYALL=200000 SEEDS=1 RUNS="cryall:vac" ./soft_component_scan.sh   # pilot: check showers/s
#
# Every job is one /run/beamOn: CRY's live time (timeSimulated) keeps counting
# across beamOn calls, so one job = one run = one live time.
# Several seeds per configuration are independent runs; the analysis
# (analysis/soft_component.py) adds their counts and live times.
set -euo pipefail

EXE="${EXE:-./Muon_Simulation}"
OUTDIR="${OUTDIR:-SoftComponent}"
RUNS="${RUNS:-ecomug:vac crymu:vac cryall:vac crymu:slabs cryall:slabs}"
SEEDS="${SEEDS:-1 2 3 4}"
JOBS="${JOBS:-4}"

# Events per job and seed. EcoMug: muons; CRY: showers with at least one
# particle on the plane. The live time per job is written to RunInfo
# (live_time_s); aim for a few thousand seconds per configuration in total.
N_ECO="${N_ECO:-1000000}"
N_CRYMU="${N_CRYMU:-1000000}"
N_CRYALL="${N_CRYALL:-2000000}"

# Building: same as overburden_scan.sh
NSLABS="${NSLABS:-5}"
THICK="${THICK:-20}"              # cm per floor (G4_CONCRETE)

# Generation plane: 1 mm above 5 x 30 cm of slabs (roof bottom at 1.30 m),
# so one plane serves every thickness up to 30 cm and the vacuum runs.
PLANE_Z="${PLANE_Z:-2.81}"        # m
ECO_SIZE="${ECO_SIZE:-2.4}"       # m, EcoMug sky plane side
CRY_BOX="${CRY_BOX:-3}"           # m, CRY sub-box side (tables: 1, 3, 10, 30, 100, 300 m)
CRY_LAT="${CRY_LAT:-48.0}"        # Freiburg
CRY_ALT="${CRY_ALT:-0}"           # only 0, 2100, 11300 m exist; sea level
CRY_DATE="${CRY_DATE:-9-1-2026}"  # month-day-year (solar modulation)
CRY_DATA="${CRY_DATA:-}"          # empty: data directory compiled in

[ -x "$EXE" ] || { echo "Executable $EXE not found (run from the build directory)"; exit 1; }
mkdir -p "$OUTDIR"
TMPD="$(mktemp -d)"
trap 'rm -rf "$TMPD"' EXIT

write_macro() {   # gen roof nevents file
    local gen=$1 roof=$2 nev=$3 f=$4
    local roofflag=false; [ "$roof" = slabs ] && roofflag=true
    {
        echo "/run/initialize"
        echo "/detector/zenith  0 deg"
        echo "/detector/azimuth 0 deg"
        echo "/detector/geometryMode 0"
        echo "/detector/roof          $roofflag"
        echo "/detector/roofSlabs     $NSLABS"
        echo "/detector/roofThickness $THICK cm"
        echo "/detector/roofSlabGap   0 m"
        echo "/detector/roofHeight    1.30 m"
        echo "/detector/roofHalfSize  2.0 m"
        echo "/detector/rebuild"
        if [ "$gen" = ecomug ]; then
            echo "/muonSource/ecomug/surface     sky"
            echo "/muonSource/ecomug/skySizeX    $ECO_SIZE m"
            echo "/muonSource/ecomug/skySizeY    $ECO_SIZE m"
            echo "/muonSource/ecomug/skyCenter   0 0 $PLANE_Z m"
            echo "/muonSource/ecomug/minMomentum 0.1 GeV"
            echo "/muonSource/ecomug/maxMomentum 1000 GeV"
        else
            local all=false; [ "$gen" = cryall ] && all=true
            [ -n "$CRY_DATA" ] && echo "/muonSource/cry/dataDir $CRY_DATA"
            echo "/muonSource/cry/boxLength     $CRY_BOX m"
            echo "/muonSource/cry/planeZ        $PLANE_Z m"
            echo "/muonSource/cry/altitude      $CRY_ALT m"
            echo "/muonSource/cry/latitude      $CRY_LAT"
            echo "/muonSource/cry/date          $CRY_DATE"
            echo "/muonSource/cry/returnMuons     true"
            echo "/muonSource/cry/returnElectrons $all"
            echo "/muonSource/cry/returnGammas    $all"
            echo "/muonSource/cry/returnNeutrons  $all"
            echo "/muonSource/cry/returnProtons   $all"
            echo "/muonSource/cry/returnPions     $all"
        fi
        echo "/run/beamOn $nev"
    } > "$f"
}

run_one() {   # gen roof seed
    local gen=$1 roof=$2 seed=$3
    local tag="$roof"; [ "$roof" = slabs ] && tag="slabs${NSLABS}_t${THICK}"
    local name="${gen}_${tag}_s${seed}"           # no dots: G4Analysis takes ".x" as extension
    local nev g
    case $gen in
        ecomug) nev=$N_ECO;    g=ecomug ;;
        crymu)  nev=$N_CRYMU;  g=cry ;;
        cryall) nev=$N_CRYALL; g=cry ;;
        *) echo "unknown generator $gen"; return 1 ;;
    esac
    if [ -f "$OUTDIR/${name}.root" ]; then echo "  ${name}: exists, skipped"; return 0; fi
    write_macro "$gen" "$roof" "$nev" "$TMPD/${name}.mac"
    cp "$TMPD/${name}.mac" "$OUTDIR/${name}.mac"
    echo "  ${name}: start (${nev} events)"
    local t0=$SECONDS
    if "$EXE" -m "$TMPD/${name}.mac" -o "$OUTDIR/${name}" -g "$g" -r "$seed" > "$OUTDIR/${name}.log" 2>&1; then
        echo "  ${name}: done in $((SECONDS - t0)) s"
    else
        rm -f "$OUTDIR/${name}.root"             # no partial file, so a rerun repeats the job
        if grep -q "NoCRY" "$OUTDIR/${name}.log"; then
            echo "  ${name}: FAILED, binary built without CRY (cmake -DWITH_CRY=ON -DCRY_DIR=\$CRYHOME ..)"
        else
            echo "  ${name}: FAILED, see $OUTDIR/${name}.log"
        fi
    fi
}

echo "Runs: $RUNS | seeds: $SEEDS | floors: ${NSLABS} x ${THICK} cm | jobs: $JOBS"
for r in $RUNS; do
    gen=${r%%:*}; roof=${r##*:}
    case $roof in vac|slabs) ;; *) echo "roof must be vac or slabs: $r"; exit 1 ;; esac
    for s in $SEEDS; do
        while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do sleep 5; done
        run_one "$gen" "$roof" "$s" &
    done
done
wait

echo
echo "Finished. Output in $OUTDIR/  ->  python analysis/soft_component.py $OUTDIR"
ls -lh "$OUTDIR"/*.root 2>/dev/null || true