#!/bin/bash
# Builds, tests, signs and zips one plugin variant on a macOS runner.
# Usage: build_variant.sh <src_dir> <build_dir> <plugin_name> <artifact_name> <au_code> "<test targets>"
# Writes the ZIP path to <build_dir>/zip_path.txt and the AU build result to <build_dir>/au_ok.
set -euo pipefail

SRC_DIR="$1"; BUILD_DIR="$2"; PLUGIN_NAME="$3"; ARTIFACT="$4"; AU_CODE="$5"; TESTS="$6"
JOBS="$(sysctl -n hw.ncpu)"
ART="$BUILD_DIR/BLEEP001_artefacts/Release"

echo "::group::Configure $PLUGIN_NAME"
cmake -S "$SRC_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DJUCE_DIR="$GITHUB_WORKSPACE/external/JUCE" \
    -DBLEEP_BUILD_PLUGIN_TESTS=ON
echo "::endgroup::"

echo "::group::Build VST3 ($PLUGIN_NAME)"
cmake --build "$BUILD_DIR" --config Release --target BLEEP001_VST3 -j "$JOBS"
echo "::endgroup::"

echo "::group::Build AU ($PLUGIN_NAME)"
rm -f "$BUILD_DIR/au_ok"
if cmake --build "$BUILD_DIR" --config Release --target BLEEP001_AU -j "$JOBS"; then
    touch "$BUILD_DIR/au_ok"
else
    echo "::warning::AU build failed for $PLUGIN_NAME (VST3 unaffected)"
fi
echo "::endgroup::"

echo "::group::Tests ($PLUGIN_NAME)"
# shellcheck disable=SC2086
cmake --build "$BUILD_DIR" --config Release --target $TESTS -j "$JOBS"
ctest --test-dir "$BUILD_DIR" --output-on-failure
echo "::endgroup::"

echo "::group::Sign ($PLUGIN_NAME)"
for bundle in "$ART/VST3/$PLUGIN_NAME.vst3" "$ART/AU/$PLUGIN_NAME.component"; do
    [ -d "$bundle" ] || { echo "skip (not built): $bundle"; continue; }
    codesign --force --deep --sign - --timestamp=none "$bundle"
    codesign --verify --deep --strict --verbose=2 "$bundle"
    lipo -info "$bundle/Contents/MacOS/$PLUGIN_NAME"
    echo "CFBundleIdentifier: $(plutil -extract CFBundleIdentifier raw "$bundle/Contents/Info.plist")"
done
echo "::endgroup::"

echo "::group::Package ($PLUGIN_NAME)"
STAGE="$GITHUB_WORKSPACE/dist/$ARTIFACT"
rm -rf "$STAGE"; mkdir -p "$STAGE"
ditto "$ART/VST3/$PLUGIN_NAME.vst3" "$STAGE/$PLUGIN_NAME.vst3"
[ -d "$ART/AU/$PLUGIN_NAME.component" ] && ditto "$ART/AU/$PLUGIN_NAME.component" "$STAGE/$PLUGIN_NAME.component"
cp "$GITHUB_WORKSPACE/packaging/install.sh" "$GITHUB_WORKSPACE/packaging/INSTALL.txt" "$STAGE/"
chmod +x "$STAGE/install.sh"
echo "$PLUGIN_NAME - commit $GITHUB_SHA, JUCE $JUCE_TAG, AU aumu $AU_CODE Blpa, built $(date -u +%Y-%m-%dT%H:%MZ)" > "$STAGE/BUILD_INFO.txt"
ZIP="$GITHUB_WORKSPACE/$ARTIFACT-${GITHUB_SHA::7}.zip"
(cd "$GITHUB_WORKSPACE/dist" && ditto -c -k --sequesterRsrc --keepParent "$ARTIFACT" "$ZIP")
unzip -l "$ZIP"
echo "$ZIP" > "$BUILD_DIR/zip_path.txt"
echo "::endgroup::"
