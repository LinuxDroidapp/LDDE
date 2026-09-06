#!/usr/bin/env bash
# test_packaging.sh — LDDE D15 Packaging Test Suite
#
# Tests:
#   1. CMake configure succeeds and generates version header
#   2. CMake build succeeds
#   3. Staging install contains required files
#   4. Version manifest content matches CMakeLists.txt version
#   5. Generated version.hpp content is correct
#   6. ldde.desktop Wayland session file is valid
#   7. Config file is present and parseable
#   8. debian/control metadata is correct
#   9. dpkg-deb package roundtrip (if dpkg-deb available)
#   10. All existing unit tests still pass
#   11. All existing integration tests still pass

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build-pkg-test"
STAGING_DIR="${BUILD_DIR}/staging"

PASS=0
FAIL=0

pass() { echo "  [PASS] $1"; ((PASS++)) || true; }
fail() { echo "  [FAIL] $1"; ((FAIL++)) || true; }

section() {
    echo ""
    echo "--- $1 ---"
}

echo "========================================================"
echo "  LDDE Packaging Test Suite"
echo "========================================================"

# ---------------------------------------------------------------
section "Test 1: CMake Configure"

mkdir -p "${BUILD_DIR}"

if cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_INSTALL_SYSCONFDIR=/etc \
        -DBUILD_TESTING=ON \
        > "${BUILD_DIR}/configure.log" 2>&1; then
    pass "cmake configure succeeded"
else
    fail "cmake configure failed (see ${BUILD_DIR}/configure.log)"
    cat "${BUILD_DIR}/configure.log" >&2
fi

# ---------------------------------------------------------------
section "Test 2: Generated Version Header"

VERSION_HPP="${BUILD_DIR}/include/ldde/version.hpp"
if [[ -f "${VERSION_HPP}" ]]; then
    pass "version.hpp generated"
    if grep -q 'LDDE_VERSION_MAJOR' "${VERSION_HPP}" && \
       grep -q 'LDDE_VERSION_STRING' "${VERSION_HPP}"; then
        pass "version.hpp contains expected symbols"
        MAJOR="$(grep '#define LDDE_VERSION_MAJOR' "${VERSION_HPP}" | awk '{print $3}')"
        MINOR="$(grep '#define LDDE_VERSION_MINOR' "${VERSION_HPP}" | awk '{print $3}')"
        PATCH="$(grep '#define LDDE_VERSION_PATCH' "${VERSION_HPP}" | awk '{print $3}')"
        [[ "${MAJOR}.${MINOR}.${PATCH}" == "1.0.0" ]] \
            && pass "version.hpp version is 1.0.0" \
            || fail "version.hpp version is ${MAJOR}.${MINOR}.${PATCH}, expected 1.0.0"
    else
        fail "version.hpp missing expected symbols"
    fi
else
    fail "version.hpp not generated at ${VERSION_HPP}"
fi

# ---------------------------------------------------------------
section "Test 3: CMake Build"

if cmake --build "${BUILD_DIR}" --parallel "$(nproc)" \
        > "${BUILD_DIR}/build.log" 2>&1; then
    pass "cmake build succeeded"
else
    fail "cmake build failed (see ${BUILD_DIR}/build.log)"
    tail -30 "${BUILD_DIR}/build.log" >&2
fi

# ---------------------------------------------------------------
section "Test 4: Staging Install"

rm -rf "${STAGING_DIR}"
if DESTDIR="${STAGING_DIR}" cmake --install "${BUILD_DIR}" \
        > "${BUILD_DIR}/install.log" 2>&1; then
    pass "cmake install succeeded"
else
    fail "cmake install failed (see ${BUILD_DIR}/install.log)"
    cat "${BUILD_DIR}/install.log" >&2
fi

# Check required installed files
check_staged() {
    local rel="$1"
    local path="${STAGING_DIR}${rel}"
    if [[ -e "${path}" ]]; then
        pass "Staged: ${rel}"
    else
        fail "Missing from staging: ${rel}"
    fi
}

check_staged "/usr/bin/ldde"
check_staged "/usr/share/wayland-sessions/ldde.desktop"
check_staged "/etc/linuxdroid/desktop.conf"
check_staged "/usr/share/linuxdroid/ldde/version"
check_staged "/usr/share/doc/linuxdroid-desktop-environment/README.md"

# ---------------------------------------------------------------
section "Test 5: Version Manifest"

VER_FILE="${STAGING_DIR}/usr/share/linuxdroid/ldde/version"
if [[ -f "${VER_FILE}" ]]; then
    MANIFEST_VER="$(cat "${VER_FILE}" | tr -d '[:space:]')"
    [[ "${MANIFEST_VER}" == "1.0.0" ]] \
        && pass "Version manifest: ${MANIFEST_VER}" \
        || fail "Version manifest is '${MANIFEST_VER}', expected '1.0.0'"
else
    fail "Version manifest not found"
fi

# ---------------------------------------------------------------
section "Test 6: Wayland Session File"

SESSION="${STAGING_DIR}/usr/share/wayland-sessions/ldde.desktop"
if [[ -f "${SESSION}" ]]; then
    grep -q '^Name=' "${SESSION}"            && pass "Session: Name present"    || fail "Session: Name missing"
    grep -q '^Exec=ldde' "${SESSION}"        && pass "Session: Exec=ldde"       || fail "Session: Exec missing/wrong"
    grep -q '^Type=Application' "${SESSION}" && pass "Session: Type=Application" || fail "Session: Type missing"
else
    fail "Wayland session file not staged"
fi

# ---------------------------------------------------------------
section "Test 7: Config File"

CONF="${STAGING_DIR}/etc/linuxdroid/desktop.conf"
if [[ -f "${CONF}" ]]; then
    pass "Config file present"
    grep -q '\[general\]' "${CONF}"   && pass "Config: [general] section present"  || fail "Config: [general] section missing"
    grep -q '\[display\]' "${CONF}"   && pass "Config: [display] section present"  || fail "Config: [display] section missing"
    grep -q '\[shell\]' "${CONF}"     && pass "Config: [shell] section present"    || fail "Config: [shell] section missing"
else
    fail "Config file not staged"
fi

# ---------------------------------------------------------------
section "Test 8: debian/control Metadata"

CONTROL="${ROOT_DIR}/debian/control"
if [[ -f "${CONTROL}" ]]; then
    pass "debian/control exists"
    grep -q 'Package: linuxdroid-desktop-environment' "${CONTROL}" \
        && pass "debian/control: Package name correct" \
        || fail "debian/control: Package name wrong"
    grep -q 'Architecture: arm64' "${CONTROL}" \
        && pass "debian/control: Architecture arm64" \
        || fail "debian/control: Architecture not arm64"
    grep -q 'libwayland-client0' "${CONTROL}" \
        && pass "debian/control: libwayland-client0 dependency present" \
        || fail "debian/control: libwayland-client0 dependency missing"
else
    fail "debian/control not found"
fi

# ---------------------------------------------------------------
section "Test 9: CPack DEB Roundtrip"

if command -v cpack &>/dev/null && command -v dpkg-deb &>/dev/null; then
    DEB_OUT=""
    # Run CPack from the build dir to produce a .deb
    if (cd "${BUILD_DIR}" && cpack -G DEB --config CPackConfig.cmake > "${BUILD_DIR}/cpack.log" 2>&1); then
        # Find the generated .deb
        DEB_OUT="$(ls "${BUILD_DIR}"/*.deb 2>/dev/null | head -1)"
        if [[ -n "${DEB_OUT}" && -f "${DEB_OUT}" ]]; then
            pass "CPack DEB generated: $(basename "${DEB_OUT}")"

            # Inspect via dpkg-deb
            INFO="$(dpkg-deb --info "${DEB_OUT}" 2>/dev/null || true)"
            [[ -n "${INFO}" ]] \
                && pass "dpkg-deb --info: package readable" \
                || fail "dpkg-deb --info: failed to read package"

            # Check package name in info
            echo "${INFO}" | grep -q 'Package: linuxdroid-desktop-environment' \
                && pass "dpkg-deb: Package name correct" \
                || fail "dpkg-deb: Package name wrong"

            # Check version in info
            echo "${INFO}" | grep -q 'Version: 1.0.0' \
                && pass "dpkg-deb: Version 1.0.0" \
                || fail "dpkg-deb: Version wrong"

            # Check contents include ldde binary
            dpkg-deb --contents "${DEB_OUT}" 2>/dev/null | grep -q 'usr/bin/ldde' \
                && pass "dpkg-deb: /usr/bin/ldde present in package" \
                || fail "dpkg-deb: /usr/bin/ldde missing from package"

            # Check conffiles
            dpkg-deb -e "${DEB_OUT}" "${BUILD_DIR}/DEBIAN_inspect" 2>/dev/null || true
            if [[ -f "${BUILD_DIR}/DEBIAN_inspect/conffiles" ]]; then
                grep -q '/etc/linuxdroid/desktop.conf' "${BUILD_DIR}/DEBIAN_inspect/conffiles" \
                    && pass "dpkg-deb: conffiles lists /etc/linuxdroid/desktop.conf" \
                    || fail "dpkg-deb: conffiles missing /etc/linuxdroid/desktop.conf"
            else
                fail "dpkg-deb: no conffiles in generated package"
            fi
        else
            fail "CPack DEB: no .deb file produced"
        fi
    else
        fail "CPack DEB generation failed (see ${BUILD_DIR}/cpack.log)"
        cat "${BUILD_DIR}/cpack.log" >&2 || true
    fi
else
    echo "  [SKIP] cpack or dpkg-deb not available"
fi

# ---------------------------------------------------------------
section "Test 10: Unit Tests"

UNIT_BIN="${BUILD_DIR}/tests/ldde_unit_tests"
if [[ -f "${UNIT_BIN}" ]]; then
    if "${UNIT_BIN}" --gtest_brief=1 > "${BUILD_DIR}/unit_test.log" 2>&1; then
        TOTAL="$(grep -oE '[0-9]+ tests? from' "${BUILD_DIR}/unit_test.log" | tail -1 | grep -oE '[0-9]+' || echo '?')"
        pass "Unit tests passed (${TOTAL} tests)"
    else
        fail "Unit tests FAILED (see ${BUILD_DIR}/unit_test.log)"
        tail -20 "${BUILD_DIR}/unit_test.log" >&2
    fi
else
    echo "  [SKIP] Unit test binary not built (BUILD_TESTING may be OFF)"
fi

# ---------------------------------------------------------------
section "Test 11: Integration Tests"

INT_BIN="${BUILD_DIR}/tests/ldde_integration_tests"
if [[ -f "${INT_BIN}" ]]; then
    if "${INT_BIN}" --gtest_brief=1 > "${BUILD_DIR}/integration_test.log" 2>&1; then
        TOTAL="$(grep -oE '[0-9]+ tests? from' "${BUILD_DIR}/integration_test.log" | tail -1 | grep -oE '[0-9]+' || echo '?')"
        pass "Integration tests passed (${TOTAL} tests)"
    else
        fail "Integration tests FAILED (see ${BUILD_DIR}/integration_test.log)"
        tail -20 "${BUILD_DIR}/integration_test.log" >&2
    fi
else
    echo "  [SKIP] Integration test binary not built"
fi

# ---------------------------------------------------------------
echo ""
echo "========================================================"
echo "  Test Summary: PASS=${PASS}  FAIL=${FAIL}"
echo "========================================================"

if [[ ${FAIL} -gt 0 ]]; then
    echo "  RESULT: FAILED"
    exit 1
else
    echo "  RESULT: PASSED"
    exit 0
fi
