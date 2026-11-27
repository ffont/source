#!/usr/bin/env bash
#
# Generates the Xcode project for the iPad version of SourceSampler (standalone app + AUv3) and optionally builds it.
#
# Usage:
#   ./build_ios.sh                        Generate the Xcode project (build-ios/SourceSampler.xcodeproj). Open it in
#                                         Xcode, select the "SourceSampler_Standalone" scheme and your iPad, and run.
#   ./build_ios.sh --simulator [--install]
#                                         Also build the app for the iOS simulator (no signing needed) and optionally
#                                         install it in the booted simulator
#   TEAM_ID=ABCDE12345 ./build_ios.sh --device [--install]
#                                         Also build a signed Release app for devices (needs an Apple development team)
#                                         and optionally install it on the connected iPad (or DEVICE_ID)
#
# Environment variables:
#   TEAM_ID     Apple development team ID (10 characters), needed to run on a real iPad
#   APP_GROUP   iOS app group shared by the app and the AUv3 (default: group.ffont.source). It must be registered
#               in your team, otherwise use APP_GROUP="" to disable it (the app then uses its own Documents folder).
#   DEVICE_ID   Device to install on (UDID or name, see "xcrun devicectl list devices"). Default: the first connected
#               iPad.
#   CMAKE_ARGS  Extra CMake arguments (e.g. "-DSOURCE_FREESOUND_API_KEY=xxxx")

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$SCRIPT_DIR/build-ios}"
MODE="generate"
INSTALL=0

for arg in "$@"; do
    case "$arg" in
        --simulator) MODE="simulator" ;;
        --device)    MODE="device" ;;
        --install)   INSTALL=1 ;;
        -h|--help)   sed -n '2,21p' "$0"; exit 0 ;;
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done

if [[ $INSTALL -eq 1 && "$MODE" == "generate" ]]; then
    echo "--install needs --simulator or --device" >&2
    exit 1
fi
if [[ "$MODE" == "device" && -z "${TEAM_ID:-}" ]]; then
    echo "TEAM_ID is required to build for devices" >&2
    exit 1
fi

if [[ ! -f "$SCRIPT_DIR/3rdParty/JUCE/CMakeLists.txt" ]]; then
    echo "==> Initialising git submodules"
    git -C "$SCRIPT_DIR/.." submodule update --init --recursive
fi

# The project is always generated for the device SDK; simulator builds select the simulator SDK at build time
CONFIGURE_ARGS=(-S "$SCRIPT_DIR" -B "$BUILD_DIR" -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos)
if [[ -n "${TEAM_ID:-}" ]]; then
    CONFIGURE_ARGS+=(-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM="$TEAM_ID"
                     -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE=Automatic
                     -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY="Apple Development")
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
            -sdk iphonesimulator CODE_SIGNING_ALLOWED=NO
        APP="$BUILD_DIR/SourceSampler_artefacts/Debug/Standalone/SourceSampler.app"
        echo "==> Built: $APP"
        if [[ $INSTALL -eq 1 ]]; then
            echo "==> Installing in the booted simulator"
            xcrun simctl install booted "$APP"
        fi
        ;;
    device)
        echo "==> Building for devices (Release)"
        cmake --build "$BUILD_DIR" --config Release --target SourceSampler_Standalone -- \
            -sdk iphoneos -allowProvisioningUpdates -allowProvisioningDeviceRegistration
        APP="$BUILD_DIR/SourceSampler_artefacts/Release/Standalone/SourceSampler.app"
        echo "==> Built: $APP"
        if [[ $INSTALL -eq 1 ]]; then
            if [[ -z "${DEVICE_ID:-}" ]]; then
                DEVICE_ID=$(xcrun devicectl list devices 2>/dev/null | awk '/connected/ && /physical/ && /iPad/ { for (i = 1; i <= NF; i++) if ($i ~ /^[0-9A-F]{8}-[0-9A-F]+$/) { print $i; exit } }')
            fi
            if [[ -z "${DEVICE_ID:-}" ]]; then
                echo "No connected iPad found (set DEVICE_ID)" >&2
                exit 1
            fi
            echo "==> Installing on device $DEVICE_ID"
            xcrun devicectl device install app --device "$DEVICE_ID" "$APP"
        fi
        ;;
esac
