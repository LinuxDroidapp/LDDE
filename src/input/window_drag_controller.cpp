#include "ldde/input/window_drag_controller.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::input {

bool WindowDragController::start(
    window::WindowId id,
    const core::Point& touch_pos,
    const core::Rect& initial_geom,
    window::WindowManager& wm) {

    if (is_active_) {
        cancel(wm);
    }

    bool started = wm.start_move(id, touch_pos, true /* is_touch */);
    if (!started) {
        LDDE_LOG_WARN(Input, "WindowManager failed to start move for window " << id);
        return false;
    }

    is_active_ = true;
    window_id_ = id;
    start_touch_pos_ = touch_pos;
    initial_geom_ = initial_geom;
    current_geom_ = initial_geom;

    LDDE_LOG_INFO(Input, "WindowDragController started move for window " << id
                         << " at (" << touch_pos.x << ", " << touch_pos.y << ")");
    return true;
}

core::Rect WindowDragController::update(const core::Point& touch_pos, window::WindowManager& wm) {
    if (!is_active_) {
        return initial_geom_;
    }

    current_geom_ = wm.update_move(touch_pos);
    return current_geom_;
}

core::Rect WindowDragController::finish(window::WindowManager& wm) {
    if (!is_active_) {
        return initial_geom_;
    }

    core::Rect final_geom = wm.end_move();
    LDDE_LOG_INFO(Input, "WindowDragController finished move for window "
                         << (window_id_ ? std::to_string(*window_id_) : "?")
                         << " final bounds: " << final_geom.width << "x" << final_geom.height
                         << " at (" << final_geom.x << ", " << final_geom.y << ")");
    reset();
    return final_geom;
}

core::Rect WindowDragController::cancel(window::WindowManager& wm) {
    if (!is_active_) {
        return initial_geom_;
    }

    core::Rect initial = wm.cancel_move();
    LDDE_LOG_INFO(Input, "WindowDragController cancelled move for window "
                         << (window_id_ ? std::to_string(*window_id_) : "?")
                         << " restored to (" << initial.x << ", " << initial.y << ")");
    reset();
    return initial;
}

void WindowDragController::reset() noexcept {
    is_active_ = false;
    window_id_.reset();
    start_touch_pos_ = {0, 0};
    initial_geom_ = {0, 0, 0, 0};
    current_geom_ = {0, 0, 0, 0};
}

} // namespace ldde::input
