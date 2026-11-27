#!/usr/bin/env bash
#
# Builds the release version of SourceSampler (native JUCE UI) and installs the
# AU and VST3 plugins and the standalone app (macOS).
#
# Usage:
#   ./build_and_install.sh [--no-tests] [--no-install] [--clean]
#
# Install locations can be changed with environment variables:
#   AU_DIR          (default: ~/Library/Audio/Plug-Ins/Components)
#   VST3_DIR        (default: ~/Library/Audio/Plug-Ins/VST3)
#   STANDALONE_DIR  (default: /Applications if writable, otherwise ~/Applications)
#
# Extra CMake arguments can be passed with CMAKE_ARGS, e.g.:
#   CMAKE_ARGS="-DSOURCE_FREESOUND_API_KEY=xxxx" ./build_and_install.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$SCRIPT_DIR/build-release}"
CONFIG="Release"
RUN_TESTS=1
INSTALL=1
CLEAN=0

for arg in "$@"; do
    case "$arg" in
        --no-tests)   RUN_TESTS=0 ;;
        --no-install) INSTALL=0 ;;
        --clean)      CLEAN=1 ;;
        -h|--help)    sed -n '2,17p' "$0"; exit 0 ;;
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done

if [[ "$(uname)" != "Darwin" ]]; then
    echo "This script installs macOS plugins (AU/VST3/app). On other platforms build with CMake directly." >&2
    exit 1
fi

AU_DIR="${AU_DIR:-$HOME/Library/Audio/Plug-Ins/Components}"
VST3_DIR="${VST3_DIR:-$HOME/Library/Audio/Plug-Ins/VST3}"
if [[ -z "${STANDALONE_DIR:-}" ]]; then
    if [[ -w /Applications ]]; then STANDALONE_DIR="/Applications"; else STANDALONE_DIR="$HOME/Applications"; fi
fi

if [[ ! -f "$SCRIPT_DIR/3rdParty/JUCE/CMakeLists.txt" ]]; then
    echo "==> Initialising git submodules"
    git -C "$SCRIPT_DIR/.." submodule update --init --recursive
fi

if [[ ! -f "$SCRIPT_DIR/Source/api_key.h" && "${CMAKE_ARGS:-}" != *SOURCE_FREESOUND_API_KEY* ]]; then
    echo "WARNING: no Freesound API key configured (Source/api_key.h or -DSOURCE_FREESOUND_API_KEY). Searching Freesound will not work." >&2
fi

[[ $CLEAN -eq 1 ]] && rm -rf "$BUILD_DIR"

GENERATOR_ARGS=()
command -v ninja >/dev/null 2>&1 && GENERATOR_ARGS=(-G Ninja)

echo "==> Configuring ($CONFIG) in $BUILD_DIR"
# shellcheck disable=SC2086
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" "${GENERATOR_ARGS[@]}" \
    -DCMAKE_BUILD_TYPE="$CONFIG" -DSOURCE_WEB_UI=OFF -DSOURCE_BUILD_TESTS=ON ${CMAKE_ARGS:-}

echo "==> Building"
cmake --build "$BUILD_DIR" --config "$CONFIG" --target SourceSampler_AU SourceSampler_VST3 SourceSampler_Standalone SourceSamplerTests

if [[ $RUN_TESTS -eq 1 ]]; then
    echo "==> Running tests"
    (cd "$BUILD_DIR" && ctest -C "$CONFIG" --output-on-failure)
fi

ARTEFACTS="$BUILD_DIR/SourceSampler_artefacts/$CONFIG"
AU_BUNDLE=$(find "$ARTEFACTS/AU" -maxdepth 1 -name "*.component" | head -n 1)
VST3_BUNDLE=$(find "$ARTEFACTS/VST3" -maxdepth 1 -name "*.vst3" | head -n 1)
APP_BUNDLE=$(find "$ARTEFACTS/Standalone" -maxdepth 1 -name "*.app" | head -n 1)

if [[ $INSTALL -eq 0 ]]; then
    echo "==> Built (not installed):"
    printf '    %s\n' "$AU_BUNDLE" "$VST3_BUNDLE" "$APP_BUNDLE"
    exit 0
fi

install_bundle() {
    local bundle="$1" destination_dir="$2"
    local destination="$destination_dir/$(basename "$bundle")"
    mkdir -p "$destination_dir"
    rm -rf "$destination"
    cp -R "$bundle" "$destination"
    # Ad-hoc sign so macOS/hosts accept locally built bundles
    codesign --force --deep --sign - "$destination" >/dev/null 2>&1 || echo "    (warning: could not codesign $destination)"
    echo "    $destination"
}

echo "==> Installing"
install_bundle "$AU_BUNDLE" "$AU_DIR"
install_bundle "$VST3_BUNDLE" "$VST3_DIR"
install_bundle "$APP_BUNDLE" "$STANDALONE_DIR"

# Make hosts pick up the new Audio Unit
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

echo "==> Done. Restart your DAW (or rescan plugins) to load the new version."
