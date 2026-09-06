# LDDE D11 — System UI Subsystem Architecture

## 1. Overview & Purpose

The **LDDE System UI** subsystem implements the operating-environment status indicators and quick controls surrounding the LinuxDroid Desktop Environment (LDDE). It delivers a unified, mobile-first, Wayland-native surface that exposes essential operational metrics—clock, network connectivity, audio levels, battery telemetry, display geometry, and session status—while providing responsive quick control toggles.

Operating in a containerized, PRoot, or VM Linux guest environment hosted on Android presents distinct challenges: hardware subsystems (audio cards, battery sysfs nodes, network devices) may be partially exposed, virtualized, or completely absent. The LDDE System UI is designed with defensive capability tracking (`Available`, `Unavailable`, `Unsupported`, `Error`) and graceful degradation, guaranteeing deterministic behavior with zero exceptions, zero crashes, and zero `/proc` or `ps` process scans.

```
       Android Host (LinuxDroid)
                  ↓
       Guest Init / Wayland Compositor (Weston)
                  ↓
       LDDE Application / Shell Overlay
 ┌─────────────────────────────────────────────────────────┐
 │                   Top Status Region                     │
 │  [ 12:00 ]                  [Net] [Audio] [Batt] [Sess] │
 └────────────────────────────┬────────────────────────────┘
                              │ Tap status bar
                              ▼
 ┌─────────────────────────────────────────────────────────┐
 │                   System UI Panel                       │
 │  ┌───────────────────────┐   ┌───────────────────────┐  │
 │  │   Audio Mute/Volume   │   │     Network State     │  │
 │  │      [Available]      │   │      [Available]      │  │
 │  └───────────────────────┘   └───────────────────────┘  │
 │  ┌───────────────────────┐   ┌───────────────────────┐  │
 │  │     Display Info      │   │    Session Control    │  │
 │  │      [Available]      │   │      [Available]      │  │
 │  └───────────────────────┘   └───────────────────────┘  │
 └─────────────────────────────────────────────────────────┘
```

---

## 2. Architectural Components

The subsystem resides in `include/ldde/system/` and `src/system/`, structured into distinct, decoupled layers:

### 2.1 Telemetry Providers & Aggregator

1. **Clock (`ClockProvider`, `SystemClockProvider`, `MockClockProvider`, `ClockStatus`)**
   - Implements thread-safe local time querying via `<ctime>` and `localtime_r`.
   - Supports configurable 12-hour and 24-hour formats with optional seconds display.
   - Dispatches callbacks whenever time or date text changes.

2. **Network (`NetworkStatusProvider`, `LinuxSysfsNetworkProvider`, `MockNetworkProvider`, `NetworkStatus`)**
   - Inspects Linux sysfs nodes (`/sys/class/net/*`) checking `operstate` and `carrier`.
   - Distinguishes WiFi (`wlan*`), Ethernet (`eth*`, `en*`), Cellular (`rmnet*`, `wwan*`), and Loopback (`lo`).
   - Supports interface toggle actions where permitted by host capabilities.

3. **Audio (`AudioStatusProvider`, `LinuxAudioProvider`, `MockAudioProvider`, `AudioStatus`)**
   - Queries Linux audio availability via `/proc/asound/cards` and `/dev/snd`.
   - Calculates discrete volume tiers (`Muted`, `Low`, `Medium`, `High`, `Unavailable`).
   - Implements atomic mute toggles and volume clamping without executing external shell commands.

4. **Battery (`BatteryStatusProvider`, `LinuxSysfsBatteryProvider`, `MockBatteryProvider`, `BatteryStatus`)**
   - Reads battery capacity and status from `/sys/class/power_supply/*`.
   - Detects charging states (`Charging`, `Discharging`, `Full`, `NotCharging`).
   - Automatically degrades to `BatteryState::Unavailable` in containerized environments lacking host power supply passthrough.

5. **Display (`DisplayStatus`)**
   - Binds to D4 `display::DisplayPolicy`.
   - Tracks resolution, scale factor, and orientation (`Portrait`, `Landscape`).
   - Fires update events upon physical or virtual display changes.

6. **Session (`SessionStatusProvider`, `DesktopSessionStatusProvider`, `SessionStatus`)**
   - Reports compositor status ("Weston"), desktop environment ("LinuxDroid LDDE"), and session lifecycle state.

7. **Aggregator (`SystemDataProvider`)**
   - Composes all status providers into a unified facade.
   - Manages periodic polling and composite change notifications.

---

### 2.2 Quick Controls & Capability Awareness

Quick controls expose actionable tiles within the pop-up/drop-down panel:
- **`ControlCapability`**: `Available`, `Unavailable`, `Unsupported`, `Error`.
  - When a hardware subsystem is absent, the corresponding tile displays `Unavailable` and rejects activation gestures safely.
- **`QuickControlsManager`**:
  - Maintains tile list with bounding box geometry.
  - Implements coordinate hit-testing.
  - Supports keyboard focus traversal (`select_next()`, `select_prev()`, `activate_selected()`).

---

### 2.3 State Machine & Gesture Interaction

- **`SystemPanelStateMachine`**:
  - Enforces deterministic state transitions:
    `Closed` $\leftrightarrow$ `Opening` $\leftrightarrow$ `Open` $\leftrightarrow$ `Closing`.
  - Emits `StateChangedCallback` events to synchronize with the D1 `ShellOverlay`.

- **Touch Ergonomics**:
  - **Status Bar Tap**: Bounding box test on the top status pill toggles the panel open or closed.
  - **Tile Tap**: Bounding box hit test inside the panel triggers the associated quick control action.
  - **Outside Tap**: Any touch outside the panel bounds dismisses the panel immediately.
  - **Swipe-Up Dismiss**: Upward vertical drag $\ge 40\,\text{px}$ dismisses the panel.

- **Keyboard Navigation**:
  - `Escape`: Closes panel.
  - `Tab` / `Down` / `Right`: Cycles selection forward.
  - `Shift+Tab` / `Up` / `Left`: Cycles selection backward.
  - `Enter` / `Space`: Activates selected control.

---

### 2.4 Layout & Mobile Responsiveness

- **`SystemUILayout`**:
  - Phone Portrait: Centers panel horizontally with adaptive width ($\le 380\,\text{dp}$, $\ge 280\,\text{dp}$).
  - Phone Landscape: Aligns panel to the right edge to maximize visible desktop workspace ($\le 360\,\text{dp}$).
  - Touch Targets: Enforces minimum $48 \times 48\,\text{dp}$ touch target guidelines across all interactive tiles.
  - Safe Insets: Consumes D4 safe display geometry to avoid camera cutouts and notch regions.

---

### 2.5 Vector Rendering (`SystemUIView`)

- Renders status bar and panel contents using double-buffered Cairo vector graphics into shared memory (`shm::ShmBuffer`).
- Draws high-contrast status bar indicators with rounded pills, vector Wi-Fi arcs, speaker cones, and battery level bars.
- Draws frosted dark surface cards with glowing focus rings for selected quick controls.

---

### 2.6 Subsystem Coordination

- **D1 Shell**: Binds `StatusRegion::set_render_callback` to paint the top bar; multiplexes `ShellOverlay` for the dropdown panel.
- **D7 Launcher & D9 Switcher**: Mutual exclusion ensures only one overlay surface is visible at a time.
- **D10 Desktop**: Empty desktop taps dismiss the System UI panel cleanly.
- **Graceful Degradation**: Zero crashes, exceptions, or shell scripts under PRoot/container environments without battery or sound card.

