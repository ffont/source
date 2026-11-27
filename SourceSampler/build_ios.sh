#!/usr/bin/env bash
#
# Generates the Xcode project for the iPad version of SourceSampler (standalone app + AUv3) and optionally builds it.
#
# Usage:
#   ./build_ios.sh                        Generate the Xcode project (build-ios/SourceSampler.xcodeproj). Open it in
#                                         Xcode, select the "SourceSampler_Standalone" scheme and your iPad, and run.
#   ./build_ios.sh --simulator            Also build the app for the iOS simulator (no signing needed)
#   TEAM_ID=ABCDE12345 ./build_ios.sh --device
#                                         Also build a signed Release app for devices (needs an Apple development team)
#
# Environment variables:
#   TEAM_ID     Apple development team ID (10 characters), needed to run on a real iPad
#   APP_GROUP   iOS app group shared by the app and the AUv3 (default: group.ritaiaurora.source). It must be registered
#               in your team, otherwise use APP_GROUP="" to disable it (the app then uses its own Documents folder).
#   CMAKE_ARGS  Extra CMake arguments (e.g. "-DSOURCE_FREESOUND_API_KEY=xxxx")

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$SCRIPT_DIR/build-ios}"
MODE="generate"

for arg in "$@"; do
    case "$arg" in
        --simulator) MODE="simulator" ;;
        --device)    MODE="device" ;;
        -h|--help)   sed -n '2,19p' "$0"; exit 0 ;;
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done

if [[ ! -f "$SCRIPT_DIR/3rdParty/JUCE/CMakeLists.txt" ]]; then
    echo "==> Initialising git submodules"
    git -C "$SCRIPT_DIR/.." submodule update --init --recursive
fi

CONFIGURE_ARGS=(-S "$SCRIPT_DIR" -B "$BUILD_DIR" -G Xcode -DCMAKE_SYSTEM_NAME=iOS)
if [[ -n "${TEAM_ID:-}" ]]; then
    CONFIGURE_ARGS+=(-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM="$TEAM_ID" -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY="Apple Development")
fi
if [[ -n "${APP_GROUP+x}" ]]; then
    CONFIGURE_ARGS+=(-DSOURCE_IOS_APP_GROUP="$APP_GROUP")
fi

echo "==> Generating Xcode project in $BUILD_DIR"
# shellcheck disable=SC2086
cmake "${CONFIGURE_ARGS[@]}" ${CMAKE_ARGS:-}

case "$MODE" in
    generate)
        echo "==> Done. Open $BUILD_DIR/SourceSampler.xcodeproj in Xcode and run the SourceSampler_Standalone scheme."
        ;;
    simulator)
        echo "==> Building for the iOS simulator"
        cmake --build "$BUILD_DIR" --config Debug --target SourceSampler_Standalone -- \
            -sdk iphonesimulator -destination 'generic/platform=iOS Simulator' CODE_SIGNING_ALLOWED=NO
        echo "==> Built: $BUILD_DIR/SourceSampler_artefacts/Debug/Standalone/SourceSampler.app"
        echo "    Install it with: xcrun simctl install booted \"$BUILD_DIR/SourceSampler_artefacts/Debug/Standalone/SourceSampler.app\""
        ;;
    device)
        if [[ -z "${TEAM_ID:-}" ]]; then
            echo "TEAM_ID is required to build for devices" >&2
            exit 1
        fi
        echo "==> Building for devices (Release)"
        cmake --build "$BUILD_DIR" --config Release --target SourceSampler_Standalone -- \
            -sdk iphoneos -allowProvisioningUpdates
        echo "==> Built: $BUILD_DIR/SourceSampler_artefacts/Release/Standalone/SourceSampler.app"
        ;;
esac
