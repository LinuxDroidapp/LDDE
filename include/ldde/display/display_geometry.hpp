#pragma once

#include <cstdint>
#include "ldde/core/types.hpp"
#include "ldde/display/orientation.hpp"
#include "ldde/display/safe_area.hpp"

namespace ldde::display {

/**
 * Coordinate System Definitions:
 * 
 * 1. Physical Pixel Coordinates:
 *    Native framebuffer / hardware pixel coordinates (e.g., 1080 x 2400).
 * 
 * 2. Logical Wayland Coordinates:
 *    Compositor surface coordinates where logical = physical / scale,
 *    taking into account display orientation/transform swaps.
 * 
 * 3. LDDE Layout Coordinates:
 *    Central desktop coordinate space anchored at the primary display.
 * 
 * 4. Window Geometry Coordinates:
 *    Position and dimensions of application windows in logical layout space.
 * 
 * 5. Input Coordinates:
 *    Pointer and touch event coordinates arriving via Wayland seat in logical space.
 */

// Explicit Coordinate Conversion Utilities

[[nodiscard]] constexpr int32_t physical_to_logical(int32_t physical, int32_t scale) noexcept {
    int32_t s = (scale > 0) ? scale : 1;
    return physical / s;
}

[[nodiscard]] constexpr int32_t logical_to_physical(int32_t logical, int32_t scale) noexcept {
    int32_t s = (scale > 0) ? scale : 1;
    return logical * s;
}

[[nodiscard]] constexpr core::Point physical_to_logical(core::Point pt, int32_t scale) noexcept {
    return core::Point{
        .x = physical_to_logical(pt.x, scale),
        .y = physical_to_logical(pt.y, scale)
    };
}

[[nodiscard]] constexpr core::Point logical_to_physical(core::Point pt, int32_t scale) noexcept {
    return core::Point{
        .x = logical_to_physical(pt.x, scale),
        .y = logical_to_physical(pt.y, scale)
    };
}

[[nodiscard]] constexpr core::Size physical_to_logical(core::Size sz, int32_t scale) noexcept {
    return core::Size{
        .width = physical_to_logical(sz.width, scale),
        .height = physical_to_logical(sz.height, scale)
    };
}

[[nodiscard]] constexpr core::Size logical_to_physical(core::Size sz, int32_t scale) noexcept {
    return core::Size{
        .width = logical_to_physical(sz.width, scale),
        .height = logical_to_physical(sz.height, scale)
    };
}

[[nodiscard]] constexpr core::Rect physical_to_logical(core::Rect r, int32_t scale) noexcept {
    return core::Rect{
        .x = physical_to_logical(r.x, scale),
        .y = physical_to_logical(r.y, scale),
        .width = physical_to_logical(r.width, scale),
        .height = physical_to_logical(r.height, scale)
    };
}

[[nodiscard]] constexpr core::Rect logical_to_physical(core::Rect r, int32_t scale) noexcept {
    return core::Rect{
        .x = logical_to_physical(r.x, scale),
        .y = logical_to_physical(r.y, scale),
        .width = logical_to_physical(r.width, scale),
        .height = logical_to_physical(r.height, scale)
    };
}

// Swaps or preserves dimensions based on Wayland output transform
void apply_transform_to_dimensions(DisplayTransform transform,
                                   int32_t in_width, int32_t in_height,
                                   int32_t& out_width, int32_t& out_height) noexcept;

// Central Available Geometry Model
struct AvailableGeometry {
    core::Rect full_bounds;    // Full display boundary in logical coordinates
    core::Rect safe_bounds;    // Usable area excluding hardware cutouts / safe insets
    core::Rect shell_bounds;   // Area occupied by Shell chrome (status bar, dock)
    core::Rect window_bounds;  // Area available for application windows (safe_bounds - shell_bounds)

    constexpr bool operator==(const AvailableGeometry& other) const = default;
};

// Shell Layout Reservations (communicated from Shell to DisplayPolicy)
struct ShellReservations {
    core::Rect status_region;
    core::Rect dock_region;
    core::Rect overlay_region;

    constexpr bool operator==(const ShellReservations& other) const = default;
};

} // namespace ldde::display

