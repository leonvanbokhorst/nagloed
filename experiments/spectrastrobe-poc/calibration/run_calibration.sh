#!/usr/bin/env bash
# NAGLOED v2 — SpectraStrobe colour calibration. DIAGNOSTIC EXPERIMENT,
# separate from NAGLOED 1.0.4 and from the artistic composition Eerste Gloed.
# Renders the colour sequence deterministically and verifies it independently.
# Non-zero exit on FAIL.
set -euo pipefail

cd "$(dirname "$0")"

PYTHON="${PYTHON:-python3}"
mkdir -p out

echo "== 1/2 render =="
"$PYTHON" render_calibration.py --calibration calibration.json --out-dir out

echo
echo "== 2/2 verify =="
"$PYTHON" verify_calibration.py --calibration calibration.json --out-dir out

echo
echo "DONE: out/calibration-report.md, out/colour-timeline.csv, out/colour-timeline.svg"
echo "DIAGNOSTIC — hardware only together with ../HARDWARE-PROTOCOL.md (binding safety reference)."
