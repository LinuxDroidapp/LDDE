#include "ldde/input/window_control_interaction.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::input {

void WindowControlInteraction::press(
    window::WindowId id,
    HitTargetType target,
    const core::Rect& touch_rect) {

    window_id_ = id;
    target_ = target;
    target_rect_ = touch_rect;
    is_pressed_ = true;

    LDDE_LOG_DEBUG(Input, "WindowControlInteraction pressed button "
                          << hit_target_type_name(target)
                          << " on window " << id);
}

bool WindowControlInteraction::update_point(const core::Point& current_pos) {
    if (!window_id_) {
        return false;
    }

    bool inside = target_rect_.contains(current_pos);
    if (inside != is_pressed_) {
        is_pressed_ = inside;
        LDDE_LOG_DEBUG(Input, "WindowControlInteraction button "
                              << hit_target_type_name(target_)
                              << " pressed state changed: " << is_pressed_);
    }
    return is_pressed_;
}

bool WindowControlInteraction::release(const core::Point& release_pos, window::WindowManager& wm) {
    if (!window_id_) {
        return false;
    }

    window::WindowId id = *window_id_;
    HitTargetType target = target_;
    bool inside = target_rect_.contains(release_pos) && is_pressed_;

    reset();

    if (!inside) {
        LDDE_LOG_DEBUG(Input, "WindowControlInteraction released outside target button; cancelled");
        return false;
    }

    LDDE_LOG_INFO(Input, "WindowControlInteraction executing "
                         << hit_target_type_name(target)
                         << " on window " << id);

    switch (target) {
        case HitTargetType::CloseControl: {
            auto s = wm.close(id);
            return s.is_ok();
        }
        case HitTargetType::MaximizeControl: {
            auto s = wm.toggle_maximize(id);
            return s.is_ok();
        }
        case HitTargetType::MinimizeControl: {
            auto s = wm.minimize(id);
            return s.is_ok();
        }
        default:
            break;
    }

    return false;
}

void WindowControlInteraction::cancel() noexcept {
    reset();
}

void WindowControlInteraction::reset() noexcept {
    window_id_.reset();
    target_ = HitTargetType::None;
    target_rect_ = {0, 0, 0, 0};
    is_pressed_ = false;
}

} // namespace ldde::input

