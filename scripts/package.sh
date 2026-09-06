#!/usr/bin/env bash
# package.sh — Build and package LDDE as a Debian .deb
#
# Usage:
#   ./scripts/package.sh [--arch arm64|amd64] [--prefix /usr] [--output-dir ./dist]
#
# This script uses CMake + CPack to produce:
#   dist/linuxdroid-desktop-environment_<version>_<arch>.deb
#
# The resulting .deb can be installed with:
#   sudo dpkg -i dist/linuxdroid-desktop-environment_1.0.0_arm64.deb

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Defaults
ARCH="$(dpkg --print-architecture 2>/dev/null || echo arm64)"
PREFIX="/usr"
OUTPUT_DIR="${ROOT_DIR}/dist"
BUILD_DIR="${ROOT_DIR}/build-release"
BUILD_TYPE="Release"

usage() {
    echo "Usage: $0 [--arch arm64|amd64] [--prefix /usr] [--output-dir ./dist]"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --arch)    ARCH="$2";       shift 2 ;;
        --prefix)  PREFIX="$2";     shift 2 ;;
        --output-dir) OUTPUT_DIR="$2"; shift 2 ;;
        --help|-h) usage ;;
        *) echo "Unknown argument: $1" >&2; usage ;;
    esac
done

echo "========================================================"
echo "  LDDE Release Packaging"
echo "========================================================"
echo "  Architecture : ${ARCH}"
echo "  Prefix       : ${PREFIX}"
echo "  Build dir    : ${BUILD_DIR}"
echo "  Output dir   : ${OUTPUT_DIR}"
echo "========================================================"

mkdir -p "${OUTPUT_DIR}"

# --- Configure ---
echo ""
echo "[1/4] Configuring..."
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
    -DCMAKE_INSTALL_SYSCONFDIR=/etc \
    -DBUILD_TESTING=OFF

# --- Build ---
echo ""
echo "[2/4] Building..."
cmake --build "${BUILD_DIR}" --parallel "$(nproc)"

# --- Stage install (for validation) ---
STAGING_DIR="${BUILD_DIR}/staging"
rm -rf "${STAGING_DIR}"
echo ""
echo "[3/4] Staging install to ${STAGING_DIR}..."
DESTDIR="${STAGING_DIR}" cmake --install "${BUILD_DIR}"

# --- CPack DEB ---
echo ""
echo "[4/4] Generating .deb package..."
cd "${BUILD_DIR}"
cpack -G DEB --config CPackConfig.cmake

# Move .deb to output dir
DEBS=("${BUILD_DIR}"/*.deb)
if [[ ${#DEBS[@]} -eq 0 || ! -f "${DEBS[0]}" ]]; then
    echo "ERROR: No .deb produced by cpack" >&2
    exit 1
fi

for deb in "${DEBS[@]}"; do
    dest="${OUTPUT_DIR}/$(basename "${deb}")"
    cp "${deb}" "${dest}"
    echo ""
    echo "Package produced: ${dest}"
    echo "Package contents:"
    dpkg-deb --contents "${dest}" 2>/dev/null | head -60 || true
    echo ""
    echo "Package info:"
    dpkg-deb --info "${dest}" 2>/dev/null || true
done

echo ""
echo "========================================================"
echo "  Packaging complete."
echo "========================================================"
echo ""
echo "To install:"
echo "  sudo dpkg -i ${OUTPUT_DIR}/*.deb"
echo ""
echo "To validate without installing:"
echo "  ./scripts/validate-package.sh ${OUTPUT_DIR}/*.deb"
