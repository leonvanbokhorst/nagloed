#!/usr/bin/env bash
# NAGLOED v2 — SpectraStrobe A/B colour calibration (issue #31). DIAGNOSTIC
# EXPERIMENT, separate from NAGLOED 1.0.4, the artistic composition Eerste
# Gloed and the base colour calibration. The colour levels are a controlled
# diagnostic ladder, NOT artistic settings.
# Renders all variants deterministically and verifies them independently.
# Non-zero exit on FAIL.
set -euo pipefail

cd "$(dirname "$0")"

PYTHON="${PYTHON:-python3}"
mkdir -p out

echo "== 1/2 render =="
"$PYTHON" render_ab.py --ab ab.json --out-dir out

echo
echo "== 2/2 verify =="
"$PYTHON" verify_ab.py --ab ab.json --out-dir out

echo
echo "DONE: out/ab-report.md, out/ab-transition-envelopes.csv, out/ab-timeline.svg"
echo "DIAGNOSTIC — hardware only with ../../HARDWARE-PROTOCOL.md (binding safety reference)."
