# LDDE D8 — Dock Subsystem Architecture & Implementation

## 1. Overview

The **LDDE Dock** is a production-grade, Linux-native, mobile-first desktop dock subsystem for the LinuxDroid Desktop Environment (LDDE). It sits inside the LDDE Shell alongside the Launcher (D7), Window Manager (D3), Window Registry (D2), Display Policy (D4), and Touch Interaction Manager (D5).

The Dock provides:
* **Instant Application Access**: Displays pinned applications configured via Linux configuration files (`ldde.conf`).
* **Authoritative Running State**: Tracks running applications and their active/minimized states strictly derived from D2 `WindowRegistry` and D3 `WindowManager` events. No process table polling, `/proc` scraping, or shell commands are used.
* **Application vs. Window Separation**: Groups multiple open windows belonging to the same application under a single unified dock item with window count badges.
* **Integrated Launcher Access**: Dedicated launcher button pinned at the leading edge of the dock calling `Launcher::toggle()` seamlessly.
* **Direct Window & Application Lifecycle Controls**:
  - Tapping an unrunning application launches it via the non-shell `ApplicationLauncher` backend.
  - Tapping a running background or minimized application activates and restores it via `WindowManager`.
  - Tapping the currently active application minimizes it to clear screen space.
* **Responsive Cairo Rendering**: Rendered directly onto the shell's `DockRegion` `ShmBuffer` with hardware-neutral Cairo vector graphics, glowing indicators, running dots, and minimized visual states.
* **Responsive Mobile Touch Layout**: Dynamic geometry adhering to D4 `DisplayPolicy` metrics, guaranteeing $\ge 48\,\text{dp}$ touch targets, safe area avoidance, and smooth horizontal scrolling when item count exceeds display width.

---

## 2. Architectural Position

```text
Android LinuxDroid App
        ↓
LinuxDroid Runtime
        ↓
Guest Init
        ↓
LDDM
        ↓
Weston
        ↓
LDDE
 ┌───────────────────────────────────────┐
 │ Shell (DesktopSurface, Status, Dock)  │
 │   ├─ DockRegion (ShmBuffer / Cairo)   │
 │   │    └─ DockView                    │
 │   ├─ Dock (Facade)                    │
 │   │    ├─ DockStateMachine            │
 │   │    ├─ DockModel                   │
 │   │    ├─ DockLayout                  │
 │   │    └─ DockController              │
 │   ├─ Launcher (D7)                    │
 │   ├─ Window Manager (D3)              │
 │   ├─ Window Registry (D2)             │
 │   ├─ Touch Interaction (D5)           │
 │   └─ Display Policy (D4)              │
 └───────────────────────────────────────┘
        ↓
Linux Applications (via nested wayland-ldde-apps socket)
```

---

## 3. Subsystem Components

The D8 Dock subsystem is organized under `include/ldde/dock/` and `src/dock/`:

### 3.1. Dock State Machine (`DockStateMachine`)
A deterministic state machine governing dock visibility transitions:
* States: `Hidden`, `Showing`, `Visible`, `Hiding`.
* Synchronous and animation-friendly transitions with transition hooks.
* Respects `dock.visibility` and `dock.enabled` configuration flags.

### 3.2. Dock Item (`DockItem`)
The presentation model for items rendered in the dock:
* Tracks `ApplicationId`, display name, executable path, and icon reference.
* Tracks pinned state (`is_pinned`).
* Tracks running state (`is_running`), active state (`is_active`), and minimized state (`is_minimized`).
* Maintains a collection of associated `WindowId`s for multi-window applications.
* Flags availability (`is_available`), allowing pinned items whose desktop entries are missing to remain visible without crashing.

### 3.3. Dock Model (`DockModel`)
Synchronizes state across LDDE subsystems:
* **D6 Application Catalog**: Resolves desktop metadata, localized names, executables, and icons.
* **D2 Window Registry & D3 Window Manager**: Listens to window lifecycle events (`Created`, `Destroyed`, `StateChanged`, `FocusChanged`). Automatically updates item running/active/minimized statuses.
* **Pinned Applications**: Persists and loads pinned items from configuration strings (comma or semicolon delimited). Supports dynamic pinning and unpinning.
* **Unpinned Running Applications**: Automatically adds items for unpinned applications when their windows open, and removes them when all windows close.

### 3.4. Dock Layout (`DockLayout`)
Calculates responsive dock geometry and performs touch hit-testing:
* Computes touch targets conforming to $\ge 48\,\text{dp}$ mobile interaction standards.
* Places the dedicated Launcher button at the leading edge (`x = padding`).
* Arranges dock application items sequentially with configurable spacing.
* Computes horizontal overflow bounds (`max_scroll_x`) and clamps scrolling safely when items exceed display width.
* Performs hit-testing returning `DockHitType::LauncherButton`, `DockHitType::Item`, or `DockHitType::None`.

### 3.5. Dock View (`DockView`)
Renders the dock using Cairo 2D vector graphics into the shared memory buffer:
* **Background Pill**: Rounded floating pill container with configurable background fill and border.
* **Launcher Button**: 3x3 rounded grid icon symbolizing the application launcher.
* **Application Icons**: Resolves application icons via the icon resolver, falling back to a clean rounded-rect vector badge with lettermark.
* **State Badges**:
  - **Running indicator**: Centered circular dot beneath running application items.
  - **Active indicator**: Glowing elongated pill beneath the focused application.
  - **Window count badge**: Small numerical badge for applications with $> 1$ windows.
  - **Minimized styling**: Semi-transparent rendering for fully minimized applications.

### 3.6. Dock Controller (`DockController`)
Dispatches user input into application actions:
* **Touch Events**: Translates tap gestures into launch, focus, restore, or minimize commands. Supports horizontal touch dragging for scrolling overflow.
* **Pointer Events**: Handles mouse hover states and wheel axis scrolling.
* **Keyboard Navigation**: Supports arrow key traversal, Enter/Space activation, and Escape dismiss.
* **Launcher Button**: Calls `Launcher::toggle()` directly.

---

## 4. State Flow Matrix

| Application Status | Dock Interaction | Action Performed | Subsystem Delegated To |
| :--- | :--- | :--- | :--- |
| **Not running** (pinned) | Tap | Launch application | `ApplicationLauncher::launch` |
| **Running, Unfocused** | Tap | Focus & raise window (restore if minimized) | `WindowManager::restore` & `activate` |
| **Running, Focused** | Tap | Minimize active window | `WindowManager::minimize` |
| **Running, Multi-Window** | Tap | Focus next window in application stack | `WindowManager::activate` |
| **Launcher Button** | Tap | Open or close launcher | `Launcher::toggle` |

---

## 5. Architectural Verification & Compliance

1. **Zero Process Polling**:
   Running states are exclusively updated via `WindowRegistry` listener callbacks triggered by Wayland protocol events on the nested tracking socket.
2. **Zero Shell Execution**:
   Applications are executed via `fork`/`execvp` using structured argument vectors in `LinuxSessionApplicationLauncher`. `/bin/sh -c` is forbidden.
3. **Safe Desktop Disconnections**:
   Uninstalled or missing pinned applications are rendered gracefully as disabled or fallback badges without crashes or broken state.
4. **Wayland & Cairo Decoupling**:
   Cairo drawing operations render into client-side `ShmBuffer` allocations attached via `DockRegion::set_render_callback`, maintaining strict separation between Wayland protocol handling and UI drawing.
