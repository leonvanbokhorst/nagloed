#!/usr/bin/env bash
# Full local validation for NAGLOED:
#   build plugin + tools, unit tests, demo render, offscreen UI snapshot,
#   Apple AU validation, and (if present) pluginval strictness 10 on AU + VST3.
#
# Usage: ./scripts/validate.sh [--quick]
#   --quick  skip the pluginval stage (it is the slow one)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
QUICK=0
[[ "${1:-}" == "--quick" ]] && QUICK=1

[ -d .deps/JUCE ] || ./scripts/fetch-juce.sh

echo "== configure =="
cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
      -DNAGLOED_JUCE_DIR="$ROOT/.deps/JUCE"

echo
echo "== build plugin (AU / VST3 / Standalone) + tools =="
cmake --build build -j"$(sysctl -n hw.ncpu)" --target \
      NAGLOED_AU NAGLOED_VST3 NAGLOED_Standalone \
      nagl_tests nagl_render nagl_preview

echo
echo "== unit / DSP tests =="
./build/nagl_tests_artefacts/Release/nagl_tests

echo
echo "== demo render =="
./build/nagl_render_artefacts/Release/nagl_render

echo
echo "== offscreen UI snapshot =="
./build/nagl_preview_artefacts/Release/nagl_preview

echo
echo "== editor refresh in a host-style (embedded) mount =="
./build/nagl_preview_artefacts/Release/nagl_preview --embedded

echo
echo "== grain-field phosphor soak (8 s) =="
./build/nagl_preview_artefacts/Release/nagl_preview --soak

echo
echo "== Apple AU validation =="
AU_SRC="$ROOT/build/NAGLOED_artefacts/Release/AU/NAGLOED.component"
AU_DEST="$HOME/Library/Audio/Plug-Ins/Components/NAGLOED.component"
mkdir -p "$(dirname "$AU_DEST")"
rm -rf "$AU_DEST"
cp -R "$AU_SRC" "$AU_DEST"
killall -9 AudioComponentRegistrar 2>/dev/null || true
for attempt in 1 2 3; do
    sleep 2
    if auval -v aufx Nagl MLLW; then break; fi
    [[ "$attempt" == 3 ]] && exit 1
done

PLUGINVAL="$ROOT/.deps/pluginval/pluginval.app/Contents/MacOS/pluginval"
if [[ "$QUICK" == 0 && -x "$PLUGINVAL" ]]; then
    echo
    echo "== pluginval strictness 10: VST3 =="
    "$PLUGINVAL" --strictness-level 10 \
        --validate "$ROOT/build/NAGLOED_artefacts/Release/VST3/NAGLOED.vst3" | tail -5
    echo
    echo "== pluginval strictness 10: AU =="
    "$PLUGINVAL" --strictness-level 10 \
        --validate "$AU_SRC" | tail -5
else
    echo
    echo "(pluginval skipped: --quick or $PLUGINVAL not installed)"
fi

echo
echo "== artifacts =="
find build/NAGLOED_artefacts/Release -maxdepth 2 \( -name '*.component' -o -name '*.vst3' -o -name '*.app' \) | sed 's/^/  /'
echo "  Demo/Nagloed-Demo.wav"
echo "  docs/ui-preview.png"
echo
echo "VALIDATION COMPLETE"
