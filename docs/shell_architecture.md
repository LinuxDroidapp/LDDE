# LDDE D1 — Wayland Shell Architecture

This document specifies the architectural foundation of the **LDDE D1 Wayland Shell** subsystem.

---

## 1. Architectural Role & Boundary

LDDE is strictly a **Wayland client** running under the **Weston** compositor. LDDE does **not** act as a compositor.

```text
LinuxDroid Runtime
        ↓
    Guest Init
        ↓
       LDDM
        ↓
      Weston (Compositor)
        ↓
   LDDE Shell (Wayland Client)
   ├── Desktop Surface (Background)
   ├── Status Region (Top bar subsurface)
   ├── Dock Region (Floating pill subsurface)
   └── Shell Overlay (Modal / Scrim subsurface)
        ↓
 Linux Applications
```

### Protocol Usage
- **Core Protocol**: `wl_compositor` (v4/v5), `wl_subcompositor` (v1), `wl_shm` (v1), `wl_output` (v4), `wl_seat` (v7)
- **Subsurface Composition**: Subsurfaces are created via `wl_subcompositor_get_subsurface(child, parent)` and set to desynchronized mode (`wl_subsurface_set_desync`) for independent, low-latency repainting.
- **Buffers**: Zero-copy shared memory (`wl_shm`) backed by Linux POSIX anonymous shared memory (`memfd_create` with `MFD_CLOEXEC` fallback to `shm_open`).

---

## 2. Shell Subsystem Component Diagram

```text
                               +-------------------+
                               |    Application    |
                               +---------+---------+
                                         |
                               +---------v---------+
                               |    Shell Core     |
                               +----+----+----+----+
                                    |    |    |
        +---------------------------+    |    +--------------------------+
        |                                |                               |
+-------v-------+               +--------v--------+              +-------v-------+
|  ShellLayout  |               |  DesignTokens   |              |  ShellTheme   |
| (Responsive,  |               |  (DP baseline,  |              | (Hex parsing, |
|  hit-testing) |               |   DPI scaled)   |              |   presets)    |
+-------+-------+               +-----------------+              +---------------+
        |
+-------v------------------------------------------------------------------------+
|                               ShellSurfaces Hierarchy                         |
|                                                                                |
|  +--------------------------------------------------------------------------+  |
|  | DesktopSurface (Root wl_surface, Background layer, 100% viewport)        |  |
|  +---------------------+-------------------------------+--------------------+  |
|                        |                               |                       |
|           +------------v------------+     +------------v------------+          |
|           | StatusRegion Subsurface |     |  DockRegion Subsurface  |          |
|           | (Top safe area, clock,  |     | (Bottom floating pill,  |          |
|           |  status indicators)     |     |  responsive width ratio)|          |
|           +-------------------------+     +-------------------------+          |
|                                                                                |
|           +---------------------------------------------------------+          |
|           | ShellOverlay Subsurface (Modal / Scrim transient layer) |          |
|           +---------------------------------------------------------+          |
+--------------------------------------------------------------------------------+
        |
+-------v-------+
| CairoRenderer | (2D vector rendering, rounded rects, anti-aliased text)
+-------+-------+
        |
+-------v-------+
| ShmBufferPool | (Double-buffered shm allocation & recycle lifecycle)
+---------------+
```

---

## 3. Structural Regions & Layering

| Region | Layer | Geometry & Role | Subsurface Mode |
|---|---|---|---|
| **Desktop** | `Background` (0) | Fullscreen ($0, 0, W, H$). Wallpapers, gradients, desktop area. | Root surface |
| **Dock** | `Bottom` (1) | Centered pill at bottom edge: 90% screen width in portrait, 60% in landscape. | Desync subsurface attached to Desktop |
| **Status** | `Top` (2) | Spans width of top safe area; height scaled to 40dp. Clock and system badge. | Desync subsurface attached to Desktop |
| **Overlay** | `Overlay` (3) | Fullscreen backdrop scrim + centered modal card. Dismisses on tap outside. | Desync subsurface attached to Desktop |

---

## 4. Mobile-First Responsive Layout

### Safe Area Insets
The layout engine accounts for display notches, cutouts, and system navigation gesture margins (`safe_insets`: top, right, bottom, left).

### Geometry Equations
- **Status Region**:
  $$\text{status.x} = \text{safe\_insets.left}$$
  $$\text{status.y} = \text{safe\_insets.top}$$
  $$\text{status.w} = \text{screen.w} - \text{safe\_insets.left} - \text{safe\_insets.right}$$
  $$\text{status.h} = \text{tokens.status\_height\_px}$$

- **Dock Region (Bottom)**:
  $$\text{dock.w} = \min(\text{safe.w}, \text{safe.w} \times \text{width\_ratio})$$
  $$\text{dock.h} = \text{tokens.dock\_height\_px}$$
  $$\text{dock.x} = \text{safe.x} + \frac{\text{safe.w} - \text{dock.w}}{2}$$
  $$\text{dock.y} = \text{safe.y} + \text{safe.h} - \text{dock.h} - \text{tokens.dock\_margin\_bottom\_px}$$

---

## 5. Design Tokens & DPI Scaling

All dimensions are defined in Density-Independent Pixels (DP) based on a 160 DPI baseline:
- `kStatusHeightDp`: 40 dp
- `kDockHeightDp`: 68 dp
- `kDockMarginBottomDp`: 16 dp
- `kDockCornerRadiusDp`: 24 dp
- `kMinTouchTargetDp`: 48 dp (matches accessibility standards)
- `kSpacingXsDp`, `Sm`, `Md`, `Lg`: 4, 8, 16, 24 dp

Pixels are scaled deterministically:
$$\text{px} = \operatorname{round}(\text{dp} \times \text{scale\_factor})$$

---

## 6. Rendering Pipeline

1. **Buffer Acquisition**: The surface requests an `ARGB8888` buffer of $(W, H)$ from `ShmBufferPool`. The pool recycles released buffers or allocates new `memfd` buffers on demand.
2. **Vector Painting via Cairo**: `CairoRenderer` wraps the buffer memory with a `cairo_image_surface_t` and paints:
   - Subtle vertical gradients and wallpaper glow on the desktop
   - Semi-transparent rounded pills with typography on the status bar
   - Floating pill with slot indicators on the dock
   - Dimming scrim with modal card on overlays
3. **Commit & Damage**:
   - `wl_surface_attach(surf, buffer->wl_buf(), 0, 0)`
   - `wl_surface_damage(surf, 0, 0, width, height)`
   - `wl_surface_commit(surf)`
4. **Buffer Release Lifecycle**: Compositor signals `wl_buffer.release` when reading finishes, transitioning buffer busy state to false for reuse.

---

## 7. Input Routing & Hit Testing

The shell implements geometric hit testing without depending on external input widgets:
- Priority order: `Overlay` (if active) $\rightarrow$ `Dock` $\rightarrow$ `Status` $\rightarrow$ `Desktop`
- Dispatches touch and pointer events to the focused shell region.
- Future phases (D2–D4) build launchers, docks, and window manager input atop this routing foundation.
