# LDDE D4 — Mobile Display Policy

## 1. Overview & Architectural Principle

The **Mobile Display Policy** subsystem establishes the centralized, authoritative display and responsive-layout foundation for the LinuxDroid Desktop Environment (LDDE).

The primary target is a **mobile Linux desktop running on a phone**, while simultaneously supporting landscape phones, tablets, foldables, and external monitors.

> **Key Principle:** LDDE adapts the desktop environment to the display instead of forcing a traditional desktop layout onto a mobile display.

```
                 Wayland Output
                       │
                       ▼
                DisplayManager
                       │
                       ▼
                  DisplayInfo
                       │
                       ▼
                 DisplayPolicy
                       │
              ┌────────┴────────┐
              ▼                 ▼
        Shell Layout       Window Geometry
              │                 │
              ▼                 ▼
            LDDE             WindowManager
```

No other LDDE subsystem independently calculates usable display size, safe-area geometry, orientation, scale, desktop bounds, shell bounds, or window placement bounds.

---

## 2. Coordinate Systems

LDDE distinguishes between five explicit coordinate spaces:

1. **Physical Pixel Coordinates**:
   Native raster framebuffer and hardware mode pixels (e.g. $1080 \times 2400$). Modes reported by `wl_output.mode` are in this space.

2. **Logical Wayland Coordinates**:
   Compositor surface coordinate space where:
   $$\text{logical} = \frac{\text{transformed physical pixels}}{\text{scale}}$$
   Accounts for hardware orientation / `wl_output.transform`.

3. **LDDE Layout Coordinates**:
   Desktop workspace coordinate space with origin $(0,0)$ anchored at the primary display.

4. **Window Geometry Coordinates**:
   Logical bounding rectangles of application windows positioned within the available workspace.

5. **Input Coordinates**:
   Touch tap and pointer motion coordinates arriving from the Wayland seat in logical surface space.

Conversions are defined explicitly in `ldde/display/display_geometry.hpp`:
* `physical_to_logical(pixels, scale)`
* `logical_to_physical(coords, scale)`
* `apply_transform_to_dimensions(transform, in_w, in_h, out_w, out_h)`

---

## 3. Wayland Output Discovery

Outputs are discovered dynamically via the Wayland protocol (`wl_output` global):
* **Identity**: `id`, `name` (e.g. `DSI-1`, `WL-1`), `make`, `model`, `description`.
* **Geometry**: compositor logical position $(x, y)$, physical dimensions in millimeters (`physical_width_mm`, `physical_height_mm`), hardware transform.
* **Modes**: supported modes list, preferred mode, active mode resolution and refresh rate in millihertz.
* **Scale**: integer output scale factor (e.g. 1x, 2x, 3x).

No target phone resolution (such as $1080 \times 2400$) is hardcoded.

---

## 4. Scaling Policy

Wayland output scaling is encapsulated in `ScalePolicy`:
* Exposes `scale_factor()` (integer) and `effective_scale()` (double).
* Converts points, sizes, and rectangles between physical and logical coordinate spaces.
* UI and layout metrics (e.g. minimum touch hit target) scale consistently.
* Touch hit areas maintain usability across diverse DPIs without inflating icons unnecessarily.

---

## 5. Orientation & Dynamic Changes

Orientation is centralized in `ldde/display/orientation.hpp`:
```cpp
enum class Orientation : uint8_t {
    Portrait,
    Landscape,
    PortraitReverse,
    LandscapeReverse
};
```

Orientation is derived from actual output dimensions and transform:
* Natural aspect ratio evaluation (height > width $\to$ portrait).
* Respects `wl_output` transforms: `Normal`, `Rotate90`, `Rotate180`, `Rotate270`, `Flipped`, etc.

### Dynamic Change Flow
When the display rotates or resolution changes:
1. `DisplayManager` updates `DisplayInfo`.
2. `DisplayPolicy` recalculates metrics, orientation, and available geometry.
3. D1 Shell updates its surfaces (status bar, dock, desktop) to fit the new geometry.
4. Shell registers its updated layout reservations with `DisplayPolicy`.
5. `DisplayPolicy` recalculates final `available_window_geometry()`.
6. D3 WindowManager adapts tracked windows:
   - Maximized windows re-maximize to new bounds.
   - Fullscreen windows cover the full display.
   - Normal floating windows are clamped to remain visible with accessible headers.
   - Saved geometries are constrained.
7. Windows and client processes are preserved without restart.

---

## 6. Safe Areas & Cutouts

The safe-area model (`SafeInsets`) represents hardware cutouts, camera notches, and rounded display corners:
```cpp
struct SafeInsets {
    int32_t left = 0;
    int32_t top = 0;
    int32_t right = 0;
    int32_t bottom = 0;
};
```
* Can be configured via `display.safe_area.top`, `display.safe_area.bottom`, etc.
* Rotates with display orientation transitions (e.g. a top notch moves to side in landscape).
* Separated from physical display bounds.

---

## 7. Responsive Layout Classes

LDDE defines three responsive layout classes:

| Layout Class | Target Device / Setup | Windowing Policy Hint | Sizing & Target Characteristics |
|---|---|---|---|
| **Compact** | Narrow phone portrait (< 600dp) | `SingleDominant` | Single dominant app, 48dp touch targets, compact shell |
| **Standard** | Phone landscape / foldable / compact tablet | `MultipleWindows` | Floating windows, expanded controls, diagonal cascade |
| **Expanded** | Large tablet / desktop / external monitor | `MultipleWindowsExpanded` | Multi-window workspace, tighter pointer targets, spacious margins |

Configurable overrides are supported via `display.layout_class = "compact" | "standard" | "expanded" | "auto"`.

---

## 8. Layout Metrics & Touch Usability

Centralized in `LayoutMetrics`:
* `minimum_touch_target_px`: 48px baseline for mobile touch.
* `window_control_target_px`: 44–48px hit area for close, maximize, and minimize buttons.
* `window_control_visual_size_px`: 20–24px visual icons (visual size decoupled from hit target).
* `resize_target_px`: 20–24px grab border.
* `title_bar_height_px`: 36–44px depending on layout class.
* `status_bar_height_px`: 32–40px.
* `dock_height_px`: 56–68px.
* `min_window_width_px` / `min_window_height_px`: 200x150px minimum usable window size.

---

## 9. Available Geometry & Shell Reservations

`AvailableGeometry` provides the single source of geometric truth:
* `full_bounds`: Full logical display size $(0, 0, W_{\text{logical}}, H_{\text{logical}})$.
* `safe_bounds`: `full_bounds` inset by `safe_insets`.
* `shell_bounds`: Bounding box occupied by shell regions.
* `window_bounds`: Usable area for application windows (`safe_bounds` minus shell reservations and content margins).

### Non-Circular Data Flow
```
DisplayManager
      ↓
DisplayPolicy
      ↓
LayoutMetrics
      ↓
ShellLayout
      ↓
Shell reservations
      ↓
DisplayPolicy window bounds
      ↓
WindowManager
```

---

## 10. Authoritative Window Geometry Policy (D3 Integration)

WindowManager delegates geometry calculations to `DisplayPolicy`:
* `available_window_geometry()`: Usable floating window region.
* `maximized_geometry()`: Bounded window area between status bar and dock.
* `fullscreen_geometry()`: Complete display bounds covering shell chrome.
* `default_window_size()`: Responsive window sizing based on layout class and orientation.
* `calculate_initial_window_geometry()`: Centered cascade for portrait, diagonal cascade for landscape.
* `constrain_window_geometry()`: Enforces minimum size, titlebar accessibility, and horizontal visibility.
* `restore_window_geometry()`: Revalidates saved window geometry within current display bounds.

---

## 11. Multi-Output Foundation & Primary Display

* Displays are tracked by `DisplayId` in a `DisplayRegistry`.
* `primary_display()` and `primary_policy()` are authoritative.
* Primary display selection checks configured preference (`display.primary`) before falling back to active outputs.
* Output removal triggers window migration to the remaining primary output without destroying application state.

---

## 12. Configuration Reference

```ini
[display]
# Preferred primary output name (e.g. DSI-1, WL-1, HDMI-A-1)
primary = DSI-1

# Responsive layout class override: auto, compact, standard, expanded
layout_class = auto

# Output scale override (integer >= 1)
scale = 1

# Hardware safe area insets (logical pixels)
safe_area.top = 36
safe_area.bottom = 24
safe_area.left = 0
safe_area.right = 0
```
