# LinuxDroid Desktop Environment (LDDE)

[![C++20](https://img.shields.io/badge/standard-C%2B%2B20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/build-CMake-brightgreen.svg)](https://cmake.org)
[![Wayland](https://img.shields.io/badge/wayland-client-orange.svg)](https://wayland.freedesktop.org)

The **LinuxDroid Desktop Environment (LDDE)** is a standalone Linux-native Wayland desktop environment designed specifically for mobile-first Linux desktop usage.

> **Scope Notice**: This repository currently contains **D0 — Foundation**. D0 establishes the production foundation (runtime lifecycle, Wayland client connection, centralized logging, event loop, display and input models, configuration system, and LDDM readiness contract). It does **not** yet implement shell UI, launcher, dock, window management, or other later-phase features.

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
      Weston
        ↓
       LDDE
        ↓
 Linux Applications
```

### Separation of Responsibilities
- **LDDM**: Graphical session and display-manager lifecycle.
- **Weston**: Wayland compositor.
- **LDDE**: Desktop environment and desktop UX.
- **Linux Applications**: Standard Wayland/Xwayland applications.

LDDE is strictly a **Wayland client** of Weston. It contains **no Android-specific code** and does not know about Android APIs, APK paths, or PRoot internals.

---

## Subsystems in D0

1. **Core Lifecycle State Machine**: Formal state tracking (`STARTING`, `INITIALIZING`, `CONNECTING_WAYLAND`, `INITIALIZING_COMPONENTS`, `READY`, `RUNNING`, `STOPPING`, `STOPPED`, `FAILED`).
2. **Centralized Logger**: Multi-category, thread-safe logger with severity levels (`TRACE`, `DEBUG`, `INFO`, `WARN`, `ERROR`, `FATAL`).
3. **Structured Error Model**: Categorized diagnostics with `Status` and `Result<T>` monadic error handling.
4. **Linux Event Loop**: Linux `epoll(7)` event loop with timerfd, signalfd, and eventfd support.
5. **Wayland Client Layer**: RAII wrappers around `wl_display`, `wl_registry`, `wl_output`, `wl_seat`, and event dispatching.
6. **Display Foundation**: Display geometry model (`DisplayInfo`), mode tracking, transforms, UI scaling, and safe insets.
7. **Input Foundation**: Wayland seat capability monitoring (`Pointer`, `Keyboard`, `Touch`).
8. **Configuration System**: INI parser with cascading precedence (Defaults → `/etc/linuxdroid/desktop.conf` → `~/.config/linuxdroid/desktop.conf` → CLI overrides) and schema versioning (`config_version = 1`).
9. **LDDM Readiness Contract**: Dual-channel readiness reporting via `NOTIFY_SOCKET` (`sd_notify("READY=1")`) and `--ready-fd <fd>`.

---

## Build Requirements

### Build Dependencies
- C++20 compiler (`g++` >= 13 or `clang++` >= 16)
- CMake (>= 3.20)
- `pkg-config`
- `libwayland-dev` (>= 1.20)
- `wayland-protocols`
- `libgtest-dev` (for unit and integration tests)

### Runtime Dependencies
- `libwayland-client0`
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
The integration test launches Weston in headless mode and verifies client connection, registry discovery, output/seat enumeration, readiness signaling, and clean shutdown:
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
```

---

## LDDE Roadmap

- **D0 Foundation** *(Completed)*
- **D1 Wayland Shell**
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