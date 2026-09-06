#include "ldde/window/window_placement.hpp"
#include <algorithm>

namespace ldde::window {

WindowPlacement::WindowPlacement(PlacementConstraints constraints)
    : constraints_(constraints) {}

core::Rect WindowPlacement::get_usable_area(const display::DisplayPolicy& policy) const noexcept {
    return policy.available_window_geometry();
}

core::Rect WindowPlacement::get_usable_area(const display::DisplayInfo& display) const noexcept {
    if (display.available_geometry.window_bounds.width > 0 && display.available_geometry.window_bounds.height > 0) {
        return display.available_geometry.window_bounds;
    }

    int32_t x = constraints_.margin_horizontal;
    int32_t y = constraints_.status_bar_height + constraints_.margin_top;

    int32_t total_w = display.width > 0 ? display.width : 720;
    int32_t total_h = display.height > 0 ? display.height : 1280;

    int32_t width = total_w - (constraints_.margin_horizontal * 2);
    int32_t height = total_h - y - constraints_.dock_height - constraints_.margin_bottom;

    if (width < 100) width = 100;
    if (height < 100) height = 100;

    return core::Rect{x, y, width, height};
}

core::Rect WindowPlacement::calculate_initial_geometry(
    const display::DisplayPolicy& policy,
    size_t existing_window_count,
    const core::Size& requested_size,
    const core::Size& min_size,
    const core::Size& max_size) const noexcept {
    return policy.calculate_initial_window_geometry(existing_window_count, requested_size, min_size, max_size);
}

core::Rect WindowPlacement::calculate_initial_geometry(
    const display::DisplayInfo& display,
    size_t existing_window_count,
    const core::Size& requested_size,
    const core::Size& min_size,
    const core::Size& max_size) const noexcept {

    core::Rect usable = get_usable_area(display);
    bool is_portrait = display.is_portrait();

    int32_t target_w = 0;
    int32_t target_h = 0;

    if (requested_size.width > 0 && requested_size.height > 0) {
        target_w = requested_size.width;
        target_h = requested_size.height;
    } else if (is_portrait) {
        // Portrait phone mode: ~88% usable width, ~60% usable height
        target_w = (usable.width * 88) / 100;
        target_h = (usable.height * 60) / 100;
    } else {
        // Landscape mode: ~65% usable width, ~70% usable height
        target_w = (usable.width * 65) / 100;
        target_h = (usable.height * 70) / 100;
        if (target_w < 600 && usable.width >= 600) target_w = 600;
        if (target_h < 400 && usable.height >= 400) target_h = 400;
    }

    // Apply min/max constraints
    if (min_size.width > 0 && target_w < min_size.width) target_w = min_size.width;
    if (min_size.height > 0 && target_h < min_size.height) target_h = min_size.height;
    if (max_size.width > 0 && target_w > max_size.width) target_w = max_size.width;
    if (max_size.height > 0 && target_h > max_size.height) target_h = max_size.height;

    // Never exceed usable area
    if (target_w > usable.width) target_w = usable.width;
    if (target_h > usable.height) target_h = usable.height;

    int32_t x = usable.x;
    int32_t y = usable.y;

    if (is_portrait) {
        // Centered horizontally, cascade vertically
        x = usable.x + (usable.width - target_w) / 2;
        int32_t offset_y = static_cast<int32_t>((existing_window_count % 5) * 24);
        if (usable.y + offset_y + target_h <= usable.y + usable.height) {
            y = usable.y + offset_y;
        } else {
            y = usable.y;
        }
    } else {
        // Cascading diagonally
        int32_t max_offset_x = std::max(0, usable.width - target_w);
        int32_t max_offset_y = std::max(0, usable.height - target_h);

        int32_t step = constraints_.cascade_step;
        int32_t offset_x = (max_offset_x > 0)
            ? static_cast<int32_t>((existing_window_count * static_cast<size_t>(step)) % static_cast<size_t>(max_offset_x + 1))
            : 0;
        int32_t offset_y = (max_offset_y > 0)
            ? static_cast<int32_t>((existing_window_count * static_cast<size_t>(step)) % static_cast<size_t>(max_offset_y + 1))
            : 0;

        x = usable.x + offset_x;
        y = usable.y + offset_y;
    }

    return clamp_to_usable(core::Rect{x, y, target_w, target_h}, usable);
}

core::Rect WindowPlacement::clamp_to_usable(
    const core::Rect& geom,
    const core::Rect& usable_area,
    int32_t min_visible_titlebar) const noexcept {

    core::Rect clamped = geom;

    // Minimum size guarantee
    if (clamped.width > usable_area.width) clamped.width = usable_area.width;
    if (clamped.height > usable_area.height) clamped.height = usable_area.height;
    if (clamped.width < 100) clamped.width = std::min(100, usable_area.width);
    if (clamped.height < 100) clamped.height = std::min(100, usable_area.height);

    // Titlebar must remain accessible: top of window cannot be above usable.y
    if (clamped.y < usable_area.y) {
        clamped.y = usable_area.y;
    }
    // Bottom of titlebar must be at least min_visible_titlebar inside usable area
    int32_t max_y = usable_area.y + usable_area.height - min_visible_titlebar;
    if (clamped.y > max_y) {
        clamped.y = max_y;
    }

    // Horizontal clamping: at least 48px visible horizontally
    int32_t min_x = usable_area.x - clamped.width + 48;
    int32_t max_x = usable_area.x + usable_area.width - 48;
    if (clamped.x < min_x) clamped.x = min_x;
    if (clamped.x > max_x) clamped.x = max_x;

    return clamped;
}

} // namespace ldde::window
