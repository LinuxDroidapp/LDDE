#include "ldde/display/display_policy.hpp"
#include <algorithm>

namespace ldde::display {

DisplayPolicy::DisplayPolicy() {
    info_.logical_width = 720;
    info_.logical_height = 1280;
    info_.width = 720;
    info_.height = 1280;
    info_.pixel_width = 720;
    info_.pixel_height = 1280;
    info_.scale = 1;
    info_.transform = DisplayTransform::Normal;
    recalculate();
}

DisplayPolicy::DisplayPolicy(const DisplayInfo& info)
    : info_(info) {
    recalculate();
}

void DisplayPolicy::update_display(const DisplayInfo& info) {
    info_ = info;
    recalculate();
}

void DisplayPolicy::set_shell_reservations(const ShellReservations& reservations) {
    reservations_ = reservations;
    recalculate();
}

void DisplayPolicy::set_layout_class_override(std::optional<LayoutClass> override_class) {
    layout_class_override_ = override_class;
    recalculate();
}

void DisplayPolicy::set_safe_insets_override(std::optional<SafeInsets> override_insets) {
    safe_insets_override_ = override_insets;
    recalculate();
}

void DisplayPolicy::recalculate() {
    int32_t log_w = (info_.logical_width > 0) ? info_.logical_width : ((info_.width > 0) ? info_.width : 720);
    int32_t log_h = (info_.logical_height > 0) ? info_.logical_height : ((info_.height > 0) ? info_.height : 1280);

    scale_policy_ = ScalePolicy(info_.scale > 0 ? info_.scale : 1);
    orientation_ = derive_orientation(info_.transform, log_w, log_h);
    layout_class_ = classify_layout(log_w, log_h, orientation_, info_.physical_width_mm, info_.physical_height_mm, layout_class_override_);
    multi_window_hint_ = derive_multi_window_hint(layout_class_);
    metrics_ = LayoutMetrics::compute(layout_class_, scale_policy_, orientation_, info_.physical_width_mm, info_.physical_height_mm);

    // Full screen bounds
    available_geometry_.full_bounds = core::Rect{
        .x = info_.logical_x,
        .y = info_.logical_y,
        .width = log_w,
        .height = log_h
    };

    // Safe area
    SafeInsets insets = safe_insets_override_.value_or(info_.safe_insets);
    available_geometry_.safe_bounds = core::Rect{
        .x = available_geometry_.full_bounds.x + insets.left,
        .y = available_geometry_.full_bounds.y + insets.top,
        .width = std::max(0, available_geometry_.full_bounds.width - insets.left - insets.right),
        .height = std::max(0, available_geometry_.full_bounds.height - insets.top - insets.bottom)
    };

    // Calculate window bounds: safe bounds minus shell reservations and margins
    int32_t margin_h = metrics_.content_margin_px;
    int32_t margin_top = metrics_.shell_spacing_px;
    int32_t margin_bot = metrics_.shell_spacing_px;

    int32_t status_h = (reservations_.status_region.height > 0)
        ? reservations_.status_region.height
        : metrics_.status_bar_height_px;

    int32_t dock_h = (reservations_.dock_region.height > 0)
        ? reservations_.dock_region.height
        : metrics_.dock_height_px;

    int32_t win_x = available_geometry_.safe_bounds.x + margin_h;
    int32_t win_y = available_geometry_.safe_bounds.y + status_h + margin_top;
    int32_t win_w = std::max(metrics_.min_window_width_px,
                             available_geometry_.safe_bounds.width - (margin_h * 2));
    int32_t win_h = std::max(metrics_.min_window_height_px,
                             available_geometry_.safe_bounds.height - status_h - margin_top - dock_h - margin_bot);

    available_geometry_.window_bounds = core::Rect{
        .x = win_x,
        .y = win_y,
        .width = win_w,
        .height = win_h
    };

    available_geometry_.shell_bounds = core::Rect{
        .x = available_geometry_.safe_bounds.x,
        .y = available_geometry_.safe_bounds.y,
        .width = available_geometry_.safe_bounds.width,
        .height = available_geometry_.safe_bounds.height
    };
}

core::Rect DisplayPolicy::maximized_geometry() const noexcept {
    return available_geometry_.window_bounds;
}

core::Rect DisplayPolicy::fullscreen_geometry() const noexcept {
    return available_geometry_.full_bounds;
}

core::Size DisplayPolicy::default_window_size(
    const core::Size& requested_size,
    const core::Size& min_size,
    const core::Size& max_size) const noexcept {

    const core::Rect& usable = available_geometry_.window_bounds;
    int32_t target_w = 0;
    int32_t target_h = 0;

    if (requested_size.width > 0 && requested_size.height > 0) {
        target_w = requested_size.width;
        target_h = requested_size.height;
    } else {
        switch (layout_class_) {
            case LayoutClass::Compact:
                if (is_portrait()) {
                    // Mobile phone portrait
                    target_w = (usable.width * 88) / 100;
                    target_h = (usable.height * 60) / 100;
                } else {
                    target_w = (usable.width * 75) / 100;
                    target_h = (usable.height * 75) / 100;
                }
                break;
            case LayoutClass::Standard:
                if (is_portrait()) {
                    target_w = (usable.width * 80) / 100;
                    target_h = (usable.height * 65) / 100;
                } else {
                    // Landscape phone / foldable
                    target_w = (usable.width * 65) / 100;
                    target_h = (usable.height * 70) / 100;
                    if (target_w < 600 && usable.width >= 600) target_w = 600;
                    if (target_h < 400 && usable.height >= 400) target_h = 400;
                }
                break;
            case LayoutClass::Expanded:
                // Tablet / external monitor
                target_w = (usable.width * 55) / 100;
                target_h = (usable.height * 65) / 100;
                if (target_w < 640 && usable.width >= 640) target_w = 640;
                if (target_h < 480 && usable.height >= 480) target_h = 480;
                break;
        }
    }

    int32_t effective_min_w = std::max(min_size.width, metrics_.min_window_width_px);
    int32_t effective_min_h = std::max(min_size.height, metrics_.min_window_height_px);

    if (target_w < effective_min_w) target_w = effective_min_w;
    if (target_h < effective_min_h) target_h = effective_min_h;

    if (max_size.width > 0 && target_w > max_size.width) target_w = max_size.width;
    if (max_size.height > 0 && target_h > max_size.height) target_h = max_size.height;

    if (target_w > usable.width) target_w = usable.width;
    if (target_h > usable.height) target_h = usable.height;

    return core::Size{.width = target_w, .height = target_h};
}

core::Rect DisplayPolicy::calculate_initial_window_geometry(
    size_t window_index,
    const core::Size& requested_size,
    const core::Size& min_size,
    const core::Size& max_size) const noexcept {

    const core::Rect& usable = available_geometry_.window_bounds;
    core::Size sz = default_window_size(requested_size, min_size, max_size);

    int32_t x = usable.x;
    int32_t y = usable.y;

    if (is_portrait()) {
        // Horizontally centered, cascade vertically
        x = usable.x + (usable.width - sz.width) / 2;
        int32_t offset_y = static_cast<int32_t>((window_index % 5) * metrics_.cascade_step_px);
        if (usable.y + offset_y + sz.height <= usable.y + usable.height) {
            y = usable.y + offset_y;
        } else {
            y = usable.y;
        }
    } else {
        // Cascading diagonally
        int32_t max_offset_x = std::max(0, usable.width - sz.width);
        int32_t max_offset_y = std::max(0, usable.height - sz.height);

        int32_t step = metrics_.cascade_step_px;
        int32_t offset_x = (max_offset_x > 0)
            ? static_cast<int32_t>((window_index * static_cast<size_t>(step)) % static_cast<size_t>(max_offset_x + 1))
            : 0;
        int32_t offset_y = (max_offset_y > 0)
            ? static_cast<int32_t>((window_index * static_cast<size_t>(step)) % static_cast<size_t>(max_offset_y + 1))
            : 0;

        x = usable.x + offset_x;
        y = usable.y + offset_y;
    }

    return constrain_window_geometry(core::Rect{.x = x, .y = y, .width = sz.width, .height = sz.height}, min_size);
}

core::Rect DisplayPolicy::constrain_window_geometry(
    const core::Rect& requested,
    const core::Size& min_size) const noexcept {

    const core::Rect& usable = available_geometry_.window_bounds;
    core::Rect clamped = requested;

    int32_t effective_min_w = std::max(min_size.width, metrics_.min_window_width_px);
    int32_t effective_min_h = std::max(min_size.height, metrics_.min_window_height_px);

    if (clamped.width < effective_min_w) clamped.width = std::min(effective_min_w, usable.width);
    if (clamped.height < effective_min_h) clamped.height = std::min(effective_min_h, usable.height);

    if (clamped.width > usable.width) clamped.width = usable.width;
    if (clamped.height > usable.height) clamped.height = usable.height;

    // Titlebar must remain accessible: top of window cannot be above usable.y
    if (clamped.y < usable.y) {
        clamped.y = usable.y;
    }

    // Titlebar must remain visible: cannot push bottom of titlebar below usable area
    int32_t min_visible_titlebar = metrics_.title_bar_height_px > 0 ? metrics_.title_bar_height_px : 36;
    int32_t max_y = usable.y + usable.height - min_visible_titlebar;
    if (clamped.y > max_y) {
        clamped.y = max_y;
    }

    // Horizontal clamping: at least minimum target remains on screen
    int32_t min_visible_h = metrics_.window_control_target_px > 0 ? metrics_.window_control_target_px : 48;
    int32_t min_x = usable.x - clamped.width + min_visible_h;
    int32_t max_x = usable.x + usable.width - min_visible_h;
    if (clamped.x < min_x) clamped.x = min_x;
    if (clamped.x > max_x) clamped.x = max_x;

    return clamped;
}

core::Rect DisplayPolicy::restore_window_geometry(
    const std::optional<core::Rect>& saved_geometry,
    const core::Size& min_size) const noexcept {

    if (saved_geometry.has_value()) {
        return constrain_window_geometry(saved_geometry.value(), min_size);
    }
    return calculate_initial_window_geometry(0, {0, 0}, min_size);
}

} // namespace ldde::display

