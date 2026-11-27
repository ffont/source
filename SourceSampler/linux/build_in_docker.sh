#!/usr/bin/env bash
#
# Builds the Linux version in an Ubuntu 22.04 container (Docker, OrbStack...), for the architecture of the container
# (PLATFORM=linux/amd64 or linux/arm64, default: the machine's). Results are left in dist/.

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
PLATFORM_ARGS=()
[[ -n "${PLATFORM:-}" ]] && PLATFORM_ARGS=(--platform "$PLATFORM")

docker run --rm ${PLATFORM_ARGS[@]+"${PLATFORM_ARGS[@]}"} -v "$REPO_DIR":/source -w /source/SourceSampler -e CMAKE_ARGS="${CMAKE_ARGS:-}" \
    -e CC=gcc-12 -e CXX=g++-12 ubuntu:22.04 bash -c "
        export DEBIAN_FRONTEND=noninteractive
        apt-get update -qq && apt-get install -y -qq \$(cat linux/dependencies.txt) > /dev/null
        git config --global --add safe.directory '*' 2>/dev/null || true
        ./linux/build_linux.sh"
