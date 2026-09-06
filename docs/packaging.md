# D15 — LDDE Packaging

## Overview

D15 is the final LDDE implementation phase. It adds production-grade Linux packaging infrastructure to make LDDE installable, deployable, upgradeable, and removable as a proper Linux-native desktop environment component.

No new desktop features are introduced. All prior phases (D0–D14) are preserved exactly.

---

## Package Identity

| Field       | Value                                  |
|-------------|----------------------------------------|
| Name        | `linuxdroid-desktop-environment`       |
| Version     | `1.0.0`                                |
| Architecture| `arm64` (primary); `amd64` (dev build) |
| Section     | `x11`                                  |
| License     | Apache 2.0                             |

---

## Version Management

The authoritative LDDE version is declared **once** in `CMakeLists.txt`:

```cmake
project(LDDE VERSION 1.0.0 LANGUAGES C CXX)
```

CMake's `configure_file()` generates `include/ldde/version.hpp` from `cmake/LDDEVersion.hpp.in` at configure time. This eliminates version string drift across source files.

All version references (`kVersion` in `types.hpp`, D-Bus notification backend) have been updated to `1.0.0`.

---

## Installed File Layout

```
/usr/bin/ldde                                                    ← main executable
/usr/share/wayland-sessions/ldde.desktop                         ← session entry (LDDM discovery)
/etc/linuxdroid/desktop.conf                                     ← system config (conffile)
/usr/share/linuxdroid/ldde/version                               ← version manifest
/usr/share/doc/linuxdroid-desktop-environment/README.md          ← documentation
/usr/share/licenses/linuxdroid-desktop-environment/LICENSE       ← license
/usr/lib/ldde/libldde_core.a                                     ← static library
/usr/include/ldde/                                               ← public headers
```

### Config File (conffile)

`/etc/linuxdroid/desktop.conf` is registered as a `dpkg` conffile. This means:

- `dpkg` will **never silently overwrite** admin customizations on upgrade.
- If you have modified the file and the package ships an updated default, `dpkg` will prompt you to choose between your version and the package's new version.
- The `postinst` script only installs the default config if no config file exists.
- The `prerm` script does **not** remove the config file.

---

## Build & Package

### Development Build (x86_64)

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build --parallel
```

### Release .deb Package

```bash
./scripts/package.sh
# Output: dist/linuxdroid-desktop-environment_1.0.0_amd64.deb (dev)
#         dist/linuxdroid-desktop-environment_1.0.0_arm64.deb (ARM64 target)
```

### ARM64 Cross-Compilation

```bash
cmake -S . -B build-arm64 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake \
    -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build-arm64 --parallel
cd build-arm64 && cpack -G DEB
```

### Staged Install (DESTDIR)

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
DESTDIR=/tmp/ldde-staging cmake --install build
# Files appear under /tmp/ldde-staging/usr/...
```

---

## Debian Source Packaging

The `debian/` directory enables `dpkg-buildpackage` for Debian-native source packaging:

```
debian/
├── control       — Package metadata and dependencies
├── changelog     — Version history
├── copyright     — License declaration
├── rules         — Build rules (debhelper + CMake)
├── conffiles     — Files preserved on upgrade
├── postinst      — Post-install: create config if absent
├── prerm         — Pre-removal: preserve config
└── source/
    └── format    — 3.0 (native)
```

To build using `dpkg-buildpackage`:

```bash
dpkg-buildpackage -us -uc -b
```

---

## Wayland Session Entry

`/usr/share/wayland-sessions/ldde.desktop` registers LDDE as a Wayland session. LDDM (and other display managers) discover sessions by scanning this directory.

Content:
```ini
[Desktop Entry]
Name=LDDE
Comment=LinuxDroid Desktop Environment — Wayland shell for LinuxDroid
Exec=ldde
TryExec=ldde
Type=Application
DesktopNames=LDDE
```

---

## Runtime Dependencies

| Library            | Minimum Version | Debian Package       |
|--------------------|----------------|----------------------|
| libwayland-client  | 1.20           | `libwayland-client0` |
| libcairo           | 1.16           | `libcairo2`          |
| libglib2.0         | 2.60           | `libglib2.0-0`       |

Recommended:
- `weston >= 10.0` — Wayland compositor

---

## Packaging Tests

```bash
# Run full packaging test suite (configure, build, install, validate)
./tests/packaging/test_packaging.sh

# Validate an existing .deb without installing
./scripts/validate-package.sh dist/linuxdroid-desktop-environment_1.0.0_amd64.deb
```

The packaging test suite checks:
1. CMake configure succeeds and generates `version.hpp`
2. Build succeeds without warnings
3. Staging install contains all required files
4. Version manifest matches CMakeLists.txt version
5. Generated `version.hpp` has correct content
6. Wayland session file is valid
7. Config file is present and has correct sections
8. `debian/control` metadata is correct
9. `dpkg-deb` roundtrip succeeds
10. All unit tests pass (293 tests)
11. All integration tests pass (42 tests)

---

## Upgrade Safety

| Scenario                              | Behavior                                          |
|---------------------------------------|--------------------------------------------------|
| First install, no config exists       | Default config installed to `/etc/linuxdroid/desktop.conf` |
| Upgrade, config unmodified            | Package default config silently replaces it       |
| Upgrade, config modified by admin     | `dpkg` prompts: keep yours or use new default     |
| Removal (`dpkg -r`)                   | Binary, session file, docs removed; config retained |
| Purge (`dpkg -P`)                     | Everything including config removed               |

---

## Architecture Notes

- ARM64 is the primary target architecture (LinuxDroid runs on Android devices)
- Development and CI builds produce `amd64` packages by default
- For ARM64, cross-compile using a `aarch64-linux-gnu` toolchain
- The CMakeLists.txt detects architecture via `dpkg --print-architecture` at configure time
