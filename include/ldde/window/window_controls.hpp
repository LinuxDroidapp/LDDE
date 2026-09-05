#pragma once

#include <cstdint>
#include "ldde/core/types.hpp"

namespace ldde::window {

enum class WindowControlButton {
    None = 0,
    Close,
    MaximizeRestore,
    Minimize,
    TitleDragArea
};

struct HeaderHitResult {
    WindowControlButton button = WindowControlButton::None;
    bool is_double_tap = false;
};

struct WindowControlsConfig {
    int32_t pointer_header_height = 36;
    int32_t pointer_button_width = 36;
    int32_t touch_header_height = 48;
    int32_t touch_button_width = 48;
    uint32_t double_tap_interval_ms = 350;
    int32_t double_tap_slop_px = 12;
};

class WindowControls {
public:
    explicit WindowControls(WindowControlsConfig config = {});

    [[nodiscard]] int32_t header_height(bool is_touch = false) const noexcept;
    [[nodiscard]] int32_t button_width(bool is_touch = false) const noexcept;

    [[nodiscard]] core::Rect get_header_rect(const core::Rect& window_geom, bool is_touch = false) const noexcept;
    [[nodiscard]] core::Rect get_close_button_rect(const core::Rect& window_geom, bool is_touch = false) const noexcept;
    [[nodiscard]] core::Rect get_maximize_button_rect(const core::Rect& window_geom, bool is_touch = false) const noexcept;
    [[nodiscard]] core::Rect get_minimize_button_rect(const core::Rect& window_geom, bool is_touch = false) const noexcept;

    [[nodiscard]] HeaderHitResult hit_test(
        const core::Rect& window_geom,
        const core::Point& point,
        uint32_t timestamp_ms,
        bool is_touch = false) noexcept;

    void reset_tap_tracking() noexcept;

private:
    WindowControlsConfig config_;

    uint32_t last_tap_time_ms_ = 0;
    core::Point last_tap_pos_{0, 0};
    WindowControlButton last_tap_button_ = WindowControlButton::None;
};

} // namespace ldde::window

