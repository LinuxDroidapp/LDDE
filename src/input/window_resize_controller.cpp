#include "ldde/input/window_resize_controller.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::input {

bool WindowResizeController::start(
    window::WindowId id,
    window::ResizeEdge edge,
    const core::Point& touch_pos,
    const core::Rect& initial_geom,
    window::WindowManager& wm) {

    if (is_active_) {
        cancel(wm);
    }

    bool started = wm.start_resize(id, edge, touch_pos, true /* is_touch */);
    if (!started) {
        LDDE_LOG_WARN(Input, "WindowManager failed to start resize for window " << id);
        return false;
    }

    is_active_ = true;
    window_id_ = id;
    edge_ = edge;
    start_touch_pos_ = touch_pos;
    initial_geom_ = initial_geom;
    current_geom_ = initial_geom;

    LDDE_LOG_INFO(Input, "WindowResizeController started resize for window " << id
                         << " edge=" << window::resize_edge_name(edge)
                         << " at (" << touch_pos.x << ", " << touch_pos.y << ")");
    return true;
}

core::Rect WindowResizeController::update(const core::Point& touch_pos, window::WindowManager& wm) {
    if (!is_active_) {
        return initial_geom_;
    }

    current_geom_ = wm.update_resize(touch_pos);
    return current_geom_;
}

core::Rect WindowResizeController::finish(window::WindowManager& wm) {
    if (!is_active_) {
        return initial_geom_;
    }

    core::Rect final_geom = wm.end_resize();
    LDDE_LOG_INFO(Input, "WindowResizeController finished resize for window "
                         << (window_id_ ? std::to_string(*window_id_) : "?")
                         << " final bounds: " << final_geom.width << "x" << final_geom.height);
    reset();
    return final_geom;
}

core::Rect WindowResizeController::cancel(window::WindowManager& wm) {
    if (!is_active_) {
        return initial_geom_;
    }

    core::Rect initial = wm.cancel_resize();
    LDDE_LOG_INFO(Input, "WindowResizeController cancelled resize for window "
                         << (window_id_ ? std::to_string(*window_id_) : "?")
                         << " restored to " << initial.width << "x" << initial.height);
    reset();
    return initial;
}

void WindowResizeController::reset() noexcept {
    is_active_ = false;
    window_id_.reset();
    edge_ = window::ResizeEdge::None;
    start_touch_pos_ = {0, 0};
    initial_geom_ = {0, 0, 0, 0};
    current_geom_ = {0, 0, 0, 0};
}

} // namespace ldde::input
