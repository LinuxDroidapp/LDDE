#include "ldde/input/touch_interaction_manager.hpp"
#include "ldde/core/logging.hpp"
#include <cmath>

namespace ldde::input {

TouchInteractionManager::TouchInteractionManager(
    window::WindowManager& window_manager,
    window::WindowRegistry& window_registry,
    display::DisplayManager& display_manager,
    const config::Config& config,
    shell::Shell* shell)
    : window_manager_(window_manager),
      window_registry_(window_registry),
      display_manager_(display_manager),
      shell_(shell),
      policy_([&]() {
          auto* p = display_manager_.primary_policy();
          if (p) {
              return TouchInteractionPolicy::from_config_and_display(config, *p);
          }
          display::DisplayInfo dummy;
          dummy.width = 720;
          dummy.height = 1280;
          dummy.logical_width = 720;
          dummy.logical_height = 1280;
          display::DisplayPolicy fallback(dummy);
          return TouchInteractionPolicy::from_config_and_display(config, fallback);
      }()),
      hit_testing_(window_registry_, window_manager_.stacking(), policy_, shell_) {

    LDDE_LOG_INFO(Input, "TouchInteractionManager initialized (touch_enabled="
                         << policy_.touch_enabled
                         << ", move_threshold=" << policy_.move_threshold_px
                         << "px, resize_target=" << policy_.resize_touch_target_px << "px)");
}

std::optional<window::WindowId> TouchInteractionManager::active_window_id() const noexcept {
    if (drag_controller_.is_active()) return drag_controller_.window_id();
    if (resize_controller_.is_active()) return resize_controller_.window_id();
    if (control_interaction_.is_active()) return control_interaction_.window_id();
    return initial_hit_.window_id;
}

bool TouchInteractionManager::handle_touch_down(int32_t touch_id, const core::Point& pos, uint32_t time_ms) {
    if (!policy_.touch_enabled) {
        return false;
    }

    // Only allow single active window manipulation contact
    if (active_touch_id_.has_value()) {
        LDDE_LOG_DEBUG(Input, "Touch DOWN ignored for id " << touch_id
                              << "; interaction already active for id " << *active_touch_id_);
        return false;
    }

    HitTestResult hit = hit_testing_.hit_test(pos);
    if (hit.type == HitTargetType::None || hit.type == HitTargetType::Shell) {
        LDDE_LOG_DEBUG(Input, "Touch DOWN at (" << pos.x << ", " << pos.y
                              << ") hit " << hit_target_type_name(hit.type) << "; no window interaction");
        return false;
    }

    if (!hit.window_id.has_value()) {
        return false;
    }

    active_touch_id_ = touch_id;
    touch_start_pos_ = pos;
    current_touch_pos_ = pos;
    touch_start_time_ms_ = time_ms;
    initial_hit_ = hit;

    window::WindowId win_id = *hit.window_id;

    if (hit.type == HitTargetType::CloseControl ||
        hit.type == HitTargetType::MaximizeControl ||
        hit.type == HitTargetType::MinimizeControl) {

        state_machine_.transition_to(GestureState::ContactPending);
        state_machine_.transition_to(GestureState::ControlPress);
        control_interaction_.press(win_id, hit.type, hit.target_rect);
        LDDE_LOG_DEBUG(Input, "Touch DOWN on window " << win_id << " control: "
                              << hit_target_type_name(hit.type));
        return true;
    }

    if (hit.type == HitTargetType::TitleBar || hit.type == HitTargetType::ResizeEdge) {
        state_machine_.transition_to(GestureState::ContactPending);
        LDDE_LOG_DEBUG(Input, "Touch DOWN on window " << win_id
                              << " region: " << hit_target_type_name(hit.type)
                              << "; pending tap vs drag");
        return true;
    }

    if (hit.type == HitTargetType::WindowContent) {
        state_machine_.transition_to(GestureState::ContactPending);
        state_machine_.transition_to(GestureState::WindowFocus);
        static_cast<void>(window_manager_.activate(win_id));
        LDDE_LOG_DEBUG(Input, "Touch DOWN on window " << win_id << " content; focused");
        return true;
    }

    return false;
}

bool TouchInteractionManager::handle_touch_motion(int32_t touch_id, const core::Point& pos, uint32_t /*time_ms*/) {
    // Fast path: ignore unowned contacts without work or allocations
    if (!active_touch_id_ || *active_touch_id_ != touch_id) {
        return false;
    }

    current_touch_pos_ = pos;
    int32_t dx = pos.x - touch_start_pos_.x;
    int32_t dy = pos.y - touch_start_pos_.y;
    int32_t dist_sq = dx * dx + dy * dy;
    int32_t threshold_sq = policy_.move_threshold_px * policy_.move_threshold_px;

    GestureState cur_state = state_machine_.current_state();

    if (cur_state == GestureState::ContactPending) {
        if (dist_sq >= threshold_sq) {
            if (initial_hit_.type == HitTargetType::TitleBar) {
                auto win = window_registry_.lookup(*initial_hit_.window_id);
                if (win && win->state() != window::WindowState::Minimized &&
                    win->state() != window::WindowState::Fullscreen) {
                    if (state_machine_.transition_to(GestureState::Moving)) {
                        drag_controller_.start(*initial_hit_.window_id, touch_start_pos_, win->geometry(), window_manager_);
                        drag_controller_.update(pos, window_manager_);
                        return true;
                    }
                }
            } else if (initial_hit_.type == HitTargetType::ResizeEdge) {
                auto win = window_registry_.lookup(*initial_hit_.window_id);
                if (win && win->state() != window::WindowState::Minimized &&
                    win->state() != window::WindowState::Maximized &&
                    win->state() != window::WindowState::Fullscreen) {
                    if (state_machine_.transition_to(GestureState::Resizing)) {
                        resize_controller_.start(*initial_hit_.window_id, initial_hit_.resize_edge,
                                                 touch_start_pos_, win->geometry(), window_manager_);
                        resize_controller_.update(pos, window_manager_);
                        return true;
                    }
                }
            }
        }
        return true;
    }

    if (cur_state == GestureState::Moving) {
        drag_controller_.update(pos, window_manager_);
        return true;
    }

    if (cur_state == GestureState::Resizing) {
        resize_controller_.update(pos, window_manager_);
        return true;
    }

    if (cur_state == GestureState::ControlPress) {
        control_interaction_.update_point(pos);
        return true;
    }

    if (cur_state == GestureState::WindowFocus) {
        return true;
    }

    return false;
}

bool TouchInteractionManager::handle_touch_up(int32_t touch_id, uint32_t time_ms) {
    if (!active_touch_id_ || *active_touch_id_ != touch_id) {
        return false;
    }

    GestureState cur_state = state_machine_.current_state();

    if (cur_state == GestureState::ControlPress) {
        control_interaction_.release(current_touch_pos_, window_manager_);
    } else if (cur_state == GestureState::Moving) {
        drag_controller_.finish(window_manager_);
    } else if (cur_state == GestureState::Resizing) {
        resize_controller_.finish(window_manager_);
    } else if (cur_state == GestureState::ContactPending) {
        // Released before threshold: tap!
        if (initial_hit_.window_id) {
            window::WindowId win_id = *initial_hit_.window_id;
            static_cast<void>(window_manager_.activate(win_id));

            if (initial_hit_.type == HitTargetType::TitleBar && policy_.double_tap_enabled) {
                if (last_tap_type_ == HitTargetType::TitleBar &&
                    last_tap_window_id_ == win_id &&
                    time_ms >= last_tap_time_ms_ &&
                    (time_ms - last_tap_time_ms_) <= policy_.double_tap_interval_ms &&
                    std::abs(current_touch_pos_.x - last_tap_pos_.x) <= policy_.double_tap_slop_px &&
                    std::abs(current_touch_pos_.y - last_tap_pos_.y) <= policy_.double_tap_slop_px) {

                    LDDE_LOG_INFO(Input, "Double tap on title bar detected; toggling maximize for window " << win_id);
                    static_cast<void>(window_manager_.toggle_maximize(win_id));
                    last_tap_type_ = HitTargetType::None;
                    last_tap_window_id_.reset();
                } else {
                    last_tap_time_ms_ = time_ms;
                    last_tap_pos_ = current_touch_pos_;
                    last_tap_window_id_ = win_id;
                    last_tap_type_ = HitTargetType::TitleBar;
                }
            }
        }
    }

    state_machine_.transition_to(GestureState::Completed);
    state_machine_.transition_to(GestureState::Idle);
    reset_touch_contact();
    return true;
}

bool TouchInteractionManager::handle_touch_cancel(int32_t touch_id) {
    if (!active_touch_id_ || *active_touch_id_ != touch_id) {
        return false;
    }

    cancel_active_interaction();
    return true;
}

void TouchInteractionManager::handle_touch_frame() {
    // Frame demarcation in Wayland protocol; no state transition required
}

void TouchInteractionManager::handle_window_destroyed(window::WindowId id) {
    if (active_window_id() == id || (initial_hit_.window_id && *initial_hit_.window_id == id)) {
        LDDE_LOG_INFO(Input, "Window " << id << " destroyed during touch interaction; cancelling cleanly");
        drag_controller_.reset();
        resize_controller_.reset();
        control_interaction_.reset();

        if (state_machine_.current_state() != GestureState::Idle) {
            state_machine_.transition_to(GestureState::GestureCancelled);
            state_machine_.transition_to(GestureState::Idle);
        }
        reset_touch_contact();
    }

    if (last_tap_window_id_ == id) {
        last_tap_window_id_.reset();
        last_tap_type_ = HitTargetType::None;
    }
}

void TouchInteractionManager::handle_display_change(const display::DisplayPolicy& policy) {
    policy_.update_from_display(policy);

    // If an interaction is currently active during rotation, cleanly cancel it to prevent coordinate jump
    if (state_machine_.is_active()) {
        LDDE_LOG_INFO(Input, "Display orientation/size changed during active touch interaction; cancelling");
        cancel_active_interaction();
    }
}

void TouchInteractionManager::cancel_active_interaction() noexcept {
    GestureState st = state_machine_.current_state();
    if (st == GestureState::Moving) {
        drag_controller_.cancel(window_manager_);
    } else if (st == GestureState::Resizing) {
        resize_controller_.cancel(window_manager_);
    } else if (st == GestureState::ControlPress) {
        control_interaction_.cancel();
    }

    if (st != GestureState::Idle) {
        state_machine_.transition_to(GestureState::GestureCancelled);
        state_machine_.transition_to(GestureState::Idle);
    }
    reset_touch_contact();
}

void TouchInteractionManager::reset() noexcept {
    cancel_active_interaction();
    last_tap_time_ms_ = 0;
    last_tap_pos_ = {0, 0};
    last_tap_window_id_.reset();
    last_tap_type_ = HitTargetType::None;
}

void TouchInteractionManager::reset_touch_contact() noexcept {
    active_touch_id_.reset();
    touch_start_pos_ = {0, 0};
    current_touch_pos_ = {0, 0};
    touch_start_time_ms_ = 0;
    initial_hit_ = HitTestResult{};
}

} // namespace ldde::input

