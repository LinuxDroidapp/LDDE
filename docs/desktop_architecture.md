# LDDE Home/Desktop Subsystem (D10)

## 1. Overview & Architectural Position

The **Home/Desktop (D10)** subsystem provides the persistent, Linux-native, mobile-first desktop surface for the LinuxDroid Desktop Environment (LDDE). It serves as the visual and navigational foundation of the Linux session, integrating the shell, launcher, dock, switcher, display policy, application catalog, and window-management subsystems into a coherent desktop experience.

The Home/Desktop surface operates at the base layer of LDDE:

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
 │ Home/Desktop   ← D10                 │
 │ Shell          ← D1                  │
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
1. **Persistent Visual Foundation**: The desktop surface is the lowest-layer Wayland surface in the compositor hierarchy (`DesktopSurface`), remaining persistently underneath all application windows and shell overlay chrome.
2. **Authoritative State Consumption**: Desktop empty-state and active window metrics are derived strictly from D2 (`WindowRegistry`). The desktop does **not** maintain independent window tracking, does **not** scan `/proc` or `ps`, and does **not** run shell scripts.
3. **Multi-Overlay Coordination**: Tapping an open area of the desktop dismisses active transient shell overlays (such as the D7 Launcher and D9 Switcher).
4. **Mobile-First Gesture Navigation**: A swipe-up gesture on the empty desktop surface acts as a primary entry point to open the D7 Launcher.
5. **Dynamic Display & Form-Factor Adaptation**: Layout and background geometry automatically adapt to display resolution, rotation (portrait vs landscape), and safe-area insets via D4 `DisplayPolicy`.
6. **Zero Clutter & Strict Scope Discipline**: The desktop surface does not clutter the screen with arbitrary file icons or widgets. It respects strict architectural boundaries: zero notification systems (deferred to D11+), zero settings panels, and zero Android/PRoot dependencies.

---

## 2. Component Architecture

The Desktop subsystem (`include/ldde/desktop/`, `src/desktop/`) follows a clean Model-View-Controller (MVC) and Facade pattern:

```text
┌─────────────────────────────────────────────────────────────┐
│                           Desktop                           │
│                       (Public Facade)                       │
└───────┬─────────────┬─────────────┬─────────────┬───────────┘
        │             │             │             │
        ▼             ▼             ▼             ▼
┌──────────────┐┌──────────────┐┌──────────────┐┌─────────────┐
│ DesktopState ││ DesktopModel ││DesktopLayout ││ DesktopView │
│ (State Mach.)││ (Window Agg.)││  (Geometry)  ││ (Rendering) │
└───────▲──────┘└───────▲──────┘└───────▲──────┘└─────────────┘
        │               │               │
        └───────────────┼───────────────┘
                        │
                ┌───────┴───────────┐
                │ DesktopController │
                │ (Gestures & Input)│
                └───────────────────┘
```

### 2.1 Component Responsibilities

- **`DesktopStateMachine`** (`desktop_state.hpp`):
  Governs desktop lifecycle states: `Initializing` $\to$ `Ready` $\to$ `Active` $\rightleftharpoons$ `Suspended` $\to$ `Stopping` $\to$ `Stopped`. Provides thread-safe, deterministic state transitions.
- **`DesktopBackground`** (`desktop_background.hpp`):
  Manages desktop background styling and rendering parameters loaded from configuration (`desktop.background_mode`, `desktop.background_color`, `desktop.background_color_bottom`, `desktop.ambient_glow`, `desktop.show_empty_hint`). Supports solid fills, linear vertical gradients, and subtle radial ambient glows.
- **`DesktopLayout`** (`desktop_layout.hpp`):
  Translates D4 `DisplayPolicy` into desktop geometry: screen dimensions, workspace bounds (safe area insets excluding status bar and dock), orientation, and form-factor classification (`CompactPhonePortrait`, `CompactPhoneLandscape`, `ExpandedTablet`).
- **`DesktopModel`** (`desktop_model.hpp`):
  Observes D2 `WindowRegistry` to track the count of active, non-destroyed application windows and determines empty-state (`is_empty()`) and desktop focus (`is_desktop_focused()`). Notifies observers whenever window counts or focus state change.
- **`DesktopView`** (`desktop_view.hpp`):
  Renders the desktop background and empty-state branding watermark into `ShmBuffer` using Cairo vector graphics. Ensures zero tearing and crisp rendering across high-DPI display scales.
- **`DesktopController`** (`desktop_controller.hpp`):
  Processes touch events (down, motion, up, cancel), coordinates tap detection (20px slop threshold) to dismiss open overlays (Launcher, Switcher), and detects vertical swipe-up gestures ($\ge 40\,\text{px}$) to trigger Launcher opening.
- **`Desktop`** (`desktop.hpp`):
  Unified facade orchestrating state machine, model, layout, background, view, and controller. Binds rendering directly to D1 `DesktopSurface` via `set_render_callback`.

---

## 3. Rendering Pipeline & Visual Styling

The desktop rendering pipeline integrates with the D1 Shell architecture:

1. **Surface Wiring**: During initialization, `Desktop::initialize` registers a callback with `DesktopSurface::set_render_callback`:
   ```cpp
   shell_->desktop().set_render_callback([this](shell::ShmBuffer& buf, const shell::ShellTheme& theme) {
       render(buf, theme);
   });
   ```
2. **Double-Buffered Vector Graphics**: Rendering is executed via Cairo onto the shared-memory buffer of the Wayland surface.
3. **Background Composition**:
   - **Solid or Gradient**: A linear vertical gradient interpolating between `background_color` (top) and `background_color_bottom` (bottom).
   - **Ambient Wallpaper Glow**: A subtle top-center radial gradient providing depth and modern mobile desktop aesthetics.
   - **Empty-State Branding**: When the desktop is empty (`is_empty() == true`) and `show_empty_hint` is enabled, a subtle, anti-aliased "LDDE" watermark and hint ("Swipe up for apps") is rendered centered in the available workspace.

---

## 4. Touch Gestures & Overlay Coordination

On mobile devices, touch is the primary input modality. The `DesktopController` provides:

- **Tap to Dismiss**:
  When a tap is recognized (touch down followed by touch up within 20 pixels), the controller checks if transient overlays are open:
  - If Launcher (D7) is open $\to$ closes Launcher.
  - If Switcher (D9) is open $\to$ closes Switcher.
- **Swipe Up to Open Launcher**:
  When the user swipes upwards from the desktop surface by at least 40 pixels ($dy \le -40\,\text{px}$) and horizontal deviation is bounded, the controller invokes `launcher_->open()`.

---

## 5. Scope Protection & Verification

| Requirement | Implementation | Status |
| :--- | :--- | :--- |
| **D10 Home Surface** | Cairo background rendering into D1 `DesktopSurface` | Verified |
| **Model & Window State** | Direct observation of D2 `WindowRegistry` | Verified |
| **Overlay Coordination** | Tap-to-dismiss for D7 Launcher and D9 Switcher | Verified |
| **Gesture Navigation** | Swipe-up to open D7 Launcher | Verified |
| **Display Adaptation** | Dynamic updates on display configuration change | Verified |
| **Zero D11+ Scope Creep** | No notification center, no settings panel | Strictly Enforced |
| **Zero Linux Abstractions** | No `/proc` or `ps` parsing, no shell scripts | Strictly Enforced |
