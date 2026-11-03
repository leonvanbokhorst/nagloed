#!/usr/bin/env bash
# Rebuild + run local test suite + Apple AU validation + VST3 smoke checks.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

./scripts/build.sh
echo
echo "== unit tests =="
./build/nagl_tests_artefacts/Release/nagl_tests

echo
echo "== auval =="
AU="$ROOT/build/NAGLOED_artefacts/Release/AU/NAGLOED.component"
DEST="$HOME/Library/Audio/Plug-Ins/Components/NAGLOED.component"
if [ -d "$AU" ]; then
  rm -rf "$DEST"; cp -R "$AU" "$DEST"
  killall -9 AudioComponentRegistrar 2>/dev/null || true
  sleep 1
  auval -v aufx Nagl UnlN
fi
