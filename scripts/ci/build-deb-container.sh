#!/usr/bin/env bash
#
# ComponentHub - build, test and package a .deb INSIDE a Debian Trixie container.
#
# Used by .github/workflows/packages.yml (amd64 and arm64, each on a runner of its own architecture).
# Trixie is the target of the Raspberry Pi packages: same glibc and same Qt 6.8 as the Pi,
# so the Depends detected by package-deb.sh match what the Pi installs.
#
# Usage (as root, in the container, from the repository root):
#   scripts/ci/build-deb-container.sh <linux|linux-arm64>
# The argument is the CMake preset: it picks the NATIVE build of the container's
# architecture (no cross-compilation, no sysroot to maintain).
set -euo pipefail

PRESET="${1:?usage: build-deb-container.sh <linux|linux-arm64>}"
case "$PRESET" in
    linux)       BUILD_DIR=build ;;
    linux-arm64) BUILD_DIR=build-arm64 ;;
    *) echo "unknown preset: $PRESET" >&2; exit 1 ;;
esac

export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq --no-install-recommends \
    build-essential cmake ninja-build pkg-config python3 file ca-certificates \
    qt6-base-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools \
    libsqlite3-dev nlohmann-json3-dev libgl1-mesa-dev libxkbcommon-dev \
    dpkg-dev imagemagick

cmake --preset "$PRESET"
cmake --build --preset "$PRESET" --parallel "$(nproc)"
QT_QPA_PLATFORM=offscreen ctest --test-dir "$BUILD_DIR" --output-on-failure
scripts/linux/package-deb.sh --build "$BUILD_DIR"
ls -l dist/*.deb
