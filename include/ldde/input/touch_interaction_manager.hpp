#pragma once

#include <optional>
#include <memory>
#include "ldde/core/types.hpp"
#include "ldde/config/config.hpp"
#include "ldde/display/display_manager.hpp"
#include "ldde/window/types.hpp"
#include "ldde/window/window_registry.hpp"
#include "ldde/window/window_manager.hpp"
#include "ldde/shell/shell.hpp"
#include "ldde/input/touch_interaction_policy.hpp"
#include "ldde/input/touch_gesture_state.hpp"
#include "ldde/input/touch_hit_testing.hpp"
#include "ldde/input/window_drag_controller.hpp"
#include "ldde/input/window_resize_controller.hpp"
#include "ldde/input/window_control_interaction.hpp"

namespace ldde::input {

class TouchInteractionManager {
public:
    TouchInteractionManager(
        window::WindowManager& window_manager,
        window::WindowRegistry& window_registry,
        display::DisplayManager& display_manager,
        const config::Config& config,
        shell::Shell* shell = nullptr);

    ~TouchInteractionManager() = default;

    TouchInteractionManager(const TouchInteractionManager&) = delete;
    TouchInteractionManager& operator=(const TouchInteractionManager&) = delete;

    // Touch Event Handlers
    bool handle_touch_down(int32_t touch_id, const core::Point& pos, uint32_t time_ms);
    bool handle_touch_motion(int32_t touch_id, const core::Point& pos, uint32_t time_ms);
    bool handle_touch_up(int32_t touch_id, uint32_t time_ms);
    bool handle_touch_cancel(int32_t touch_id);
    void handle_touch_frame();

    // Lifecycle and external observers
    void handle_window_destroyed(window::WindowId id);
    void handle_display_change(const display::DisplayPolicy& policy);
    void cancel_active_interaction() noexcept;
    void reset() noexcept;

    // Component accessors & queries
    [[nodiscard]] GestureState state() const noexcept { return state_machine_.current_state(); }
    [[nodiscard]] std::optional<int32_t> active_touch_id() const noexcept { return active_touch_id_; }
    [[nodiscard]] std::optional<window::WindowId> active_window_id() const noexcept;
    [[nodiscard]] const TouchInteractionPolicy& policy() const noexcept { return policy_; }
    [[nodiscard]] TouchInteractionPolicy& policy() noexcept { return policy_; }
    [[nodiscard]] const TouchHitTesting& hit_testing() const noexcept { return hit_testing_; }
    [[nodiscard]] const WindowDragController& drag_controller() const noexcept { return drag_controller_; }
    [[nodiscard]] const WindowResizeController& resize_controller() const noexcept { return resize_controller_; }
    [[nodiscard]] const WindowControlInteraction& control_interaction() const noexcept { return control_interaction_; }

private:
    window::WindowManager& window_manager_;
    window::WindowRegistry& window_registry_;
    display::DisplayManager& display_manager_;
    shell::Shell* shell_ = nullptr;

    TouchInteractionPolicy policy_;
    TouchGestureStateMachine state_machine_;
    TouchHitTesting hit_testing_;

    WindowDragController drag_controller_;
    WindowResizeController resize_controller_;
    WindowControlInteraction control_interaction_;

    std::optional<int32_t> active_touch_id_;
    core::Point touch_start_pos_{0, 0};
    core::Point current_touch_pos_{0, 0};
    uint32_t touch_start_time_ms_ = 0;
    HitTestResult initial_hit_;

    // Double-tap tracking on title bar
    uint32_t last_tap_time_ms_ = 0;
    core::Point last_tap_pos_{0, 0};
    std::optional<window::WindowId> last_tap_window_id_;
    HitTargetType last_tap_type_ = HitTargetType::None;

    void reset_touch_contact() noexcept;
};

} // namespace ldde::input

