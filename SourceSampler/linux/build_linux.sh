#!/usr/bin/env bash
#
# Builds the Linux version of SourceSampler (standalone app, VST3 and LV2) in Release mode, runs the tests and
# packages each format in a zip file in dist/. Used by the GitHub Actions release workflow and by build_in_docker.sh.
# Needs the packages listed in linux/dependencies.txt (see README in this folder).
#
# Environment variables: BUILD_DIR (default build-linux-<arch>), CMAKE_ARGS (e.g. -DSOURCE_FREESOUND_API_KEY=...),
# RUN_TESTS (default 1).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ARCH="$(uname -m)"
[[ "$ARCH" == "aarch64" ]] && ARCH="arm64"
BUILD_DIR="${BUILD_DIR:-$PROJECT_DIR/build-linux-$ARCH}"
DIST_DIR="${DIST_DIR:-$PROJECT_DIR/dist}"
VERSION=$(sed -n 's/^project(SourceSampler VERSION \([0-9.]*\).*/\1/p' "$PROJECT_DIR/CMakeLists.txt")

echo "==> Configuring SOURCE $VERSION ($ARCH)"
# shellcheck disable=SC2086
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DSOURCE_WEB_UI=OFF -DSOURCE_BUILD_TESTS=ON ${CMAKE_ARGS:-}

echo "==> Building"
cmake --build "$BUILD_DIR" --target SourceSampler_Standalone SourceSampler_VST3 SourceSampler_LV2 SourceSamplerTests

if [[ "${RUN_TESTS:-1}" == "1" ]]; then
    echo "==> Running tests"
    # The tests create UI components, which need a display on Linux
    if command -v xvfb-run >/dev/null 2>&1 && [[ -z "${DISPLAY:-}" ]]; then
        xvfb-run -a "$BUILD_DIR/SourceSamplerTests_artefacts/Release/SourceSamplerTests"
    else
        "$BUILD_DIR/SourceSamplerTests_artefacts/Release/SourceSamplerTests"
    fi
fi

echo "==> Packaging"
ARTEFACTS="$BUILD_DIR/SourceSampler_artefacts/Release"
mkdir -p "$DIST_DIR"
package() {
    local source="$1" name="$2"
    local zip="$DIST_DIR/SOURCE-$VERSION-linux-$ARCH-$name.zip"
    rm -f "$zip"
    (cd "$(dirname "$source")" && zip -qry "$zip" "$(basename "$source")")
    echo "    $(basename "$zip")"
}
package "$ARTEFACTS/Standalone/SourceSampler" "Standalone"
package "$ARTEFACTS/VST3/SourceSampler.vst3" "VST3"
package "$ARTEFACTS/LV2/SourceSampler.lv2" "LV2"
(cd "$DIST_DIR" && sha256sum SOURCE-$VERSION-linux-$ARCH-*.zip > "SOURCE-$VERSION-linux-$ARCH-SHA256.txt")
