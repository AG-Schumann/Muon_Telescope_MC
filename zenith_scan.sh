#!/usr/bin/env bash
# zenith_scan.sh: runs the telescope zenith scan
set -euo pipefail

EXE="./Muon_Simulation"
TEMPLATE="macros/zenith_scan_template.mac"
OUTDIR="Lab_Hemisphere_Scanning"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

# Angles to scan, and events per angle. The geometric acceptance falls off
# quickly with zenith angle, so more statistics are needed at large angles to
# keep the relative uncertainty roughly constant.
declare -A NEVENTS=(
  [0]=1000000   [5]=1000000   [10]=1000000  [15]=1000000
  [20]=1000000 [25]=1000000 [30]=1000000 [40]=1000000  [50]=1000000 [60]=1000000 [70]=1000000 [80]=1000000
  [85]=1000000 [90]=1000000
)

mkdir -p "$OUTDIR"

for THETA in "${!NEVENTS[@]}"; do
    N=${NEVENTS[$THETA]}
    # Prime multiplier keeps consecutive seeds far apart: EcoMug's SetSeed
    # writes the same value into both words of the xoroshiro state, so nearby
    # seeds start from a nearly degenerate point.
    SEED=$((1000003 + THETA * 7919))
    MACRO="$TMPDIR/scan_${THETA}.mac"

    sed -e "s/ZENITH/$THETA/" -e "s/NEVENTS/$N/" "$TEMPLATE" > "$MACRO"

    echo "=========================================================="
    echo " zenith = ${THETA} deg | events = ${N} | seed = ${SEED}"
    echo "=========================================================="

    /usr/bin/time -f "  elapsed: %E  maxRSS: %MkB" \
        "$EXE" -m "$MACRO" -o "$OUTDIR/theta_${THETA}" -r "$SEED"
done

echo
echo "Scan finished. Output in $OUTDIR/"
ls -lh "$OUTDIR"