#!/usr/bin/env bash
#
# Builds the Windows version of SourceSampler (standalone app and VST3, x64) in Release mode with Visual Studio, runs
# the tests and packages each format in a zip file in dist/. Runs in Git Bash (used by the GitHub Actions release
# workflow). Needs Visual Studio 2022 (C++ workload), CMake and 7-Zip.
#
# Environment variables: BUILD_DIR (default build-windows), CMAKE_ARGS (e.g. -DSOURCE_FREESOUND_API_KEY=...),
# RUN_TESTS (default 1).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$PROJECT_DIR/build-windows}"
DIST_DIR="${DIST_DIR:-$PROJECT_DIR/dist}"
VERSION=$(sed -n 's/^project(SourceSampler VERSION \([0-9.]*\).*/\1/p' "$PROJECT_DIR/CMakeLists.txt")
ARCH="x64"

echo "==> Configuring SOURCE $VERSION (Windows $ARCH)"
# shellcheck disable=SC2086
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -G "Visual Studio 17 2022" -A x64 -DSOURCE_WEB_UI=OFF -DSOURCE_BUILD_TESTS=ON ${CMAKE_ARGS:-}

echo "==> Building"
cmake --build "$BUILD_DIR" --config Release --parallel --target SourceSampler_Standalone SourceSampler_VST3 SourceSamplerTests

if [[ "${RUN_TESTS:-1}" == "1" ]]; then
    echo "==> Running tests"
    "$BUILD_DIR/SourceSamplerTests_artefacts/Release/SourceSamplerTests.exe"
fi

echo "==> Packaging"
ARTEFACTS="$BUILD_DIR/SourceSampler_artefacts/Release"
mkdir -p "$DIST_DIR"
package() {
    local source="$1" name="$2"
    local zip="$DIST_DIR/SOURCE-$VERSION-windows-$ARCH-$name.zip"
    rm -f "$zip"
    (cd "$(dirname "$source")" && 7z a -tzip -bso0 -bsp0 "$zip" "$(basename "$source")")
    echo "    $(basename "$zip")"
}
package "$ARTEFACTS/Standalone/SourceSampler.exe" "Standalone"
package "$ARTEFACTS/VST3/SourceSampler.vst3" "VST3"
(cd "$DIST_DIR" && sha256sum SOURCE-$VERSION-windows-$ARCH-*.zip > "SOURCE-$VERSION-windows-$ARCH-SHA256.txt")
