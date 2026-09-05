#include "ldde/window/window_controls.hpp"
#include <cstdlib>

namespace ldde::window {

WindowControls::WindowControls(WindowControlsConfig config)
    : config_(config) {}

int32_t WindowControls::header_height(bool is_touch) const noexcept {
    return is_touch ? config_.touch_header_height : config_.pointer_header_height;
}

int32_t WindowControls::button_width(bool is_touch) const noexcept {
    return is_touch ? config_.touch_button_width : config_.pointer_button_width;
}

core::Rect WindowControls::get_header_rect(const core::Rect& window_geom, bool is_touch) const noexcept {
    return core::Rect{window_geom.x, window_geom.y, window_geom.width, header_height(is_touch)};
}

core::Rect WindowControls::get_close_button_rect(const core::Rect& window_geom, bool is_touch) const noexcept {
    int32_t h = header_height(is_touch);
    int32_t bw = button_width(is_touch);
    return core::Rect{window_geom.x + window_geom.width - bw, window_geom.y, bw, h};
}

core::Rect WindowControls::get_maximize_button_rect(const core::Rect& window_geom, bool is_touch) const noexcept {
    int32_t h = header_height(is_touch);
    int32_t bw = button_width(is_touch);
    return core::Rect{window_geom.x + window_geom.width - (2 * bw), window_geom.y, bw, h};
}

core::Rect WindowControls::get_minimize_button_rect(const core::Rect& window_geom, bool is_touch) const noexcept {
    int32_t h = header_height(is_touch);
    int32_t bw = button_width(is_touch);
    return core::Rect{window_geom.x + window_geom.width - (3 * bw), window_geom.y, bw, h};
}

HeaderHitResult WindowControls::hit_test(
    const core::Rect& window_geom,
    const core::Point& point,
    uint32_t timestamp_ms,
    bool is_touch) noexcept {

    core::Rect header = get_header_rect(window_geom, is_touch);
    if (!header.contains(point)) {
        return HeaderHitResult{WindowControlButton::None, false};
    }

    core::Rect close_btn = get_close_button_rect(window_geom, is_touch);
    if (close_btn.contains(point)) {
        reset_tap_tracking();
        return HeaderHitResult{WindowControlButton::Close, false};
    }

    core::Rect max_btn = get_maximize_button_rect(window_geom, is_touch);
    if (max_btn.contains(point)) {
        reset_tap_tracking();
        return HeaderHitResult{WindowControlButton::MaximizeRestore, false};
    }

    core::Rect min_btn = get_minimize_button_rect(window_geom, is_touch);
    if (min_btn.contains(point)) {
        reset_tap_tracking();
        return HeaderHitResult{WindowControlButton::Minimize, false};
    }

    // Inside header but not on a button -> Title drag area
    bool is_double = false;
    if (last_tap_button_ == WindowControlButton::TitleDragArea &&
        timestamp_ms >= last_tap_time_ms_ &&
        (timestamp_ms - last_tap_time_ms_) <= config_.double_tap_interval_ms &&
        std::abs(point.x - last_tap_pos_.x) <= config_.double_tap_slop_px &&
        std::abs(point.y - last_tap_pos_.y) <= config_.double_tap_slop_px) {
        is_double = true;
        reset_tap_tracking();
    } else {
        last_tap_time_ms_ = timestamp_ms;
        last_tap_pos_ = point;
        last_tap_button_ = WindowControlButton::TitleDragArea;
    }

    return HeaderHitResult{WindowControlButton::TitleDragArea, is_double};
}

void WindowControls::reset_tap_tracking() noexcept {
    last_tap_time_ms_ = 0;
    last_tap_pos_ = {0, 0};
    last_tap_button_ = WindowControlButton::None;
}

} // namespace ldde::window

