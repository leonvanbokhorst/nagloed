#!/usr/bin/env bash
#
# uninstall-soothe3.sh
# Removes oeksound soothe3 v1.0.5 (AU / VST3 / AAX) from this Mac.
#
# Verified targets (root-owned unless noted):
#   /Library/Audio/Plug-Ins/Components/soothe3.component            (AU)
#   /Library/Audio/Plug-Ins/VST3/soothe3.vst3                        (VST3)
#   /Library/Application Support/Avid/Audio/Plug-Ins/soothe3.aaxplugin (AAX)
#   /Library/Application Support/oeksound/soothe3                    (factory presets + manual)
#   ~/Library/Application Support/oeksound/soothe3                   (user config/preset cache)
#   pkg receipts: com.oeksound.soothe3.{common,au,vst3,aax}
#
# This script does NOT touch your iLok license, iLok License Manager, or
# anything under /Library/Application Support/PACEAntiPiracy. The license
# stays in your iLok account, so you can reinstall soothe3 later.
#
# Usage:
#   bash uninstall-soothe3.sh --dry-run   # show what would be removed
#   bash uninstall-soothe3.sh             # interactive, asks for confirmation
#   bash uninstall-soothe3.sh --yes       # no confirmation prompt
#
set -euo pipefail

DRY_RUN=0
ASSUME_YES=0

for arg in "$@"; do
  case "$arg" in
    --dry-run) DRY_RUN=1 ;;
    --yes|-y)  ASSUME_YES=1 ;;
    -h|--help)
      sed -n '2,30p' "$0" | sed 's/^# \{0,1\}//'
      exit 0
      ;;
    *) echo "Unknown argument: $arg" >&2; exit 2 ;;
  esac
done

log()  { printf '%s\n' "$*"; }
warn() { printf 'WARNING: %s\n' "$*" >&2; }
die()  { printf 'ERROR: %s\n' "$*" >&2; exit 1; }

PLUGIN_DIRS=(
  "/Library/Audio/Plug-Ins/Components/soothe3.component"
  "/Library/Audio/Plug-Ins/VST3/soothe3.vst3"
  "/Library/Application Support/Avid/Audio/Plug-Ins/soothe3.aaxplugin"
)
ROOT_SUPPORT_DIRS=(
  "/Library/Application Support/oeksound/soothe3"
)
USER_SUPPORT_DIRS=(
  "$HOME/Library/Application Support/oeksound/soothe3"
)
RECEIPTS=(
  com.oeksound.soothe3.common
  com.oeksound.soothe3.au
  com.oeksound.soothe3.vst3
  com.oeksound.soothe3.aax
)

# ---------------------------------------------------------------- guards ----
# Refuse to operate on anything that is not the expected soothe3 path.
assert_safe_path() {
  local p="$1"
  [[ -n "$p" ]] || die "empty path"
  case "$p" in
    /Library/*|"$HOME"/*) ;;
    *) die "refusing to delete outside /Library or \$HOME: $p" ;;
  esac
  [[ "$p" == *soothe3* ]] || die "refusing to delete path without 'soothe3': $p"
  [[ "$p" != "/" && "$p" != "$HOME" ]] || die "refusing to delete root path"
}

for p in "${PLUGIN_DIRS[@]}" "${ROOT_SUPPORT_DIRS[@]}" "${USER_SUPPORT_DIRS[@]}"; do
  assert_safe_path "$p"
done

# ------------------------------------------------------- running hosts ----
# Deleting a plugin bundle out from under a running host can wedge the host.
HOSTS=("REAPER" "Logic Pro" "Pro Tools" "Studio One" "Cubase" "Nuendo" "Bitwig Studio" "Ableton Live")
RUNNING=()
for h in "${HOSTS[@]}"; do
  if pgrep -qf "$h" 2>/dev/null; then RUNNING+=("$h"); fi
done

if ((${#RUNNING[@]})); then
  warn "audio host(s) running: ${RUNNING[*]}"
fi

# ----------------------------------------------------------- dry run ----
if ((DRY_RUN)); then
  log "DRY RUN — nothing will be deleted."
  log ""
  log "Would remove (with sudo):"
  for p in "${PLUGIN_DIRS[@]}" "${ROOT_SUPPORT_DIRS[@]}"; do
    [[ -e "$p" ]] && log "  $p" || log "  (absent) $p"
  done
  log ""
  log "Would remove (no sudo):"
  for p in "${USER_SUPPORT_DIRS[@]}"; do
    [[ -e "$p" ]] && log "  $p" || log "  (absent) $p"
  done
  log ""
  log "Would forget package receipts:"
  for r in "${RECEIPTS[@]}"; do
    pkgutil --pkg-info "$r" >/dev/null 2>&1 && log "  $r" || log "  (not registered) $r"
  done
  exit 0
fi

# ------------------------------------------------------- quit hosts ----
# Dry run has already returned, so reaching here means real deletion.
if ((${#RUNNING[@]})); then
  if [[ " ${RUNNING[*]} " == *" REAPER "* ]]; then
    log "Asking REAPER to quit (save your work first if it prompts)..."
    if ! osascript -e 'tell application "REAPER" to quit' >/dev/null 2>&1; then
      warn "Could not quit REAPER via AppleScript."
    fi
    for _ in $(seq 1 30); do
      pgrep -qx "REAPER" 2>/dev/null || break
      sleep 1
    done
    if pgrep -qx "REAPER" 2>/dev/null; then
      warn "REAPER is still running. Quit it manually before continuing."
      die "aborting so the plugin bundles are not deleted while in use"
    fi
    log "REAPER quit."
    REMAINING=()
    for h in "${HOSTS[@]}"; do
      [[ "$h" == "REAPER" ]] && continue
      if pgrep -qf "$h" 2>/dev/null; then REMAINING+=("$h"); fi
    done
    if ((${#REMAINING[@]})); then
      die "other audio host(s) still running: ${REMAINING[*]} — quit them and re-run"
    fi
  else
    die "quit ${RUNNING[*]} first, then re-run this script"
  fi
fi

# ----------------------------------------------------- confirmation ----
PRESENT=0
for p in "${PLUGIN_DIRS[@]}" "${ROOT_SUPPORT_DIRS[@]}" "${USER_SUPPORT_DIRS[@]}"; do
  [[ -e "$p" ]] && PRESENT=1
done
if ((!PRESENT)); then
  log "Nothing to remove — none of the soothe3 paths exist."
  exit 0
fi

log "This will permanently delete:"
for p in "${PLUGIN_DIRS[@]}" "${ROOT_SUPPORT_DIRS[@]}"; do
  [[ -e "$p" ]] && log "  $p  (sudo)"
done
for p in "${USER_SUPPORT_DIRS[@]}"; do
  [[ -e "$p" ]] && log "  $p"
done
log "  plus ${#RECEIPTS[@]} installer receipts"
log ""
log "Your iLok license will NOT be touched."

if ((!ASSUME_YES)); then
  printf 'Type "yes" to continue: '
  read -r reply
  [[ "$reply" == "yes" ]] || { log "Aborted."; exit 1; }
fi

# Cache sudo credentials up front so the deletions do not prompt mid-way.
if ! sudo -v; then
  die "sudo failed — this step needs your admin password"
fi

# ----------------------------------------------------------- deletion ----
for p in "${PLUGIN_DIRS[@]}"; do
  if [[ -e "$p" ]]; then
    log "removing $p"
    sudo rm -rf -- "$p"
  fi
done

for p in "${ROOT_SUPPORT_DIRS[@]}"; do
  if [[ -e "$p" ]]; then
    log "removing $p"
    sudo rm -rf -- "$p"
  fi
done

# Remove the parent oeksound folder only if it is now empty.
if [[ -d "/Library/Application Support/oeksound" ]]; then
  sudo rmdir "/Library/Application Support/oeksound" 2>/dev/null || true
fi

for p in "${USER_SUPPORT_DIRS[@]}"; do
  if [[ -e "$p" ]]; then
    log "removing $p"
    rm -rf -- "$p"
  fi
done
if [[ -d "$HOME/Library/Application Support/oeksound" ]]; then
  rmdir "$HOME/Library/Application Support/oeksound" 2>/dev/null || true
fi

# ----------------------------------------------------------- receipts ----
for r in "${RECEIPTS[@]}"; do
  if pkgutil --pkg-info "$r" >/dev/null 2>&1; then
    log "forgetting receipt $r"
    sudo pkgutil --forget "$r" >/dev/null
  fi
done

# Refresh the AudioComponent cache so hosts stop listing the removed AU.
killall -u "$USER" AudioComponentRegistrar 2>/dev/null || true

# --------------------------------------------------------- verification ----
FAIL=0
for p in "${PLUGIN_DIRS[@]}" "${ROOT_SUPPORT_DIRS[@]}" "${USER_SUPPORT_DIRS[@]}"; do
  [[ -e "$p" ]] && { warn "still present: $p"; FAIL=1; }
done
for r in "${RECEIPTS[@]}"; do
  pkgutil --pkg-info "$r" >/dev/null 2>&1 && { warn "receipt still registered: $r"; FAIL=1; }
done

log ""
if ((FAIL)); then
  die "uninstall incomplete — see warnings above"
fi
log "soothe3 uninstalled. Open REAPER and rescan plug-ins; soothe3 should be gone."
