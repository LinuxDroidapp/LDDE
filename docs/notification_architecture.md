# LDDE D12 — Notification Subsystem Architecture

## 1. Overview & Purpose

The **LDDE Notification Subsystem** provides a production-grade, Linux-native notification service for the LinuxDroid Desktop Environment (LDDE). It fully complies with the Freedesktop.org Desktop Notifications Specification (`org.freedesktop.Notifications`), enabling native Linux applications (browsers, media players, messaging clients, package managers) to notify users seamlessly on mobile-first Wayland displays.

The subsystem operates under the unique conditions of mobile Linux environments running on Android via LinuxDroid:
- **Zero Android APIs**: Relies exclusively on standard Linux IPC and Wayland primitives.
- **Defensive Resource Bounding**: Implements strict in-memory limits, per-application flood protection, and automatic history pruning to prevent memory exhaustion in constrained container environments.
- **Mobile-First Ergonomics**: Features large touch targets ($\ge 48\,\text{dp}$), horizontal swipe-to-dismiss gestures ($\ge 40\,\text{px}$), outside-tap dismiss, and adaptive layouts tailored for portrait and landscape mobile screens.
- **Dual-Backend Flexibility**: Combines an asynchronous GDBus daemon for real Linux desktop applications with an in-memory internal backend for tests and system event notifications.

```text
 Linux Applications (Browsers, Chat, CLI notify-send)
                          ↓ D-Bus IPC (session bus)
 ┌──────────────────────────────────────────────────────────────┐
 │ LDDE Notification Subsystem (D12)                            │
 │                                                              │
 │   ┌───────────────────────┐      ┌───────────────────────┐   │
 │   │ DBusNotification      │      │ InternalNotification  │   │
 │   │ Backend               │      │ Backend               │   │
 │   └──────────┬────────────┘      └───────────┬───────────┘   │
 │              └─────────────┬─────────────────┘               │
 │                            ▼                                 │
 │                 ┌───────────────────────┐                    │
 │                 │  NotificationStore    │                    │
 │                 │  (Bounded, Flood Prot)│                    │
 │                 └──────────┬────────────┘                    │
 │                            ▼                                 │
 │                 ┌───────────────────────┐                    │
 │                 │ NotificationPresenter │                    │
 │                 │ (Popups & Auto-Dismiss│                    │
 │                 └──────────┬────────────┘                    │
 │                            │                                 │
 │         ┌──────────────────┴──────────────────┐              │
 │         ▼                                     ▼              │
 │  ┌───────────────────────┐         ┌───────────────────────┐ │
 │  │ NotificationLayout    │         │ NotificationCenter    │ │
 │  │ (Portrait / Landscape)│         │ State Machine         │ │
 │  └──────────┬────────────┘         └───────────┬───────────┘ │
 │             └──────────────────┬───────────────┘             │
 │                                ▼                             │
 │                    ┌───────────────────────┐                 │
 │                    │   NotificationView    │                 │
 │                    │   (Cairo Rendering)   │                 │
 │                    └───────────┬───────────┘                 │
 └────────────────────────────────┼─────────────────────────────┘
                                  ▼
               ShellOverlay Surface / Wayland Buffer
```

---

## 2. Architectural Components

The subsystem is implemented under `include/ldde/notification/` and `src/notification/`, organized into clean, single-responsibility modules:

### 2.1 Core Types, Model & Sanitization (`notification_types.hpp`, `notification.hpp`)

- **`NotificationId`**: Monotonically increasing 32-bit unsigned integer (`uint32_t`), starting at 1. `0` represents an invalid or unassigned ID.
- **`Urgency`**: Low (`0`), Normal (`1`), Critical (`2`). Critical notifications do not auto-dismiss on timer expiration.
- **`NotificationState`**: `Active`, `Displayed`, `Dismissed`, `Expired`, `Revoked`.
- **`CloseReason`**: `Expired` (`1`), `DismissedByUser` (`2`), `ClosedByCall` (`3`), `Undefined` (`4`).
- **Text Sanitization**:
  - `sanitize_text()` strips HTML tags (`<...>`), removes non-printable ASCII/control characters (except `\n` and `\t`), normalizes multiple consecutive whitespace characters, and clamps lengths:
    - Summary clamped to 256 characters.
    - Body clamped to 1024 characters.
- **Actions & Hints**:
  - Supports standard action key/label pairs (e.g. `default`, `Dismiss`, `Open`).
  - Preserves standard hints (`urgency`, `desktop-entry`, `category`, `transient`, `resident`).
  - Supports atomic in-place updates via `replaces_id`.

### 2.2 Bounded Store & Flood Protection (`notification_store.hpp`)

`NotificationStore` maintains the authoritative in-memory state of all active and historical notifications:
- **Capacity Limits**:
  - `max_active`: Maximum concurrent active notifications (default: 50).
  - `max_history`: Maximum historical notifications retained in Notification Center (default: 100).
  - `max_per_app`: Maximum notifications per application (default: 10). Incoming notifications exceeding this quota force-evict the oldest notification from that app.
- **Replacement (`replaces_id`)**:
  - If a valid `replaces_id` matches an existing notification, it is updated in-place without generating a new ID or disrupting queue ordering.
- **Lifecycle Transitions**:
  - `dismiss()`: Moves an active notification to history with `CloseReason::DismissedByUser`.
  - `expire()`: Moves an active notification to history with `CloseReason::Expired`.
  - `revoke()`: Closes notification with `CloseReason::ClosedByCall`.
  - `clear_history()`: Clears all historical entries.

### 2.3 Subsystem Backends (`notification_backend.hpp`, `dbus_notification_backend.hpp`, `internal_notification_backend.hpp`)

- **`NotificationBackend`**: Abstract interface decoupling notification sources from store and presentation logic.
- **`DBusNotificationBackend`**:
  - Implements the complete `org.freedesktop.Notifications` interface on the D-Bus session bus (`g_bus_own_name` / `g_dbus_connection_register_object`).
  - Exports methods:
    - `Notify(app_name, replaces_id, app_icon, summary, body, actions, hints, expire_timeout)` -> `id`
    - `CloseNotification(id)`
    - `GetCapabilities()` -> `["body", "body-markup", "actions", "icon-static", "persistence"]`
    - `GetServerInformation()` -> `("LDDE Notification Daemon", "LinuxDroid", "1.0", "1.2")`
  - Emits D-Bus signals:
    - `NotificationClosed(id, reason)`
    - `ActionInvoked(id, action_key)`
- **`InternalNotificationBackend`**:
  - Synchronous, in-memory backend for unit/integration tests and LDDE system-generated events (e.g. low battery, network offline).

### 2.4 Presentation & Queue Management (`notification_presenter.hpp`)

`NotificationPresenter` governs transient popup toast lifecycles:
- **Queue Bounds**: Displays up to `max_visible_popups` (default: 3) concurrently. Excess active notifications remain queued in `NotificationStore`.
- **Auto-Dismiss Timers**:
  - Low Urgency: 4,000 ms.
  - Normal Urgency: 7,000 ms.
  - Critical Urgency: Never auto-dismisses (persists until explicit user action or caller revocation).
  - Custom timeouts from callers clamped between 1,000 ms and 30,000 ms.
- **Promotion**: When a visible popup is dismissed or expired, the next queued notification is automatically promoted and displayed.

### 2.5 Notification Center State Machine (`notification_center_state.hpp`)

Governs the full-height pull-out / slide-over Notification Center panel:
- States: `Closed` <-> `Opening` <-> `Open` <-> `Closing`.
- Observers receive state change notifications to synchronize overlay redrawing and mutual exclusion with other surfaces.

### 2.6 Adaptive Mobile Layout (`notification_layout.hpp`)

Calculates responsive geometries based on D4 `DisplayPolicy`:
- **Toast Popups**:
  - Positioned at the top of the screen beneath the D11 status bar.
  - Full-width on mobile portrait with safe-area padding; centered floating pill on landscape/tablet.
  - Generous card height (>= 72 px) with >= 48 dp touch target action buttons.
- **Notification Center**:
  - Full-height slide-over panel on mobile portrait (100% width) or anchored right drawer on tablet/landscape (380 px width).
  - Dedicated header with title and "Clear All" button.
  - Scrollable notification item stack with swipe offsets.

### 2.7 Cairo Vector Rendering (`notification_view.hpp`)

Double-buffered vector graphics rendering into `ShellOverlay`:
- **Color Palettes & Urgency Accents**:
  - Low: Subdued slate grey accent.
  - Normal: Brand cyan/blue accent (`#4a90e2`).
  - Critical: Amber/red accent (`#e74c3c`).
- **Cards**: High-contrast dark translucent rounded cards (`#1a233aee`) with smooth borders (`#2d3b5cee`).
- **Vector Badges**: Crisp fallback vector badges when application icons are missing.
- **Action Buttons**: Highlighted rounded pills with touch press states.
- **Swipe Visual Feedback**: Horizontal card translation and progressive opacity fading during touch swipe gestures.
- **Empty State**: Elegant empty-state watermark when history is clear.

### 2.8 Gesture Controller & Touch Ergonomics (`notification_controller.hpp`)

Processes touch, pointer, and keyboard interactions:
- **Card Tap**: Invokes the `default` action and activates the owning application window via D3 `WindowManager`.
- **Action Button Tap**: Invokes the specific action key (e.g. `reply`, `snooze`) and emits D-Bus `ActionInvoked`.
- **Horizontal Swipe-to-Dismiss**:
  - Horizontal drag threshold >= 40 px dismisses the notification toast or history item.
  - Sub-threshold drags snap back smoothly.
- **Outside Tap**: Tapping outside the Notification Center panel dismisses and closes it.
- **Keyboard Navigation**:
  - `Escape`: Closes the Notification Center if open, or dismisses the top active popup toast.

### 2.9 Master Facade (`notification_manager.hpp`)

`NotificationManager` unifies the store, backends, layout, presenter, controller, view, and state machine into a cohesive, high-level API consumed by `ldde::core::Application`.

---

## 3. Subsystem Integrations

```text
 ┌────────────────────────────────────────────────────────────┐
 │                  ldde::core::Application                   │
 └──────┬──────────────┬──────────────┬──────────────┬────────┘
        │              │              │              │
        ▼              ▼              ▼              ▼
  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌───────────┐
  │ D1 Shell  │  │ D3 Window │  │ D4 Display│  │ D11 System│
  │ Overlay   │  │ Manager   │  │ Policy    │  │ UI        │
  └───────────┘  └───────────┘  └───────────┘  └───────────┘
```

1. **D1 Wayland Shell**:
   - Both transient popups and the Notification Center render directly onto the `ShellOverlay` subsurface buffer using double-buffered Cairo rendering.
2. **D3 Window Manager**:
   - Activating a notification's default action inspects the `desktop-entry` hint or `app_name`, matches the corresponding running application in D2 `WindowRegistry`, and calls `WindowManager::activate()` to focus and raise the window.
3. **D4 Mobile Display Policy**:
   - Layout calculations consume live `DisplayGeometry`, adapting between portrait and landscape modes while respecting safe-area insets.
4. **D6 Application Catalog**:
   - Resolves application metadata and icons from standard desktop entries.
5. **D11 System UI**:
   - Quick Controls dropdown features a dedicated **Notifications** tile (`ControlType::Notifications`) wired directly to `NotificationManager::open_center()`.
6. **Mutual Exclusion**:
   - Strict mutual exclusion ensures that opening the Notification Center closes other transient surfaces (D7 Launcher, D9 Switcher, D11 Quick Controls), and vice versa, preventing overlapping UI confusion.

---

## 4. Verification & Testing

The notification subsystem is verified across three testing tiers:

1. **Unit Tests (`tests/unit/test_notification.cpp`)**:
   - `NotificationModel`: Sanitization (HTML tags, control chars, length clamping), actions, geometry, `update_from`.
   - `NotificationStore`: Monotonic IDs, bounded history eviction, per-app flood limits, replacements, clear history.
   - `NotificationLayout`: Portrait vs landscape calculations, popup vertical stacking, hit testing.
   - `NotificationPresenter`: Queue management, timeout auto-dismiss, critical notification persistence, promotion.
   - `NotificationController`: Touch tap, action tap, swipe-to-dismiss threshold (>= 40 px), outside tap dismiss, keyboard `Escape`.
   - `InternalNotificationBackend`: Registration, notification posting, signal callbacks.
   - `NotificationManager`: End-to-end facade orchestration and mutual exclusion.

2. **Subsystem Integration Tests (`tests/integration/test_notification_integration.cpp`)**:
   - `NotificationLifecycleAndRendering`: Validates full lifecycle from post to Cairo rendering to auto-dismiss.
   - `ActionDispatchAndWindowActivation`: Validates action invocation and D3 window activation hooks.
   - `CenterOpenCloseAndMutualExclusion`: Validates mutual exclusion with Launcher, Switcher, and System UI.
   - `OrientationAdaptation`: Validates adaptive relayout when switching between portrait and landscape.

3. **Weston Compositor Integration Tests (`tests/integration/test_weston_integration.cpp`)**:
   - `NotificationWorkflowWithRealWestonCompositor`: End-to-end validation running against a real headless Weston compositor instance, validating Wayland surface commits, buffer rendering, notification posting, and surface state management.
