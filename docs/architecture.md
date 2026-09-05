# LDDE Architecture — D0 Foundation

The **LinuxDroid Desktop Environment (LDDE)** is a standalone Linux-native Wayland desktop environment designed specifically for mobile-first Linux desktop usage.

## 1. Frozen Stack Architecture

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

### Responsibility Separation
- **LDDM**: Manages graphical session lifecycle and coordinates initialization.
- **Weston**: Wayland display compositor.
- **LDDE**: Wayland client providing desktop environment UX, shell services, and workspace orchestration.
- **Linux Applications**: Standard Wayland/Xwayland client applications.

> LDDE is strictly a **Wayland client** of Weston. It is not a compositor, does not replace Weston, and contains zero Android/PRoot-specific dependencies.

---

## 2. Core Subsystems in D0

```text
                                  +-------------------+
                                  |     LDDE Main     |
                                  +---------+---------+
                                            |
                                  +---------v---------+
                                  |    Application    |
                                  +----+----+----+----+
                                       |    |    |
        +------------------------------+    |    +-----------------------------+
        |                                   |                                  |
+-------v-------+                  +--------v--------+                 +-------v-------+
| Configuration |                  | Lifecycle &     |                 |  Centralized  |
| (Precedence & |                  | Readiness Mgr   |                 |    Logging    |
|  Versioning)  |                  +--------+--------+                 +---------------+
+---------------+                           |
                                   +--------v--------+
                                   |   Event Loop    |
                                   | (epoll, timers, |
                                   |  signalfd, fds) |
                                   +--------+--------+
                                            |
                                  +---------v---------+
                                  | WaylandConnection |
                                  |   (wl_display)    |
                                  +---------+---------+
                                            |
                                  +---------v---------+
                                  |  WaylandRegistry  |
                                  | (globals, caps)   |
                                  +----+-----------+--+
                                       |           |
                              +--------v---+   +---v--------+
                              |  Display   |   |   Input    |
                              |  Manager   |   |  Manager   |
                              | (wl_output)|   | (wl_seat)  |
                              +------------+   +------------+
```

### Core Lifecycle State Machine
LDDE transitions through deterministic lifecycle states:
```text
STARTING → INITIALIZING → CONNECTING_WAYLAND → INITIALIZING_COMPONENTS → READY → RUNNING → STOPPING → STOPPED
                                                                           ↓
                                                                         FAILED
```
- Invalid transitions are rejected with structured errors (`ErrorCode::InvalidLifecycleTransition`).
- State changes can be observed via registered callbacks.

### Centralized Logging (`ldde::core::Logger`)
- **Levels**: `TRACE`, `DEBUG`, `INFO`, `WARN`, `ERROR`, `FATAL`.
- **Categories**: `LDDE`, `CORE`, `WAYLAND`, `SHELL`, `WINDOW`, `APPLICATION`, `LAUNCHER`, `DOCK`, `SWITCHER`, `INPUT`, `DISPLAY`, `NOTIFICATION`, `SYSTEM`, `CONFIG`, `IPC`, `SESSION`.
- **Format**: `[Timestamp] [Level] [Category] Message (file:line)`.
- Thread-safe dispatching to modular sinks.

### Linux Event Loop (`ldde::core::EventLoop`)
- Built on Linux `epoll(7)` with `timerfd(2)`, `signalfd(2)`, and `eventfd(2)`.
- Monitors arbitrary file descriptors with read/write/error callbacks.
- Supports high-precision one-shot and recurring timers.
- Traps POSIX shutdown signals (`SIGINT`, `SIGTERM`, `SIGHUP`) cleanly.
- Drives Wayland client event dispatching (`wl_display_prepare_read`, `wl_display_read_events`, `wl_display_dispatch_pending`, `wl_display_flush`).

### Configuration (`ldde::config::Config`)
- Cascading configuration precedence:
  1. Built-in defaults
  2. System configuration: `/etc/linuxdroid/desktop.conf`
  3. User configuration: `~/.config/linuxdroid/desktop.conf` (or `$XDG_CONFIG_HOME/linuxdroid/desktop.conf`)
  4. Command-line overrides (`--config`, `--log-level`, `--wayland-display`, `--ready-fd`)
- Configuration versioning: `config_version = 1`. Mismatches trigger validation errors.
- Strongly-typed accessors with fallback defaults.

### Wayland Client Layer (`ldde::wayland`)
- `WaylandConnection`: RAII ownership of `wl_display`, disconnect handling, error reporting.
- `WaylandRegistry`: Dynamic global discovery, capability tracking, distinguishing required (`wl_compositor`, `wl_shm`), optional (`wl_output`, `wl_seat`), and future protocols.
- `Wrappers`: RAII custom deleters for all Wayland C types preventing resource leaks.

### Display Model (`ldde::display`)
- `DisplayInfo`: Holds output geometry, physical dimensions, orientation transforms, refresh rate, UI scale factors, and safe insets.
- `DisplayManager`: Tracks `wl_output` hotplug and configuration changes via Wayland registry.

### Input Model (`ldde::input`)
- `Seat`: Discovers and binds `wl_seat` capabilities (`Pointer`, `Keyboard`, `Touch`).
- `InputManager`: Manages seat instances and input device readiness.

