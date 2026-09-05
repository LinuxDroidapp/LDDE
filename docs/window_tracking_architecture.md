# LDDE D2 — Real Window Tracking Architecture

## 1. Architectural Role & Frozen Boundaries

LDDE preserves the strict layered LinuxDroid architecture:

```text
LinuxDroid Runtime
        ↓
Guest Init
        ↓
LDDM (Display Manager)
        ↓
Weston (Wayland Compositor)
        ↓
LDDE (Desktop Environment + Shell + Window Tracking)
        ↓
Linux Applications (Wayland Clients)
```

### Separation of Concerns: D2 vs D3
- **D2 (Real Window Tracking)** is **authoritative and observational**. It discovers, tracks, identifies, and models real Wayland application windows. The tracker reports **facts**: identity, title, app-id, geometry, configure/ack cycles, visibility, and lifecycle transitions.
- **Tracker does NOT decide policy**: It does not make placement decisions, manage focus policies, or control window tiling/floating. Those decisions belong to **D3 (Window Manager)**.
- **Zero Synthetic Windows**: Every window object tracked in `WindowRegistry` maps directly to a real Wayland surface (`wl_surface`, `xdg_surface`, `xdg_toplevel`).
- **Zero Process Launching**: The tracker does not fork, exec, or interact with Android / PRoot / ProcessSupervisor APIs.

---

## 2. Window Object Model

The `ldde::window::Window` class represents a real tracked window surface.

```text
┌────────────────────────────────────────────────────────┐
│                      Window                            │
├────────────────────────────────────────────────────────┤
│ - id: WindowId (uint64_t)                              │
│ - title: string                                        │
│ - app_id: string                                       │
│ - geometry: Rect (x, y, width, height)                 │
│ - surface_size: Size (width, height)                   │
│ - state: WindowState                                   │
│     (Normal, Maximized, Fullscreen, Minimized)         │
│ - requested_state: WindowState                         │
│ - lifecycle_state: WindowLifecycleState                │
│ - is_active: bool (focus state)                        │
│ - is_visible: bool (mapped & rendered)                 │
│ - parent_id: optional<WindowId> (transient dialogs)    │
│ - last_configure_serial: uint32_t                      │
│ - creation_time: steady_clock::time_point              │
└────────────────────────────────────────────────────────┘
```

### Deterministic Lifecycle State Machine

A tracked window undergoes deterministic, validated lifecycle transitions:

```text
       ┌──────────────┐
       │  Discovered  │
       └──────┬───────┘
              │ (Surface/toplevel created)
              ▼
       ┌──────────────┐
       │ Initializing │
       └──────┬───────┘
              │ (Configure event received and acknowledged)
              ▼
       ┌──────────────┐
       │    Ready     │◄─────────┐
       └──────┬───────┘          │ (Unmapped / Hidden)
              │ (wl_surface committed / buffer attached)
              ▼                  │
       ┌──────────────┐          │
       │   Visible    ├──────────┘
       └──────┬───────┘
              │ (Close requested or toplevel unmapped)
              ▼
       ┌──────────────┐
       │   Closing    │
       └──────┬───────┘
              │ (Resource destroyed)
              ▼
       ┌──────────────┐
       │  Destroyed   │
       └──────────────┘
```

Invalid transitions are strictly rejected and logged. Any failure transition directs the window to `Failed` or `Destroyed`.

---

## 3. Centralized Window Registry

The `ldde::window::WindowRegistry` provides thread-safe, centralized ownership of all live window instances:

- **Lookups**: `lookup(WindowId)`, `find_by_surface(wl_surface*)`, `find_by_toplevel(xdg_toplevel*)`.
- **Enumeration**: `windows()` preserves deterministic creation order; `windows_for_app(app_id)` queries windows belonging to an application.
- **Active Window Tracking**: `set_active_window(id)` automatically unsets `is_active` on the previously active window, sets `is_active` on the new active window, and dispatches corresponding `FocusChanged` events. On window removal, the registry automatically activates the most recent remaining window.
- **Observer Pattern**: Typed listeners receive `WindowEvent` notifications synchronously upon every state change.

---

## 4. Typed Window Event Subsystem

All mutations emit strongly typed `WindowEvent` structs:

```text
enum class WindowEventType {
    Created,            // New window registered
    TitleChanged,       // xdg_toplevel.set_title
    AppIdChanged,       // xdg_toplevel.set_app_id
    GeometryChanged,    // xdg_surface.set_window_geometry or configure
    StateChanged,       // Normal, Maximized, Fullscreen, Minimized
    FocusChanged,       // Window gained or lost focus
    VisibilityChanged,  // Window mapped (visible) or unmapped
    ParentChanged,      // Transient relationship updated
    Closed,             // Close requested by compositor or client
    Destroyed           // Window resources released
};
```

---

## 5. Wayland Protocols & Dual Tracking Architecture

`WindowTracker` provides complete tracking support through two complementary mechanisms:

```text
                   ┌───────────────────────────────────┐
                   │       Weston (Compositor)         │
                   └───────────────┬───────────────────┘
                                   │
                     Client-side   │ xdg_wm_base (v5)
                     binding       │
                                   ▼
                   ┌───────────────────────────────────┐
                   │        LDDE WindowTracker         │
                   │  ┌─────────────────────────────┐  │
                   │  │       WindowRegistry        │  │
                   │  └─────────────────────────────┘  │
                   └───────────────▲───────────────────┘
                                   │
                     Server-side   │ wayland-ldde-apps
                     socket        │ (wl_compositor + xdg_wm_base)
                                   │
                   ┌───────────────┴───────────────────┐
                   │     External Wayland Clients      │
                   └───────────────────────────────────┘
```

### 1. Compositor Client Binding (Weston xdg_wm_base)
- Discovers and binds Weston's `xdg_wm_base` (negotiated up to protocol version 5).
- Supports `create_tracked_window(wl_surface*, title, app_id)` for client-side surfaces running on Weston.
- Automatically listens for:
  - `xdg_wm_base.ping` -> sends `xdg_wm_base.pong`.
  - `xdg_toplevel.configure` -> updates state (`Maximized`, `Fullscreen`, `Activated`), updates dimensions.
  - `xdg_surface.configure` -> acks configure serial, transitions window from `Initializing` to `Ready` and `Visible`.
  - `xdg_toplevel.close` -> transitions window to `Closing` and notifies registry.

### 2. Application Tracking Server Endpoint (`wayland-ldde-apps`)
- Uses `libwayland-server` to host a dedicated Wayland tracking endpoint socket (default: `wayland-ldde-apps` in `$XDG_RUNTIME_DIR`).
- Advertises `wl_compositor` and `xdg_wm_base` globals.
- Unmodified Wayland client applications connect directly to the socket.
- Handles:
  - `wl_surface.create_surface`, `wl_surface.commit`.
  - `xdg_wm_base.get_xdg_surface`.
  - `xdg_surface.get_toplevel` -> allocates stable `WindowId`, creates tracked `Window`, transitions to `Initializing`, and emits configure event.
  - `xdg_surface.set_window_geometry` -> updates window geometry bounds.
  - `xdg_surface.ack_configure` -> records client acknowledgement.
  - `xdg_toplevel.set_title` -> updates window title.
  - `xdg_toplevel.set_app_id` -> updates application identifier.
  - `xdg_toplevel.set_parent` -> links parent/child transient windows.
  - `xdg_toplevel.set_maximized` / `unset_maximized` -> updates state.
  - `xdg_toplevel.set_fullscreen` / `unset_fullscreen` -> updates state.
  - `xdg_toplevel.set_minimized` -> updates state.
  - Resource destruction -> transitions to `Closing` -> `Destroyed` and unregisters window.

---

## 6. Single-Threaded Event-Driven Main Loop Integration

The window tracker operates strictly within LDDE's single-threaded, non-blocking event loop:
1. Weston compositor fd is polled via `epoll` in `EventLoop` (`read_events`, `dispatch_pending`, `flush`).
2. Application tracking server fd (`server_fd()`) is polled via `epoll` in `EventLoop` (`dispatch_server`).
3. No threads are spawned in production; all Wayland protocol dispatches occur synchronously on the main thread, guaranteeing strict ordering and race-free window state updates.

---

## 7. Verification & Test Suite

The window tracking subsystem is verified by:
1. **64 automated unit tests**:
   - `test_window_model.cpp`: Identifiers, geometry, size, state transitions, focus, visibility, lifecycle transitions.
   - `test_window_registry.cpp`: Registration, lookups, application filtering, ordering, active window management.
   - `test_window_events.cpp`: Event dispatching for all 10 event types, multi-listener registration/removal.
   - `test_transient_windows.cpp`: Parent/child relationships, reparenting, detaching, destruction ordering.
2. **Real Weston integration tests** (`test_weston_integration.cpp`):
   - `ConnectDiscoverAndShutdown`: Capability discovery and shell initialization under headless Weston.
   - `WestonTrackedWindowLifecycle`: Weston `xdg_wm_base` binding, real client surface creation, commit/configure/ack lifecycle, destruction.
   - `RealExternalWaylandAppTracking`: Real external Wayland application connecting to `wayland-ldde-apps`, creating real `xdg_toplevel`, updating title/app_id, maximizing/unmaximizing, committing, and clean destruction.
