#!/usr/bin/env bash
# Configure + build NAGLOED (AU/VST3/Standalone + dev tools).
#   ./scripts/build.sh            # everything
#   ./scripts/build.sh NAGLOED    # one target
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
[ -d .deps/JUCE ] || ./scripts/fetch-juce.sh
cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DNAGLOED_JUCE_DIR="$ROOT/.deps/JUCE" >/dev/null
cmake --build build -j"$(sysctl -n hw.ncpu)" --target "${@:-all}"
echo
echo "Artifacts:"
find build/NAGLOED_artefacts -maxdepth 3 \( -name '*.component' -o -name '*.vst3' -o -name '*.app' \) 2>/dev/null | sed 's/^/  /'
