#!/bin/bash
# Installs BLEEP 001 into your user plug-in folders. No Xcode needed.
# Run from Terminal:  bash install.sh
set -euo pipefail
cd "$(dirname "$0")"

NAME="BLEEP 001"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"

install_bundle() {
    local src="$1" destdir="$2"
    [ -d "$src" ] || { echo "Not in this package: $src"; return; }
    mkdir -p "$destdir"
    rm -rf "$destdir/$src"
    ditto "$src" "$destdir/$src"
    # Downloads are quarantined by macOS; this ad-hoc signed dev build would be blocked otherwise.
    xattr -dr com.apple.quarantine "$destdir/$src" 2>/dev/null || true
    codesign --force --deep --sign - "$destdir/$src" >/dev/null 2>&1 || true
    echo "Installed: $destdir/$src"
}

install_bundle "$NAME.vst3" "$VST3_DIR"
install_bundle "$NAME.component" "$AU_DIR"

# Refresh the Audio Unit cache.
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

echo
echo "Done. In Ableton Live: Settings > Plug-Ins > enable 'Use VST3 Plug-in System Folders'"
echo "(and 'Use Audio Units v2' for the AU), then click Rescan."
