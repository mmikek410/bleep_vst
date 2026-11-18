#!/bin/bash
# Installs the BLEEP plug-ins in this folder into your user plug-in folders.
# Works for both "BLEEP 001" and "BLEEP 001 Classic"; they install side by side.
# No Xcode needed. Run from Terminal:  bash install.sh
set -euo pipefail
cd "$(dirname "$0")"
shopt -s nullglob

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"

install_bundle() {
    local src="$1" destdir="$2"
    mkdir -p "$destdir"
    rm -rf "$destdir/$src"          # replaces only this exact plug-in
    ditto "$src" "$destdir/$src"
    # Downloads are quarantined by macOS; this ad-hoc signed dev build would be blocked otherwise.
    xattr -dr com.apple.quarantine "$destdir/$src" 2>/dev/null || true
    codesign --force --deep --sign - "$destdir/$src" >/dev/null 2>&1 || true
    echo "Installed: $destdir/$src"
}

found=0
for b in *.vst3;      do install_bundle "$b" "$VST3_DIR"; found=1; done
for b in *.component; do install_bundle "$b" "$AU_DIR";   found=1; done
[ "$found" = 1 ] || { echo "No plug-ins found next to install.sh"; exit 1; }

# Refresh the Audio Unit cache.
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

echo
echo "Done. In Ableton Live: Settings > Plug-Ins > enable 'Use VST3 Plug-in System Folders'"
echo "(and 'Use Audio Units v2' for the AU), then click Rescan."
