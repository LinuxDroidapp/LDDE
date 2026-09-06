# LDDE D13 — Settings Subsystem Architecture

## 1. Architectural Overview

The **Settings Subsystem** provides a centralized, authoritative, touch-first preference management surface for the LinuxDroid Desktop Environment (LDDE). It allows users to inspect and customize all LDDE subsystems through an adaptive, responsive Cairo vector user interface while guaranteeing type safety, strict schema validation, and atomic filesystem persistence.

```text
                     Android LinuxDroid App
                              │
                    LinuxDroid Guest Session
                              │
                    Weston Wayland Compositor
                              │
                             LDDE
   ┌────────────────────────────────────────────────────────┐
   │                  Settings Subsystem                    │
   │                                                        │
   │   ┌──────────────────┐        ┌───────────────────┐    │
   │   │  SettingsView    │◄───────┤ SettingsController│    │
   │   │  (Cairo Render)  │        │ (Touch/Key/Mouse) │    │
   │   └────────┬─────────┘        └─────────┬─────────┘    │
   │            │                            │              │
   │            ▼                            ▼              │
   │   ┌──────────────────┐        ┌───────────────────┐    │
   │   │  SettingsLayout  │        │SettingsNavigation │    │
   │   │(Portrait / Split)│        │ (Drill-down/Tabs) │    │
   │   └──────────────────┘        └─────────┬─────────┘    │
   │                                         │              │
   │            ┌────────────────────────────┘              │
   │            ▼                                           │
   │   ┌──────────────────┐        ┌───────────────────┐    │
   │   │  SettingsSearch  │◄───────┤   SettingsStore   │    │
   │   │ (Index / Query)  │        │(Transact/Notify)  │    │
   │   └──────────────────┘        └─────────┬─────────┘    │
   │                                         │              │
   │                               ┌─────────┴─────────┐    │
   │                               ▼                   ▼    │
   │                      ┌─────────────────┐ ┌───────────┐ │
   │                      │ SettingsSchema  │ │  Config   │ │
   │                      │(Typed / Bounds) │ │ (~/conf)  │ │
   │                      └─────────────────┘ └───────────┘ │
   └────────────────────────────────────────────────────────┘
```

---

## 2. Core Architecture & Components

### 2.1 SettingsSchema & SettingDefinition
- **Type Safety**: Settings are strongly typed via `SettingType` (`Bool`, `Int`, `Double`, `String`, `Enum`).
- **Validation**: `SettingDefinition` enforces numerical ranges (`min_value`, `max_value`), enum allowed sets (`enum_values`), step intervals (`step_value`), and semantic regex matching.
- **Categories**: Organizes settings into 10 first-class categories:
  1. `Appearance` (Color scheme, UI density, accent color, desktop gradient colors)
  2. `Display` (Scale factor, orientation policy)
  3. `Windows` (Default mode, cascade offset, window margins)
  4. `Desktop` (Background style, ambient wallpaper glow, empty desktop watermark)
  5. `Dock` (Dock enabled, screen position, autohide visibility, item icon size)
  6. `Launcher` (Layout grid/list, category tabs, app search)
  7. `Input` (Tap to click, touch gestures, drag sensitivity threshold, double-tap timeout)
  8. `Notifications` (Subsystem enable, toast duration, max stacked popups, grouping)
  9. `SystemUI` (Clock format 12h/24h, seconds toggle, network/audio/battery icon toggles, quick controls drawer enable)
  10. `About` (LDDE build version, environment details, system identity)

### 2.2 SettingsStore & Atomic Persistence
- **Config Adapter**: Backed by `ldde::config::Config` reading/writing key-value pairs formatted as INI sections (`section.property`).
- **Transactional Staging**: Supports `begin_transaction()`, `commit()`, and `rollback()` for atomic multi-setting edits.
- **Atomic POSIX Persistence**: Persistence via `Config::save_to_file()` uses a temporary file (`.tmp.XXXXXX`) created in the same filesystem directory as `~/.config/linuxdroid/desktop.conf`, followed by `fsync()` and POSIX atomic `rename()`. This guarantees zero file corruption during power cuts or guest container crashes.
- **Runtime Change Notifications**: Fires event-driven callbacks (`on_setting_changed`) to notify active desktop components (`Shell`, `WindowManager`, `Dock`, `Launcher`, `SystemUI`, `NotificationManager`) to rerender or reconfigure immediately.

### 2.3 Responsive Layout & Interaction Engine
- **Portrait Mobile Mode**:
  - Automatically activates on phone displays or when display width < display height.
  - Drill-down navigation model: Root view lists categories and global search; tapping a category drills down into its settings details with a prominent back navigation button.
  - Safe-area inset avoidance for status bar, navigation bar, and display cutouts.
- **Landscape / Tablet Split Mode**:
  - Activates on landscape or tablet resolutions.
  - Dual-pane layout: Left sidebar list (210dp width) for categories and search, right pane for detailed settings controls.
- **Touch Targets**: All interactive rows, buttons, radio segments, and toggles strictly enforce $\ge 48\text{dp}$ touch target dimensions conforming to mobile ergonomics.

### 2.4 Window Management & System UI Integration
- **Authoritative Window Model**:
  - Settings runs in-process as an authoritative `window::Window` with `app_id = "org.linuxdroid.ldde.settings"`.
  - Fully registered in `WindowRegistry` (D2) and managed by `WindowManager` (D3).
  - Can be activated, raised, minimized, maximized, restored, and closed through standard LDDE window controls.
  - Tracked in `Switcher` (D9) and `Dock` (D8) as a running task.
- **Application Discovery & Interception**:
  - Registered in `ApplicationCatalog` (D6) with desktop entry identity `org.linuxdroid.ldde.settings`.
  - `LinuxSessionApplicationLauncher` intercepts launches for this app ID via `register_built_in_handler`, opening the in-process settings window without shell string execution or failing `execvp`.
- **Quick Controls Tile**:
  - Quick Controls panel contains a dedicated Settings tile. Tapping it smoothly closes the system panel and raises the Settings window.

---

## 3. Keyboard & Input Routing

- **Touch Events**: Directly routed through `Application::initialize_components()` touch listeners, handling taps, swipes, and list scrolling.
- **Pointer Events**: Mouse motion, click down/up, and axis wheel scrolling.
- **Keyboard Navigation**:
  - `Arrow Up` / `Arrow Down`: Moves selection through setting rows and categories.
  - `Arrow Left` / `Arrow Right`: Decrements/increments numerical sliders.
  - `Enter` / `Space`: Toggles booleans or cycles through enum options.
  - `Escape`: Navigates back from category details or closes Settings.
  - Typing printable ASCII characters automatically populates the real-time search field.

