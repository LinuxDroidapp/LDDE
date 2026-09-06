#pragma once

#include <cstdint>
#include "ldde/display/orientation.hpp"
#include "ldde/display/scale_policy.hpp"
#include "ldde/display/responsive_layout.hpp"

namespace ldde::display {

struct LayoutMetrics {
    int32_t minimum_touch_target_px = 48;
    int32_t window_control_target_px = 48;
    int32_t window_control_visual_size_px = 24;
    int32_t resize_target_px = 24;
    int32_t title_bar_height_px = 44;
    int32_t status_bar_height_px = 36;
    int32_t dock_height_px = 64;
    int32_t shell_spacing_px = 8;
    int32_t content_margin_px = 8;
    int32_t corner_radius_px = 12;
    int32_t min_window_width_px = 200;
    int32_t min_window_height_px = 150;
    int32_t cascade_step_px = 32;

    constexpr bool operator==(const LayoutMetrics& other) const = default;

    [[nodiscard]] static LayoutMetrics compute(
        LayoutClass layout_class,
        const ScalePolicy& scale_policy,
        Orientation orientation,
        int32_t physical_width_mm = 0,
        int32_t physical_height_mm = 0) noexcept;
};

} // namespace ldde::display

