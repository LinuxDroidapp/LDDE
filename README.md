# LinuxDroid Desktop Environment (LDDE)

[![C++20](https://img.shields.io/badge/standard-C%2B%2B20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/build-CMake-brightgreen.svg)](https://cmake.org)
[![Wayland](https://img.shields.io/badge/wayland-client-orange.svg)](https://wayland.freedesktop.org)
[![Cairo](https://img.shields.io/badge/rendering-cairo-red.svg)](https://cairographics.org)

The **LinuxDroid Desktop Environment (LDDE)** is a standalone Linux-native Wayland desktop environment designed specifically for mobile-first Linux desktop usage.

> **Scope Status**: This repository contains **D0 — Foundation**, **D1 — Wayland Shell**, **D2 — Real Window Tracking**, **D3 — Window Management Subsystem**, **D4 — Mobile Display Policy**, **D5 — Touch Window Interaction**, **D6 — Application Discovery**, **D7 — Application Launcher**, **D8 — Dock**, **D9 — Application Switcher**, and **D10 — Home/Desktop**.

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
    LDDE
    ├── Home/Desktop (Background gradient/glow, Cairo vector surface, empty state, overlay dismiss, swipe-up)
    ├── Application Switcher (MRU tracking, multi-window grouping, Cairo overlay, fast touch/key switching)
    ├── Dock Subsystem (Pinned apps, authoritative running state, multi-window grouping, launcher toggle)
    ├── Application Launcher (Deterministic state machine, search, grid, launch handoff)
    ├── Application Discovery (Catalog, XDG desktop entry scanner, inotify monitor)
    ├── Touch Window Interaction (TouchInteractionManager, gestures, controls)
    ├── Window Tracking & Management (WindowRegistry, WindowTracker, WindowManager)
    └── Mobile Display Policy (DisplayManager, DisplayPolicy, safe areas)
        ↓
  Linux Applications (Wayland Clients)
```

### Separation of Responsibilities
- **LDDM**: Graphical session and display-manager lifecycle.
- **Weston**: Wayland compositor.
- **LDDE**: Desktop environment, desktop shell, display policy, window tracking / management, application discovery, launcher, dock, switcher, and home/desktop.
- **Linux Applications**: Standard Wayland/Xwayland applications.

LDDE is strictly a **Wayland client** of Weston. It contains **no Android-specific code** and does not know about Android APIs, APK paths, or PRoot internals.

---

## Subsystems in D0 – D10

1. **Home/Desktop Subsystem (D10)**:
   - **Persistent Desktop Surface**: Base-level Wayland surface rendered via double-buffered Cairo vector graphics into `DesktopSurface`.
   - **Background Styles & Glow**: Solid and linear vertical gradients with configurable top-center ambient wallpaper glow and empty-state branding watermark.
   - **Authoritative Window State Consumption**: Synchronized directly with D2 `WindowRegistry` to track active non-destroyed window counts and evaluate empty/focused desktop state.
   - **Multi-Overlay Dismissal**: Touch tap coordination that dismisses active transient shell overlays (D7 Launcher, D9 Switcher).
   - **Swipe-Up Launcher Navigation**: Responsive vertical touch gesture navigation opening the D7 Launcher.
   - **Dynamic Display Adaptation**: Adapts immediately to screen size, orientation changes (portrait/landscape), and safe area insets via D4 `DisplayPolicy`.
2. **Application Switcher Subsystem (D9)**:
   - **Authoritative Window & App State**: Consumes running state directly from D2 `WindowRegistry` and D3 `WindowManager` (strictly zero process polling, `/proc` scraping, or shell calls).
   - **Deterministic MRU Ordering**: Focus-driven in-memory MRU tracking; automatically pre-selects the most recently used previous application for rapid Alt+Tab switching.
   - **Multi-Window Grouping & Transients**: Groups multiple windows by application ID with count badges; associates transient dialogs under parent applications.
   - **Double-Buffered Cairo Rendering**: Translucent backdrop scrim, rounded cards, active/current/selected visual highlights, and vector badge fallbacks.
   - **Touch, Pointer & Keyboard Navigation**: Card tap activation, swipe scroll, Tab / Shift+Tab cycling, Arrow keys, Enter commit, and Esc cancel.
   - **Handoff Activation**: Automatically restores minimized windows before calling `WindowManager::activate()`.
   - **Dynamic Window Destruction Resilience**: Cleanly reacts to windows closed or destroyed in the background without stale pointers or crashes.
2. **Dock Subsystem (D8)**:
   - **Authoritative Running State**: Live synchronization with D2 `WindowRegistry` and D3 `WindowManager` (strictly zero process polling, `/proc` scraping, or shell calls).
   - **Application vs. Window Separation**: Groups multiple windows under their owning application with window count indicators.
   - **Pinned & Unpinned Management**: Configurable pinned applications via `dock.pinned`, dynamic pinning/unpinning, graceful missing desktop entry handling, and automatic dock presentation/removal of unpinned running apps.
   - **Integrated Launcher Button**: Leading-edge launcher button triggering `Launcher::toggle()` seamlessly.
   - **Lifecycle & Window Interaction**:
     - Not running: launches application via structured `ApplicationLauncher` backend (no `/bin/sh -c`).
     - Running unfocused or minimized: activates and restores window via `WindowManager`.
     - Running active: minimizes window to clear desktop space.
   - **Cairo Vector Rendering**: Floating pill geometry with custom application badges, active glowing indicator, running dots, and minimized alpha blending rendered into shell `DockRegion` `ShmBuffer`.
   - **Responsive Touch Layout**: Dynamic geometry adhering to D4 `DisplayPolicy` metrics, $\ge 48\,\text{dp}$ touch targets, and horizontal overflow scrolling.
2. **Application Launcher Subsystem (D7)**:
   - **Deterministic State Machine**: `Closed` ↔ `Opening` ↔ `Open` ↔ `Searching` ↔ `Launching` (with `LaunchFailed` banner) ↔ `Closing`.
   - **Responsive Grid Layout**: Dynamically computes grid columns based on D4 `DisplayPolicy` metrics, margins, and minimum item width ($\ge 48\,\text{dp}$ touch targets).
   - **Freedesktop Icon Resolution**: Thread-safe resolution and LRU caching across standard XDG icon directories and sizes, with fallback Cairo vector badges.
   - **In-Memory Search & Multi-Tier Scoring**: Exact Name (1000) → Prefix (800) → Word Prefix (600) → Substring (400) → GenericName (300) → Keywords (200) → Comment (100), with stable tie-breaking by localized Name then `ApplicationId`.
   - **Category Navigation & Filtering**: Canonical categories (`AudioVideo`, `Development`, `Education`, `Game`, `Graphics`, `Network`, `Office`, `Settings`, `System`, `Utility`, `Other`) with dynamic item counts.
   - **Input Controller**: Arrow navigation, Enter to launch, Esc to clear search/close, Tab category cycling, touch tap, drag-to-scroll with boundary clamping.
   - **Structured Launch Handoff**: Clean `fork()` + `pipe2(O_CLOEXEC)` + `execvp()` launch execution without shell strings (`/bin/sh -c` is prohibited) and zero synthetic window creation.
2. **Application Discovery Subsystem (D6)**:
   - **Authoritative Catalog**: Scans `$XDG_DATA_HOME/applications` and `$XDG_DATA_DIRS/applications`, parses `.desktop` files, enforces user override rules, and monitors live filesystem updates with inotify.
3. **Touch Window Interaction Subsystem (D5)**:
   - **Touch-First Window Manipulation**: Touch-safe dragging, multi-edge resizing, window controls (close, minimize, maximize), and multi-touch contact ownership.
4. **Mobile Display Policy Subsystem (D4)**:
   - **Responsive Display Model**: Dynamic form factor detection (phone portrait/landscape, tablet, desktop), logical scale normalization, cutout/safe-area handling, and display geometry change propagation.
5. **Window Management Subsystem (D3)**:
   - **State & Geometry Control**: Focus arbitration, Z-order stacking, maximize, fullscreen, minimize, and restore policies.
6. **Real Window Tracking Subsystem (D2)**:
   - **Authoritative Window Model**: Real Wayland surface tracking (`wl_surface`, `xdg_surface`, `xdg_toplevel`), stable `WindowId`, metadata, lifecycle states, and dedicated application tracking endpoint (`wayland-ldde-apps`).
7. **Wayland Shell Subsystem (D1)**:
   - **Root Surface & Subsurfaces**: Desktop background, status region, dock region, shell overlay foundation with double-buffered POSIX shared memory and Cairo vector rendering.
8. **Core Foundation (D0)**:
   - **Lifecycle & Event Loop**: State tracking, thread-safe logger, monadic `Status`/`Result<T>`, Linux `epoll(7)` event loop, Wayland client connection, INI configuration system, and LDDM dual-channel readiness reporting (`sd_notify` / `--ready-fd`).

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
- **D2 Real Window Tracking** *(Completed)*
- **D3 Window Manager** *(Completed)*
- **D4 Mobile Display Policy** *(Completed)*
- **D5 Touch Interaction** *(Completed)*
- **D6 Application Discovery** *(Completed)*
- **D7 Launcher** *(Completed)*
- **D8 Dock** *(Completed)*
- **D9 Application Switcher** *(Completed)*
- **D10 Home/Desktop** *(Completed)*
- **D11 System UI**
- **D12 Notifications**
- **D13 Settings**
- **D14 Performance & UX**
- **D15 Packaging**
