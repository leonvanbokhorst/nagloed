#!/usr/bin/env bash
# NAGLOED v2 — SpectraStrobe proof-of-concept. EXPERIMENT, not part of v1.0.4.
# Renders the score deterministically and verifies signal integrity (M1–M10)
# plus the advisory corruption matrix (M14). Non-zero exit on FAIL.
set -euo pipefail

cd "$(dirname "$0")"

PYTHON="${PYTHON:-python3}"
mkdir -p out

echo "== 1/2 render =="
"$PYTHON" render.py --score score.json --out-dir out

echo
echo "== 2/2 verify =="
"$PYTHON" verify.py --score score.json --out-dir out

echo
echo "DONE: out/verify-report.md and out/verify-report.json"
echo "EXPERIMENT — do not use on hardware without HARDWARE-PROTOCOL.md."
