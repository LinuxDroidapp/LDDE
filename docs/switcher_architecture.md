# LDDE Application Switcher Subsystem (D9)

## 1. Overview & Architectural Position

The **Application Switcher (D9)** provides a production-grade, Linux-native, mobile-first task and application switching experience for the LinuxDroid Desktop Environment (LDDE). It enables users on touch displays, physical keyboards, and pointing devices to rapidly navigate and switch between currently running Linux applications and their windows while strictly preserving normal Linux window behavior.

The switcher operates as an integrated shell component positioned alongside the Launcher (D7) and Dock (D8):

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
 ┌──────────────────────────────────────┐
 │ Shell                                │
 │ Launcher       ← D7                  │
 │ Dock           ← D8                  │
 │ Switcher       ← D9                  │
 │ Window Manager ← D3                  │
 │ Window Registry← D2                  │
 │ Application Catalog ← D6             │
 │ Display Policy ← D4                  │
 └──────────────────────────────────────┘
        ↓
Linux Applications
```

### Core Architectural Guarantees
1. **Authoritative State Consumption**: The switcher consumes application and window state directly from D2 (`WindowRegistry`), D3 (`WindowManager`), and D6 (`ApplicationCatalog`). It does **not** maintain a secondary window management authority, does **not** poll `/proc` or `pgrep`, and does **not** invoke shell scripts.
2. **Deterministic Focus-Driven MRU**: Window and application recency is tracked in-memory by listening directly to focus change events dispatched by the `WindowRegistry`.
3. **Multi-Window Grouping & Transients**: In `Application` presentation mode (default), multiple windows with the same application identifier are grouped into a single switcher item displaying a window count badge, with the most recently focused window serving as the primary activation target. Transient child dialogs are associated with their parent application.
4. **Handoff to WindowManager**: Switching is executed strictly through D3 `WindowManager` (`restore()` if minimized, followed by `activate()`).
5. **Dynamic Resilience**: Closes, minimizes, state changes, or sudden window destruction (e.g. process termination) are processed reactively without switcher crashes, memory leaks, or stale selections.

---

## 2. Component Architecture

The Switcher subsystem (`include/ldde/switcher/`, `src/switcher/`) is composed of clean, decoupled classes adhering to the Single Responsibility Principle:

```text
┌─────────────────────────────────────────────────────────────┐
│                          Switcher                           │
│                      (Public Facade)                        │
└───────┬─────────────┬─────────────┬─────────────┬───────────┘
        │             │             │             │
        ▼             ▼             ▼             ▼
┌──────────────┐┌──────────────┐┌──────────────┐┌─────────────┐
│ SwitcherState││SwitcherModel ││SwitcherLayout││ SwitcherView│
│ (State Mach.)││ (Aggregation)││  (Geometry)  ││ (Rendering) │
└───────▲──────┘└───────▲──────┘└───────▲──────┘└─────────────┘
        │               │               │
        └───────────────┼───────────────┘
                        │
                ┌───────┴──────────┐
                │SwitcherController│
                │(Input & Navig.)  │
                └──────────────────┘
```

### 2.1 Component Responsibilities

- **`SwitcherStateMachine`** (`switcher_state.hpp`):
  Manages strict state lifecycle: `Closed` $\to$ `Opening` $\to$ `Open` $\to$ `Selecting` $\to$ `Activating` $\to$ `Closing` $\to$ `Closed`. Guarantees thread-safe transitions and prevents invalid concurrent operations.
- **`SwitcherItem`** (`switcher_item.hpp`):
  Represents a single switchable entry (application group or individual window). Encapsulates `ApplicationId`, primary `WindowId`, list of all associated window IDs, display name, icon name/path, current focus status (`is_current`), selected state (`is_selected`), and minimized status (`is_minimized`).
- **`SwitcherMru`** (`switcher_mru.hpp`):
  Maintains in-memory Most Recently Used ordering for window IDs and application IDs. Automatically prunes records when windows are destroyed.
- **`SwitcherModel`** (`switcher_model.hpp`):
  Aggregates running windows from `WindowRegistry`, correlates them with `.desktop` metadata from `ApplicationCatalog`, applies transient window hierarchy rules, computes MRU sorting, and notifies listeners on change. Supports both `Application` (grouped) and `Window` (individual) presentation modes.
- **`SwitcherLayout`** (`switcher_layout.hpp`):
  Calculates responsive geometry based on D4 `DisplayPolicy` (portrait vs landscape), display scaling, safe area insets (avoiding status bar, dock, and display cutouts), minimum touch targets ($\ge 48\,\text{dp}$), and card grid/row positions.
- **`SwitcherView`** (`switcher_view.hpp`):
  Renders the switcher overlay into `ShmBuffer` using double-buffered Cairo vector graphics. Features a dark translucent backdrop/scrim, rounded switcher cards, active/current/selected visual cues, application icon rasterization/fallbacks, window count badges, and empty-state placeholders.
- **`SwitcherController`** (`switcher_controller.hpp`):
  Handles all user interactions: Touch gestures (tap card to activate, drag/swipe to scroll, tap outside to dismiss), Pointer clicks, and Keyboard navigation (Tab/Shift+Tab, Arrow keys, Enter to activate, Esc to dismiss).
- **`Switcher`** (`switcher.hpp`):
  Unified facade integrating the switcher into the LDDE `Application` shell lifecycle, overlay rendering, and Wayland compositor loop.

---

## 3. MRU Ordering & Fast Switching

### Alt+Tab / Fast-Switch Paradigm
When the user invokes the switcher (via shortcut, gesture, or API):
1. The currently focused application (MRU index 0) is marked `is_current = true`.
2. If two or more applications exist, the switcher pre-selects MRU index 1 (`selected_index = 1`). This allows a single trigger/tap or release to immediately switch to the previous application without extra cycling.
3. If only one application exists, `selected_index = 0`.
4. Subsequent navigation moves the selection ring forward (`select_next`) or backward (`select_prev`).
5. Committing the selection activates the chosen application. If the switcher is cancelled (e.g. Esc or tap outside), the initial active window retains or reclaims focus.

---

## 4. Multi-Window Grouping & Transient Policy

### Grouping Rules (`SwitcherPresentationMode::Application`)
- Multiple top-level windows sharing the same `ApplicationId` (or mapped from `startup_wm_class` / executable) are collapsed into one `SwitcherItem`.
- The item displays a badge showing the number of open windows ($N \ge 2$).
- The primary window target for activation is the window within the group with the highest MRU focus rank.

### Transient Dialog Hierarchy
- Transient child surfaces and dialog windows that declare a `parent_id()` are not displayed as separate cards in `Application` mode.
- Instead, their existence is included in the parent application's window count, and activating the application activates the parent or its active transient top.

---

## 5. Mobile-First Layout & Touch Ergonomics

Derived dynamically from `DisplayPolicy`:

| Property | Portrait Phone | Landscape / Tablet |
| :--- | :--- | :--- |
| **Card Dimensions** | $220 \times 140\,\text{px}$ | $240 \times 160\,\text{px}$ |
| **Card Spacing** | $16\,\text{px}$ | $20\,\text{px}$ |
| **Orientation** | Horizontal scrollable strip / grid | Centered horizontal row |
| **Touch Targets** | Cards $\ge 48\,\text{dp}$, full card hit-testable | Cards $\ge 48\,\text{dp}$, full card hit-testable |
| **Safe Areas** | Insets avoid Status Bar ($40\,\text{px}$) & Dock ($68\,\text{px}$) | Insets avoid margins & cutouts |

---

## 6. Verification & Test Coverage

The subsystem is validated with comprehensive automated test suites:
- **Unit Tests (`test_switcher.cpp`)**: 18 tests covering `SwitcherState`, `SwitcherMru`, `SwitcherItem`, `SwitcherModel`, `SwitcherLayout`, `SwitcherController`, and `Switcher` facade.
- **Integration Tests (`test_switcher_integration.cpp`)**: 4 end-to-end scenarios covering multi-app discovery to MRU switching, restoring minimized applications, window destruction during active switcher display, and responsive display orientation adaptation.
- **Weston Integration Test (`test_weston_integration.cpp`)**: Verified with real Wayland client surfaces connected to the embedded Weston compositor.

