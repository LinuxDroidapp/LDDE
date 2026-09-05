# LinuxDroid Desktop Environment (LDDE)

[![C++20](https://img.shields.io/badge/standard-C%2B%2B20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/build-CMake-brightgreen.svg)](https://cmake.org)
[![Wayland](https://img.shields.io/badge/wayland-client-orange.svg)](https://wayland.freedesktop.org)
[![Cairo](https://img.shields.io/badge/rendering-cairo-red.svg)](https://cairographics.org)

The **LinuxDroid Desktop Environment (LDDE)** is a standalone Linux-native Wayland desktop environment designed specifically for mobile-first Linux desktop usage.

> **Scope Status**: This repository contains **D0 — Foundation** and **D1 — Wayland Shell**. D1 establishes the real visual shell running as a Linux-native Wayland client under Weston with desktop background, status bar, floating dock, modal overlay layer, responsive layout engine, Cairo vector rendering, and design tokens.

---

## Architecture

LDDE resides in the LinuxDroid runtime stack:

```text
LinuxDroid Runtime
        ↓
    Guest Init
        ↓
       LDDM
        ↓
      Weston (Compositor)
        ↓
   LDDE Shell (Wayland Client)
   ├── Desktop Surface (Background)
   ├── Status Region (Top bar subsurface)
   ├── Dock Region (Floating pill subsurface)
   └── Shell Overlay (Modal / Scrim subsurface)
        ↓
 Linux Applications
```

### Separation of Responsibilities
- **LDDM**: Graphical session and display-manager lifecycle.
- **Weston**: Wayland compositor.
- **LDDE**: Desktop environment, desktop shell, and desktop UX.
- **Linux Applications**: Standard Wayland/Xwayland applications.

LDDE is strictly a **Wayland client** of Weston. It contains **no Android-specific code** and does not know about Android APIs, APK paths, or PRoot internals.

---

## Subsystems in D0 + D1

1. **Wayland Shell Subsystem (D1)**:
   - **Root Desktop Surface**: Fullscreen background with Cairo vector gradients and subtle wallpaper ambient glow.
   - **Top / Status Region**: Subsurface positioned in top safe area with typography clock and system status indicator badge.
   - **Bottom / Dock Region**: Centered floating rounded pill with responsive width ratio (90% in portrait, 60% in landscape) and slot indicators.
   - **Shell Overlay Foundation**: Fullscreen scrim and modal card subsurface with hit interception.
   - **Subsurface Composition**: Uses `wl_subcompositor` desync mode for independent, zero-flicker rendering.
   - **Shm Buffer Pool**: Double-buffered POSIX shared memory (`memfd_create` with `MFD_CLOEXEC` fallback to `shm_open`).
   - **Design Tokens & Theme**: DP-to-pixel scaling (160 DPI baseline) with `#RRGGBB` / `#RRGGBBAA` hex color parser and theme presets.
   - **Responsive Shell Layout**: Centralized layout calculation with display cutout/gesture safe insets and geometric hit testing.
2. **Core Runtime & Lifecycle (D0)**: Formal state tracking (`STARTING`, `INITIALIZING`, `CONNECTING_WAYLAND`, `INITIALIZING_COMPONENTS`, `READY`, `RUNNING`, `STOPPING`, `STOPPED`, `FAILED`).
3. **Centralized Logger (D0)**: Multi-category, thread-safe logger with severity levels (`TRACE`, `DEBUG`, `INFO`, `WARN`, `ERROR`, `FATAL`).
4. **Structured Error Model (D0)**: Categorized diagnostics with `Status` and `Result<T>` monadic error handling.
5. **Linux Event Loop (D0)**: Linux `epoll(7)` event loop with timerfd, signalfd, and eventfd support.
6. **Wayland Client Layer (D0)**: RAII wrappers around `wl_display`, `wl_registry`, `wl_output`, `wl_seat`, and event dispatching.
7. **Display Foundation (D0)**: Display geometry model (`DisplayInfo`), mode tracking, transforms, UI scaling, and safe insets.
8. **Input Foundation (D0)**: Wayland seat capability monitoring (`Pointer`, `Keyboard`, `Touch`).
9. **Configuration System (D0)**: INI parser with cascading precedence (Defaults → `/etc/linuxdroid/desktop.conf` → `~/.config/linuxdroid/desktop.conf` → CLI overrides) and schema versioning (`config_version = 1`).
10. **LDDM Readiness Contract (D0/D1)**: Dual-channel readiness reporting via `NOTIFY_SOCKET` (`sd_notify("READY=1")`) and `--ready-fd <fd>`, signaled only when shell reaches `READY`.

---

## Build Requirements

### Build Dependencies
- C++20 compiler (`g++` >= 13 or `clang++` >= 16)
- CMake (>= 3.20)
- `pkg-config`
- `libwayland-dev` (>= 1.20)
- `wayland-protocols`
- `libcairo2-dev` (>= 1.16)
- `libgtest-dev` (for unit and integration tests)

### Runtime Dependencies
- `libwayland-client0`
- `libcairo2`
- `weston` (Wayland compositor)

---

## Building

### Debug Build
```bash
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -j$(nproc)
```

### Release Build
```bash
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j$(nproc)
```

---

## Running Tests

### Automated Test Suite (CTest)
```bash
ctest --test-dir build-debug --output-on-failure
```

### Unit Tests
```bash
./build-debug/tests/ldde_unit_tests
```

### Real Weston Integration Test
The integration test launches Weston in headless mode and verifies Wayland connection, registry discovery, output enumeration, surface creation, Cairo rendering, buffer commits, hit testing, readiness signaling, and clean shutdown:
```bash
./build-debug/tests/ldde_integration_tests
```

---

## Running LDDE

```bash
# Export standard Wayland runtime environment
export XDG_RUNTIME_DIR=/run/user/$(id -u)
export WAYLAND_DISPLAY=wayland-0

# Run LDDE
./build-release/ldde [options]
```

### Command-Line Options
```text
Options:
  -h, --help                  Show help text and exit
  -v, --version               Show version and exit
  -c, --config <path>         Specify path to configuration file
  -l, --log-level <level>     Log level (TRACE, DEBUG, INFO, WARN, ERROR, FATAL)
  -d, --wayland-display <name> Wayland display socket name to connect to
      --ready-fd <fd>         File descriptor to signal readiness to LDDM
```

---

## Configuration

LDDE searches configuration files in the following precedence order:
1. Built-in defaults
2. `/etc/linuxdroid/desktop.conf` (System configuration)
3. `~/.config/linuxdroid/desktop.conf` (User configuration)
4. Command-line overrides

A template configuration is provided in `config/desktop.conf.example`.

```ini
[general]
config_version = 1
desktop_name = LDDE

[logging]
level = INFO

[display]
scale_factor = 1.0

[input]
tap_to_click = true

[shell]
status_bar_enabled = true
dock_enabled = true
dock_position = bottom
desktop_bg_top = #111726
desktop_bg_bottom = #0a0e17
status_bg = #161e30dd
dock_bg = #141c2cfa
```

---

## LDDE Roadmap

- **D0 Foundation** *(Completed)*
- **D1 Wayland Shell** *(Completed)*
- **D2 Real Window Tracking**
- **D3 Window Manager**
- **D4 Mobile Display Policy**
- **D5 Touch Interaction**
- **D6 Application Discovery**
- **D7 Launcher**
- **D8 Dock**
- **D9 Application Switcher**
- **D10 Home/Desktop**
- **D11 System UI**
- **D12 Notifications**
- **D13 Settings**
- **D14 Performance & UX**
- **D15 Packaging**
