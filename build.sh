#!/usr/bin/env bash
# BLEEP / 001 — one-command macOS build.
# Checks the toolchain, fetches JUCE, builds + tests + installs VST3/AU,
# signs the bundles for Apple Silicon, and validates the AU.
# Everything is logged to build.log next to this script.
#
# Usage:  ./build.sh            (Release, native architecture)
#         UNIVERSAL=1 ./build.sh (arm64 + x86_64, e.g. for Ableton under Rosetta)
#         CLEAN=1 ./build.sh     (delete the build folder first)

set -euo pipefail
cd "$(dirname "$0")"
ROOT="$PWD"
LOG="$ROOT/build.log"
exec > >(tee "$LOG") 2>&1

JUCE_TAG="${JUCE_TAG:-9.0.5}"
BUILD_DIR="$ROOT/build"
PLUGIN_NAME="BLEEP 001"

step() { printf '\n== %s ==\n' "$1"; }
fail() { printf '\nERROR: %s\n' "$1"; exit 1; }

step "Environment"
[[ "$(uname -s)" == "Darwin" ]] || fail "This script is for macOS."
echo "macOS $(sw_vers -productVersion) ($(sw_vers -buildVersion)), CPU: $(uname -m), cores: $(sysctl -n hw.ncpu)"
FREE_GB=$(df -g "$ROOT" | awk 'NR==2 {print $4}')
echo "Free disk space: ${FREE_GB} GB"
if (( FREE_GB < 3 )); then
    fail "Less than 3 GB free. JUCE + the build need about 2 GB. Free some space and retry."
fi

step "Xcode Command Line Tools"
if ! xcode-select -p >/dev/null 2>&1; then
    echo "Not installed. Run this, accept the dialog, then re-run ./build.sh:"
    echo "    xcode-select --install"
    exit 1
fi
echo "Developer dir: $(xcode-select -p)"
clang++ --version | head -1
xcrun --show-sdk-version >/dev/null 2>&1 && echo "macOS SDK: $(xcrun --show-sdk-version)"

step "CMake"
if ! command -v cmake >/dev/null 2>&1; then
    if command -v brew >/dev/null 2>&1; then
        echo "CMake missing. Install it with:  brew install cmake"
    else
        echo "CMake missing. Install Homebrew (https://brew.sh) then: brew install cmake"
        echo "or download the macOS installer from https://cmake.org/download/ and run:"
        echo "    sudo \"/Applications/CMake.app/Contents/bin/cmake-gui\" --install"
    fi
    exit 1
fi
CMAKE_VER=$(cmake --version | head -1 | awk '{print $3}')
echo "cmake $CMAKE_VER"
if [[ "$(printf '%s\n3.22\n' "$CMAKE_VER" | sort -t. -k1,1n -k2,2n | head -1)" != "3.22" ]]; then
    fail "CMake 3.22 or newer required (found $CMAKE_VER). Run: brew upgrade cmake"
fi

step "JUCE $JUCE_TAG"
if [[ -f "$ROOT/external/JUCE/CMakeLists.txt" ]]; then
    echo "Found external/JUCE"
else
    command -v git >/dev/null 2>&1 || fail "git not found (it comes with the Command Line Tools)."
    mkdir -p "$ROOT/external"
    git clone --depth 1 --branch "$JUCE_TAG" https://github.com/juce-framework/JUCE.git "$ROOT/external/JUCE"
fi

if [[ "${CLEAN:-0}" == "1" ]]; then
    step "Clean"
    rm -rf "$BUILD_DIR"
fi

ARCHS="$(uname -m)"
[[ "${UNIVERSAL:-0}" == "1" ]] && ARCHS="arm64;x86_64"

step "Configure ($ARCHS)"
cmake -S "$ROOT" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="$ARCHS"

step "Build"
cmake --build "$BUILD_DIR" --config Release -j "$(sysctl -n hw.ncpu)"

step "DSP tests"
ctest --test-dir "$BUILD_DIR" --output-on-failure

step "Install + sign"
VST3="$HOME/Library/Audio/Plug-Ins/VST3/$PLUGIN_NAME.vst3"
AU="$HOME/Library/Audio/Plug-Ins/Components/$PLUGIN_NAME.component"
for bundle in "$VST3" "$AU"; do
    if [[ -d "$bundle" ]]; then
        codesign --force --deep --sign - "$bundle"
        echo "OK  $bundle"
    else
        echo "MISSING  $bundle"
    fi
done

# Make macOS notice the new/updated Audio Unit.
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

step "AU validation"
if auval -v aumu Bl01 Blpa > "$BUILD_DIR/auval.log" 2>&1; then
    echo "auval: PASSED"
else
    echo "auval: FAILED (see build/auval.log). VST3 is unaffected."
fi
tail -3 "$BUILD_DIR/auval.log" || true

STANDALONE=$(find "$BUILD_DIR" -name "$PLUGIN_NAME.app" -maxdepth 4 | head -1)
step "Done"
echo "VST3:       $VST3"
echo "AU:         $AU"
[[ -n "$STANDALONE" ]] && echo "Standalone: $STANDALONE"
echo "Now rescan plug-ins in Ableton (Settings > Plug-Ins)."
