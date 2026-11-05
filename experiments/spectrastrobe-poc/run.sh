#!/usr/bin/env bash
# NAGLOED v2 — SpectraStrobe proof-of-concept. EXPERIMENT, geen deel van v1.0.4.
# Rendert de score deterministisch en verifieert de signaalintegriteit (M1–M10)
# plus de adviserende corruptiematrix (M14). Exitcode != 0 bij een FAIL.
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
echo "KLAAR: out/verify-report.md en out/verify-report.json"
echo "EXPERIMENT — niet op de hardware zonder HARDWARE-PROTOCOL.md."
