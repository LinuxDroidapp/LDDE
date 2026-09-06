#include "ldde/input/touch_interaction_policy.hpp"

namespace ldde::input {

TouchInteractionPolicy TouchInteractionPolicy::from_config_and_display(
    const config::Config& config,
    const display::DisplayPolicy& display_policy) {
    TouchInteractionPolicy policy;
    policy.move_threshold_px = static_cast<int32_t>(config.get_int_or("input", "touch_move_threshold", 10));
    policy.double_tap_interval_ms = static_cast<uint32_t>(config.get_int_or("input", "touch_double_tap_timeout", 350));
    policy.double_tap_slop_px = static_cast<int32_t>(config.get_int_or("input", "touch_double_tap_distance", 16));
    policy.touch_enabled = config.get_bool_or("input", "touch_enabled", true);
    policy.double_tap_enabled = config.get_bool_or("input", "touch_double_tap_enabled", true);

    policy.update_from_display(display_policy);

    // If config explicitly specified resize target, let it take precedence
    int64_t cfg_resize = config.get_int_or("input", "touch_resize_target", -1);
    if (cfg_resize > 0) {
        policy.resize_touch_target_px = static_cast<int32_t>(cfg_resize);
    }

    return policy;
}

void TouchInteractionPolicy::update_from_display(const display::DisplayPolicy& display_policy) {
    const auto& metrics = display_policy.metrics();
    control_touch_target_px = metrics.window_control_target_px;
    resize_touch_target_px = metrics.resize_target_px;
    header_touch_height_px = metrics.title_bar_height_px;
}

} // namespace ldde::input
