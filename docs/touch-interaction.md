# LDDE D5 — Touch Window Interaction Architecture

## 1. Overview

The **LDDE Touch Window Interaction subsystem (D5)** provides the primary interaction model for the LinuxDroid Desktop Environment on mobile Linux displays. D5 makes the existing D3 Window Manager genuinely usable on mobile touchscreens (such as PinePhone Pro, Librem 5, Fairphone, and ARM64 Linux mobile devices) without requiring a mouse or physical keyboard.

D5 bridges the native Wayland touch event stream (D0) to desktop window management operations (D3), guided by mobile display policy (D4) and window tracking records (D2).

```text
Wayland Input Protocol (wl_touch / wl_seat)
                 ↓
      D0 Input Subsystem (Touch & Seat)
                 ↓
     D5 Touch Interaction Manager
  ┌──────────────┼──────────────┐
  ▼              ▼              ▼
TouchHitTest  GestureSM   Controllers (Drag/Resize/Controls)
                 ↓
     D3 Window Manager Subsystem
                 ↓
      WindowManagementBackend
                 ↓
        Weston Compositor
```

---

## 2. Core Invariants & Mobile-First Principles

1. **Native Linux Desktop Model**:
   - D5 operates strictly within the Wayland desktop model (xdg-shell, wl_touch).
   - Zero Android APIs, zero Android system calls, no fake Android surfaces, and no PRoot layers.
   - All managed windows are genuine Wayland client application windows.

2. **No Compositor or Registry Duplication**:
   - Weston remains the authoritative compositor responsible for surface rendering, buffer exchange, and compositor-level input routing.
   - `WindowRegistry` (D2) remains the sole authoritative registry of active windows and z-order stacking.
   - `WindowManager` (D3) remains the sole authority executing window transformations, focus changes, and state transitions.
   - `DisplayPolicy` (D4) remains the sole authority for screen geometry, usable insets, orientation, and responsive layout metrics.

3. **ARM64 Native Performance & Efficiency**:
   - Zero dynamic heap allocations during high-frequency motion event streaming (`handle_touch_motion`).
   - Flat cache-friendly structures and fixed-width signed integers (`int32_t`) for coordinate math, avoiding platform-dependent arithmetic issues.
   - Strict avoidance of `char` signedness assumptions (ARM64 defaults `char` to `unsigned char`).

4. **Touch Ergonomics & Human Interface Guidelines**:
   - Large, finger-friendly touch targets: title bar height (40–44px) and control buttons (44–48px) prevent mis-taps.
   - Generous resize touch targets: 20–28px perimeter grab margins for 8-directional window resizing, contrasted with fine 6px pointer margins.
   - Slop-tolerant double-tap maximize detection (within 350ms and 16px distance).

---

## 3. Subsystem Architecture

D5 is organized into focused, single-responsibility components:

```text
                                  ┌───────────────────────────────┐
                                  │    TouchInteractionManager    │ (Subsystem Coordinator)
                                  └───────────────┬───────────────┘
                 ┌───────────────────┬────────────┴────────────┬───────────────────┐
                 ▼                   ▼                         ▼                   ▼
       ┌──────────────────┐ ┌──────────────────┐     ┌──────────────────┐ ┌──────────────────┐
       │TouchHitTesting   │ │TouchGestureState │     │ WindowDragCtrl   │ │ WindowResizeCtrl │
       └──────────────────┘ └──────────────────┘     └──────────────────┘ └──────────────────┘
                 │                                             │                   │
                 ▼                                             ▼                   ▼
       ┌──────────────────┐                          ┌──────────────────────────────────────┐
       │ WindowControl    │                          │          D3 WindowManager            │
       │   Interaction    │                          │ (activate, move, resize, max, close) │
       └──────────────────┘                          └──────────────────────────────────────┘
```

### 3.1 TouchInteractionPolicy

`TouchInteractionPolicy` encapsulates all dimensional metrics, timing thresholds, and behavioral flags required for touch interaction. It is dynamically derived from `Config` and responsive `DisplayPolicy` metrics:

- `touch_enabled`: Master enable switch for touch window interaction.
- `move_threshold_px`: Distance (default 10px) contact must move to commit from pending tap to interactive drag.
- `double_tap_interval_ms`: Maximum time delta (default 350ms) between taps for double-tap gestures.
- `double_tap_slop_px`: Maximum spatial distance (default 16px) between taps.
- `control_touch_target_px`: Touch target width for window controls (44–48px based on layout class).
- `resize_touch_target_px`: Width of edge and corner resize hit zones (20–28px).
- `header_touch_height_px`: Title bar touch interaction height (40–44px).

### 3.2 TouchGestureStateMachine

`TouchGestureStateMachine` implements a deterministic 8-state model that governs touch life-cycles and prevents invalid or conflicting operations:

```text
               ┌──────────┐
               │   Idle   │
               └────┬─────┘
                    │ Touch Down
                    ▼
          ┌───────────────────┐
          │  ContactPending   │
          └───┬───────┬───────┘
  Touch Up    │       │ Motion > threshold
  (Tap Action)│       │
      ┌───────┘       ├───────────────────┐
      ▼               ▼                   ▼
┌───────────┐   ┌───────────┐       ┌───────────┐
│WindowFocus│   │  Moving   │       │ Resizing  │
└─────┬─────┘   └─────┬─────┘       └─────┬─────┘
      │               │                   │
      │ Touch Up      │ Touch Up          │ Touch Up
      ▼               ▼                   ▼
┌───────────┐   ┌───────────┐       ┌───────────┐
│ Completed │   │ Completed │       │ Completed │
└─────┬─────┘   └─────┬─────┘       └─────┬─────┘
      │               │                   │
      └───────────────┼───────────────────┘
                      ▼
                 ┌──────────┐
                 │   Idle   │
                 └──────────┘
```

When touching a title bar button, the state transitions to `ControlPress`. If the touch drifts outside the button bounds before release, the state transitions to `GestureCancelled`, preventing unintended activations.

### 3.3 TouchHitTesting

`TouchHitTesting` identifies the target window and specific component under a screen coordinate $(x, y)$:

1. **Top-Level Precedence**: If a touch strikes an active shell overlay (e.g. app launcher), window interactions are suppressed.
2. **Reverse Z-Order Traversal**: Scans visible, non-minimized windows from topmost to bottommost according to `WindowRegistry::visible_stack()`.
3. **Window Hit Regions**:
   - **Window Controls**: Evaluates Close, Maximize, and Minimize buttons using expanded `control_touch_target_px` bounds.
   - **Resize Perimeter**: Detects 8 resize directions (`TopLeft`, `Top`, `TopRight`, `Right`, `BottomRight`, `Bottom`, `BottomLeft`, `Left`) along outer window borders. Suppressed when window is maximized or fullscreen.
   - **Title Bar Drag Area**: Header region excluding control buttons, used for moving and double-tap toggling.
   - **Window Content**: Internal application client area. Touching content immediately focuses/activates the window without initiating window-management moves.

### 3.4 WindowDragController & WindowResizeController

- **WindowDragController**: Manages interactive window translation. Stores touch-down origin and initial window bounds. Upon motion beyond threshold, computes differential offsets $(\Delta x, \Delta y)$ and moves the window via `WindowManager::interactive_move()`, clamping positions within display margins. If cancelled, cleanly reverts to initial geometry.
- **WindowResizeController**: Manages 8-way interactive window resizing. Modifies width, height, and origin according to the active `ResizeEdge`. Enforces minimum window dimensions (`min_window_width_px`, `min_window_height_px`) to prevent collapsed or unrecoverable windows.

### 3.5 WindowControlInteraction

Tracks touch interactions with window control buttons:
- **Close**: Calls `WindowManager::close(window_id)`, transmitting `xdg_toplevel.close` to the application client.
- **Maximize / Restore**: Toggles window state between `Normal` and `Maximized` via `WindowManager`.
- **Minimize**: Calls `WindowManager::minimize(window_id)`, hiding the window and automatically falling back focus to the next visible window in the stack.
- **Visual Feedback & Cancellation**: Emits press state on touch down. Cancelling or dragging outside the button boundary discards the action without trigger.

---

## 4. Robustness & Dynamic Conditions

1. **Multi-Touch Contact Ownership**:
   The first touch contact to initiate an interaction acquires exclusive ownership (`active_touch_id_`). Subsequent contacts are ignored for window manipulation, eliminating jitter from palm touches or accidental multi-finger contacts.

2. **Window Destruction During Interaction**:
   If a client window terminates or crashes while being actively dragged or resized, `TouchInteractionManager::handle_window_destroyed()` instantly resets the interaction state to `Idle`, clears active controllers, and avoids dangling pointer references.

3. **Display Orientation & Layout Changes**:
   When the display rotates or changes resolution during an active touch manipulation, `handle_display_change()` aborts the gesture, safely restores window geometry, re-evaluates responsive metrics, and returns the state machine to `Idle`.

4. **Compositor Touch Cancel**:
   Wayland `wl_touch.cancel` events immediately cancel active operations, restoring the window's pre-interaction geometry without residual drag state.

---

## 5. Configuration Reference

Touch interaction settings are configured in the `[input]` section of `ldde.conf`:

| Setting | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `touch_enabled` | bool | `true` | Master toggle for touch window interaction |
| `touch_move_threshold` | int | `10` | Pixels of movement required before committing to move drag |
| `touch_double_tap_timeout` | int | `350` | Maximum time between taps for double-tap detection (ms) |
| `touch_double_tap_distance` | int | `16` | Maximum distance slop between taps (pixels) |
| `touch_resize_target` | int | `28` | Touch hit target margin for resize grab edges (pixels) |

---

## 6. Verification and Validation

The D5 implementation is verified through two layers of automated testing:

1. **Unit Test Suite (`ldde_unit_tests`)**:
   - `TouchGestureStateTest`: State transitions, naming, and invalid transition rejection.
   - `TouchInteractionPolicyTest`: Metric derivation from configuration and display policies.
   - `TouchHitTestingTest`: Stacking order priority, control button detection, fullscreen/maximized edge suppression.
   - `TouchInteractionTest`: Focus switching, tap vs drag arbitration, touch-to-move, 8-way touch-to-resize, double-tap maximize/restore, button press and drag-out cancel, window destruction handling, and multi-touch isolation.
   - **Result**: 134/134 passing tests (100%).

2. **Weston Wayland Integration Test (`ldde_integration_tests`)**:
   - `WestonRealTouchWindowInteraction`: Spawns two real Wayland application clients running on an isolated Weston headless server.
   - Validates live touch focus switching, touch dragging, corner resizing, double-tap title bar maximize and restore, minimize with focus fallback, close button protocol signaling, and rotation adaptation during active drag.
   - **Result**: 6/6 passing tests (100%).

