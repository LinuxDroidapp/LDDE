# LDDM Integration Contract

This document specifies the integration contract between the LinuxDroid Display Manager (**LDDM**) and the LinuxDroid Desktop Environment (**LDDE**).

## 1. Startup & Environment Hand-off

LDDM starts LDDE in a prepared Linux session environment:

```text
LDDM
  ↓
launches LDDE
  ↓
provides session environment
  ↓
LDDE connects to Weston
  ↓
LDDE initializes components
  ↓
LDDE establishes readiness
  ↓
LDDE reports readiness to LDDM
```

### Session Environment Variables

LDDE inspects and honors the following standard Linux session variables:

- `WAYLAND_DISPLAY`: The Wayland socket name exposed by Weston (e.g. `wayland-0`). Can be overridden via `--wayland-display <name>`.
- `XDG_RUNTIME_DIR`: The runtime directory path where Wayland and IPC sockets reside (required).
- `XDG_SESSION_TYPE`: Set to `wayland` if unset.
- `XDG_CURRENT_DESKTOP`: Set to `LDDE` if unset.

LDDE does not set or require Android-specific environment variables.

---

## 2. Readiness Protocol

LDDE clearly distinguishes between process launch and operational desktop readiness.

Readiness is signaled **only** after LDDE has:
1. Parsed CLI arguments and loaded configuration.
2. Initialized centralized logging.
3. Connected to Weston's Wayland display socket.
4. Synchronized the Wayland registry and verified required globals (`wl_compositor`, `wl_shm`).
5. Discovered outputs and seats.
6. Initialized event-loop dispatching.
7. Successfully transitioned to the `READY` lifecycle state.

### Signaling Mechanisms

LDDE supports dual signaling mechanisms for full interoperability:

#### 1. File Descriptor (`--ready-fd` or `READY_FD`)
- When invoked with `--ready-fd <fd>` or if the `READY_FD` / `LDDE_READY_FD` environment variable is set:
- LDDE writes a single newline character (`\n`) to the specified file descriptor and closes it.

#### 2. Systemd-Style Notification Socket (`NOTIFY_SOCKET`)
- If the `NOTIFY_SOCKET` environment variable is set:
- LDDE sends a datagram containing `READY=1\n` to the specified UNIX domain socket.

---

## 3. Deterministic Shutdown

LDDM can request clean shutdown of LDDE by sending standard POSIX signals:
- `SIGTERM`
- `SIGINT`
- `SIGHUP`

Upon receipt, LDDE initiates a deterministic shutdown sequence:
1. Transitions lifecycle to `STOPPING`.
2. Halts event loop task dispatching.
3. Disconnects and releases Wayland protocol objects (`wl_output`, `wl_seat`, `wl_registry`, `wl_display`).
4. Flushes all log sinks.
5. Transitions lifecycle to `STOPPED`.
6. Exits with status code `0`.

