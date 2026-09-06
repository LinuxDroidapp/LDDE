#pragma once

#include <cstdint>
#include "ldde/config/config.hpp"
#include "ldde/display/display_policy.hpp"

namespace ldde::input {

struct TouchInteractionPolicy {
    int32_t move_threshold_px = 10;
    uint32_t double_tap_interval_ms = 350;
    int32_t double_tap_slop_px = 16;
    int32_t control_touch_target_px = 48;
    int32_t resize_touch_target_px = 28;
    int32_t header_touch_height_px = 48;
    bool touch_enabled = true;
    bool double_tap_enabled = true;

    static TouchInteractionPolicy from_config_and_display(
        const config::Config& config,
        const display::DisplayPolicy& display_policy);

    void update_from_display(const display::DisplayPolicy& display_policy);
};

} // namespace ldde::input
