# LDDE D3 — Production Window Manager Architecture

## 1. Overview

The **LDDE Window Manager (D3)** builds directly on top of D0 Foundation, D1 Wayland Shell, and D2 Real Window Tracking. While D2 established authoritative discovery, tracking, and lifecycle tracking of real Wayland application windows, D3 implements the complete **desktop window-management policy and user interaction layer**.

The architecture preserves the strict frozen layering:

```text
Linux Applications (e.g. org.gnome.Calculator, org.gnome.Calendar)
       ↓
Wayland Protocol (xdg-shell)
       ↓
Weston (Compositor)
       ↓
Window-management integration / protocol bridge (wayland-ldde-apps)
       ↓
LDDE WindowManager
```

---

## 2. Core Responsibilities & Invariants

1. **Policy vs. Compositing Separation**:
   - Weston remains the compositor responsible for surface presentation, buffer sharing, and hardware output.
   - LDDE implements desktop environment policy: initial window placement, active focus management, z-order stacking, window decorations / controls, interactive moving, 8-directional resizing, maximization, minimization, fullscreen toggling, and multi-window lifecycle coordination.
2. **Zero Synthetic Windows**:
   - LDDE never invents windows or surfaces. Every managed window is bound to a verified Wayland client surface tracked by `WindowTracker`.
3. **Zero Process Launching in WM Subsystem**:
   - No `fork`, `exec`, PRoot, or Android runtime commands exist within the WindowManager. Window management operates strictly over Wayland protocol streams.
4. **ARM64 Native Portability**:
   - All arithmetic and coordinate types use fixed-width signed integers (`int32_t`) and unsigned indices (`uint32_t`, `size_t`).
   - No `char` signedness assumptions (ARM64 defaults `char` to `unsigned char`).
   - Touch hit margins comply with Android / mobile HIG: minimum 44–48dp touch targets on headers, close/maximize/minimize buttons, and edge grab zones (24px for touch vs. 6px for fine pointer).
5. **Deterministic Single-Threaded Event Loop**:
   - All window manager state mutations, geometry recalculations, and Wayland dispatching execute deterministically on the main event loop thread without lock contention.

---

## 3. Subsystem Architecture

The Window Manager subsystem is composed of modular, decoupled components:

```text
                                  ┌───────────────────────────┐
                                  │      WindowManager        │ (Coordinator)
                                  └─────────────┬─────────────┘
                ┌───────────────────┬───────────┴───────────┬───────────────────┐
                ▼                   ▼                       ▼                   ▼
      ┌──────────────────┐ ┌──────────────────┐   ┌──────────────────┐ ┌──────────────────┐
      │ WindowPlacement  │ │WindowStateCtrl   │   │   WindowFocus    │ │  WindowStacking  │
      └──────────────────┘ └──────────────────┘   └──────────────────┘ └──────────────────┘
                ▼                   ▼                       ▼                   ▼
      ┌──────────────────┐ ┌──────────────────┐   ┌──────────────────────────────────────┐
      │ WindowControls   │ │WindowInteraction │   │      WindowManagementBackend         │
      └──────────────────┘ └──────────────────┘   │     (Default / Mock / Test)          │
                                                  └──────────────────────────────────────┘
```

### 3.1 `WindowPlacement`
- **Mobile-First Orientation Awareness**:
  - **Portrait (Phone screens, e.g. 1080×2400)**:
    - Calculates safe-area usable bounds excluding top status bar (40px + safe margins) and bottom dock (68px + safe margins).
    - Windows sized to 88% usable width and 60% usable height.
    - Centered horizontally, vertical cascading offset (24px) for subsequent windows.
  - **Landscape (Tablet / Desktop / Rotated phone, e.g. 1920×1080)**:
    - Sized to 65% width and 70% height.
    - Diagonal cascading (`cascade_step = 32px`).
  - **Client-Specified Size**:
    - Respects client requested size clamped to usable screen boundaries.
  - **Header Accessibility Clamping**:
    - Ensures that regardless of screen resizing, rotation, or dragging, the window's titlebar header always remains reachable and visible (minimum 36px).

### 3.2 `WindowStateController`
- Manages discrete window state transitions:
  - `Normal` (Floating): Window has movable/resizable geometry.
  - `Maximized`: Sized to exact usable desktop bounds (excluding status bar and dock). Preserves previous floating geometry in `saved_geometry_`.
  - `Fullscreen`: Occupies 100% of the display display output (covering status bar and dock).
  - `Minimized`: Logically hidden from the visible stack; automatically transfers focus to the next topmost visible window. Restoring returns to previous geometry.
- **Display Adaptation**:
  - Re-evaluates maximized and fullscreen windows upon screen rotation or display resolution changes.

### 3.3 `WindowFocus`
- Authoritative active window tracker.
- Raises activated window to top of the stacking order.
- Deactivates previous window and triggers `WindowEventType::FocusChanged` events.
- **Automatic Focus Fallback**:
  - When the active window is closed, destroyed, or minimized, automatically queries the topmost visible window in `WindowStacking` and activates it.

### 3.4 `WindowStacking`
- Maintains z-order stack from bottom (index 0) to top (index back).
- **Hierarchical Transient Dialogs**:
  - Recognizes parent-child dialog hierarchies.
  - Raising a parent window automatically raises its transient child dialogs above it.
  - Prevents a child dialog from dropping below its parent in z-order.
- Provides `visible_stack(registry)` filtering out minimized or hidden windows.

### 3.5 `WindowInteraction`
- **Interactive Move**:
  - Tracks drag sessions with pointer or touch.
  - Clamps window position in real time so titlebars never leave the usable screen bounds.
  - Commits geometry on pointer/touch release; supports cancel/revert.
- **Interactive 8-Direction Resize**:
  - Supports `Top`, `Bottom`, `Left`, `Right`, `TopLeft`, `TopRight`, `BottomLeft`, `BottomRight`.
  - Enforces client `min_size` and `max_size` constraints.
  - Distinguishes pointer (6px edge threshold) vs touch (24px touch grab margin) for mobile touch precision.

### 3.6 `WindowControls`
- Hit testing for window headers:
  - **Close Button**: Dispatches close request to the client.
  - **Maximize/Restore Button**: Toggles maximized state.
  - **Minimize Button**: Logically minimizes window.
  - **Title Drag Area**: Initiates window move session.
  - **Double-Tap / Double-Click**: Toggles maximize/restore on the titlebar.
- Sized with 48dp touch targets on mobile for touch accessibility.

### 3.7 `WindowManagementBackend`
- Decouples window management logic from protocol transport.
- Dispatches `xdg_toplevel` configure states (activated, maximized, fullscreen), surface sizes, and close requests to real Wayland clients via `WindowTracker`.
- Flushes Wayland server client buffers immediately to guarantee real-time response.

---

## 4. Verification & Testing

### 4.1 Unit Test Coverage (`tests/unit/`)
1. `test_window_placement.cpp`: Usable area calculations, portrait safe-area margins and vertical cascade, landscape diagonal cascade, clamping rules.
2. `test_window_state_controller.cpp`: Maximize/restore, fullscreen/restore, minimize/restore, geometry preservation, display rotation adaptation.
3. `test_window_focus.cpp`: Initial state, activation, switching, raising on focus, fallback upon close or minimization.
4. `test_window_stacking.cpp`: Add, remove, raise, lower, transient dialog hierarchy preservation, minimized window filtering.
5. `test_window_interaction.cpp`: 8-way resize edge detection, touch vs pointer grab margins, interactive move, min/max resize constraints, cancel reverting.
6. `test_window_controls.cpp`: Header button hit tests (close, max, min, drag area), mobile touch target sizing (>= 48dp), double-tap detection.
7. `test_window_manager.cpp`: Master coordinator end-to-end integration, event listeners, input gesture delegation, multi-window coordination.

Total Unit Tests: **103 / 103 Passed**.

### 4.2 Real Weston Integration Testing (`tests/integration/test_weston_integration.cpp`)
- Real headless Weston 13.0.0 instance.
- **Multi-Window Integration Test (`RealMultiWindowManagement`)**:
  - Two simultaneous real Wayland application clients (`org.gnome.Calculator` and `org.gnome.Calendar`) communicating over Wayland UNIX domain sockets.
  - Dynamic discovery and registration into WindowRegistry.
  - Independent focus and stacking verification (window 2 raised above window 1).
  - WindowManager maximize operation with real configure bounds.
  - WindowManager minimize operation with automatic focus fallback to window 1.
  - WindowManager restore operation restoring visibility and geometry.
  - WindowManager close request dispatching `xdg_toplevel_send_close` to real client.
  - Clean, deterministic shutdown and socket cleanup.

Total Integration Tests: **4 / 4 Passed**.
Zero memory leaks, zero compiler warnings under GCC 13 with `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror=return-type`.
