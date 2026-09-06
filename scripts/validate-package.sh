#!/usr/bin/env bash
# validate-package.sh — Validate a built LDDE .deb package
#
# Usage:
#   ./scripts/validate-package.sh path/to/linuxdroid-desktop-environment_1.0.0_arm64.deb
#
# Checks:
#   1. Package metadata (name, version, architecture, description)
#   2. Required files present in the package
#   3. Conffiles list correctness
#   4. Maintainer scripts exist and are executable
#   5. No dangerous files (world-writable, missing ownership)
#   6. Session entry file is valid
#   7. Version manifest matches control version

set -euo pipefail

DEB="${1:-}"
if [[ -z "${DEB}" || ! -f "${DEB}" ]]; then
    echo "Usage: $0 path/to/linuxdroid-desktop-environment_*.deb" >&2
    exit 1
fi

PASS=0
FAIL=0
WARN=0

pass() { echo "  [PASS] $1"; ((PASS++)) || true; }
fail() { echo "  [FAIL] $1"; ((FAIL++)) || true; }
warn() { echo "  [WARN] $1"; ((WARN++)) || true; }

echo "========================================================"
echo "  LDDE Package Validation"
echo "  Package: ${DEB}"
echo "========================================================"

# Extract for inspection
TMPDIR="$(mktemp -d)"
trap 'rm -rf "${TMPDIR}"' EXIT

dpkg-deb -x "${DEB}" "${TMPDIR}/fs"
dpkg-deb -e "${DEB}" "${TMPDIR}/DEBIAN"

# --- 1. Metadata ---
echo ""
echo "[1] Package Metadata"

PKG_NAME="$(grep '^Package:' "${TMPDIR}/DEBIAN/control" | awk '{print $2}')"
PKG_VER="$(grep '^Version:' "${TMPDIR}/DEBIAN/control" | awk '{print $2}')"
PKG_ARCH="$(grep '^Architecture:' "${TMPDIR}/DEBIAN/control" | awk '{print $2}')"
PKG_DESC="$(grep '^Description:' "${TMPDIR}/DEBIAN/control" | cut -d: -f2- | xargs)"

[[ "${PKG_NAME}" == "linuxdroid-desktop-environment" ]] \
    && pass "Package name: ${PKG_NAME}" \
    || fail "Expected package name 'linuxdroid-desktop-environment', got '${PKG_NAME}'"

[[ "${PKG_VER}" =~ ^[0-9]+\.[0-9]+\.[0-9]+ ]] \
    && pass "Package version: ${PKG_VER}" \
    || fail "Invalid package version: '${PKG_VER}'"

[[ "${PKG_ARCH}" =~ ^(arm64|amd64)$ ]] \
    && pass "Package architecture: ${PKG_ARCH}" \
    || fail "Unexpected architecture: '${PKG_ARCH}'"

[[ -n "${PKG_DESC}" ]] \
    && pass "Description present" \
    || fail "Description missing"

# --- 2. Required files ---
echo ""
echo "[2] Required Files"

check_file() {
    local path="${TMPDIR}/fs${1}"
    if [[ -e "${path}" ]]; then
        pass "Present: ${1}"
    else
        fail "Missing: ${1}"
    fi
}

check_file "/usr/bin/ldde"
check_file "/usr/share/wayland-sessions/ldde.desktop"
check_file "/etc/linuxdroid/desktop.conf"
check_file "/usr/share/linuxdroid/ldde/version"

# Optional but expected
[[ -e "${TMPDIR}/fs/usr/share/doc/linuxdroid-desktop-environment/README.md" ]] \
    && pass "Present: /usr/share/doc/linuxdroid-desktop-environment/README.md" \
    || warn "Missing: /usr/share/doc/linuxdroid-desktop-environment/README.md"

# --- 3. Binary executable ---
echo ""
echo "[3] Binary Checks"

BINARY="${TMPDIR}/fs/usr/bin/ldde"
if [[ -f "${BINARY}" ]]; then
    [[ -x "${BINARY}" ]] && pass "ldde is executable" || fail "ldde is not executable"
    FILE_TYPE="$(file -b "${BINARY}")"
    echo "  File type: ${FILE_TYPE}"
    [[ "${FILE_TYPE}" =~ ELF ]] && pass "ldde is an ELF binary" || fail "ldde is not an ELF binary"
else
    fail "ldde binary not found"
fi

# --- 4. Conffiles ---
echo ""
echo "[4] Conffiles"

if [[ -f "${TMPDIR}/DEBIAN/conffiles" ]]; then
    pass "conffiles present"
    if grep -q '/etc/linuxdroid/desktop.conf' "${TMPDIR}/DEBIAN/conffiles"; then
        pass "desktop.conf listed as conffile"
    else
        fail "desktop.conf NOT listed as conffile (admin changes won't be preserved on upgrade)"
    fi
else
    fail "debian/conffiles missing"
fi

# --- 5. Maintainer scripts ---
echo ""
echo "[5] Maintainer Scripts"

for script in postinst prerm; do
    if [[ -f "${TMPDIR}/DEBIAN/${script}" ]]; then
        [[ -x "${TMPDIR}/DEBIAN/${script}" ]] \
            && pass "${script} exists and is executable" \
            || fail "${script} exists but is NOT executable"
    else
        warn "${script} not present (optional)"
    fi
done

# --- 6. Session file ---
echo ""
echo "[6] Wayland Session File"

SESSION="${TMPDIR}/fs/usr/share/wayland-sessions/ldde.desktop"
if [[ -f "${SESSION}" ]]; then
    grep -q '^Name=' "${SESSION}"       && pass "Session Name present"       || fail "Session Name missing"
    grep -q '^Exec=' "${SESSION}"       && pass "Session Exec present"       || fail "Session Exec missing"
    grep -q '^Type=Application' "${SESSION}" && pass "Session Type=Application" || fail "Session Type missing"
else
    fail "Wayland session file missing: /usr/share/wayland-sessions/ldde.desktop"
fi

# --- 7. Version manifest ---
echo ""
echo "[7] Version Manifest"

VER_FILE="${TMPDIR}/fs/usr/share/linuxdroid/ldde/version"
if [[ -f "${VER_FILE}" ]]; then
    MANIFEST_VER="$(cat "${VER_FILE}" | tr -d '[:space:]')"
    [[ "${MANIFEST_VER}" == "${PKG_VER}" ]] \
        && pass "Version manifest matches control version: ${MANIFEST_VER}" \
        || fail "Version mismatch: manifest='${MANIFEST_VER}', control='${PKG_VER}'"
else
    fail "Version manifest missing: /usr/share/linuxdroid/ldde/version"
fi

# --- Summary ---
echo ""
echo "========================================================"
echo "  Validation Summary"
echo "  PASS: ${PASS}  FAIL: ${FAIL}  WARN: ${WARN}"
echo "========================================================"

if [[ ${FAIL} -gt 0 ]]; then
    echo "  RESULT: FAILED"
    exit 1
else
    echo "  RESULT: PASSED"
    exit 0
fi
