#include "ldde/display/layout_metrics.hpp"

namespace ldde::display {

LayoutMetrics LayoutMetrics::compute(
    LayoutClass layout_class,
    const ScalePolicy& /*scale_policy*/,
    Orientation orientation,
    int32_t /*physical_width_mm*/,
    int32_t /*physical_height_mm*/) noexcept {

    LayoutMetrics metrics;

    switch (layout_class) {
        case LayoutClass::Compact:
            // Compact mobile portrait / narrow screen
            metrics.minimum_touch_target_px = 48;
            metrics.window_control_target_px = 48;
            metrics.window_control_visual_size_px = 22;
            metrics.resize_target_px = 24;
            metrics.title_bar_height_px = 44;
            metrics.status_bar_height_px = 40;
            metrics.dock_height_px = 68;
            metrics.shell_spacing_px = 8;
            metrics.content_margin_px = 8;
            metrics.corner_radius_px = 12;
            metrics.min_window_width_px = 200;
            metrics.min_window_height_px = 150;
            metrics.cascade_step_px = 24;
            break;

        case LayoutClass::Standard:
            // Standard landscape phone / foldable
            metrics.minimum_touch_target_px = 48;
            metrics.window_control_target_px = 44;
            metrics.window_control_visual_size_px = 24;
            metrics.resize_target_px = 20;
            metrics.title_bar_height_px = 40;
            metrics.status_bar_height_px = 36;
            metrics.dock_height_px = 64;
            metrics.shell_spacing_px = 8;
            metrics.content_margin_px = 8;
            metrics.corner_radius_px = 10;
            metrics.min_window_width_px = 260;
            metrics.min_window_height_px = 180;
            metrics.cascade_step_px = 32;
            break;

        case LayoutClass::Expanded:
            // Tablet / desktop / external display
            metrics.minimum_touch_target_px = 40;
            metrics.window_control_target_px = 36;
            metrics.window_control_visual_size_px = 20;
            metrics.resize_target_px = 16;
            metrics.title_bar_height_px = 36;
            metrics.status_bar_height_px = 32;
            metrics.dock_height_px = 56;
            metrics.shell_spacing_px = 12;
            metrics.content_margin_px = 12;
            metrics.corner_radius_px = 8;
            metrics.min_window_width_px = 320;
            metrics.min_window_height_px = 240;
            metrics.cascade_step_px = 36;
            break;
    }

    if (!is_portrait(orientation) && layout_class == LayoutClass::Compact) {
        // Landscape phone: status bar and dock can be slightly more compact vertically
        metrics.status_bar_height_px = 32;
        metrics.dock_height_px = 56;
    }

    return metrics;
}

} // namespace ldde::display

