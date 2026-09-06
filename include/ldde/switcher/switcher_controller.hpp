#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include "ldde/core/error.hpp"
#include "ldde/core/types.hpp"
#include "ldde/window/window_manager.hpp"
#include "ldde/switcher/switcher_state.hpp"
#include "ldde/switcher/switcher_model.hpp"
#include "ldde/switcher/switcher_layout.hpp"

namespace ldde::switcher {

class SwitcherController {
public:
    using RenderRequestCallback = std::function<void()>;

    SwitcherController(SwitcherStateMachine& state_machine,
                       SwitcherModel& model,
                       SwitcherLayout& layout,
                       window::WindowManager& window_manager);
    ~SwitcherController() = default;

    void open();
    void close();
    void cancel();
    void toggle();

    void select_next();
    void select_prev();
    void select_index(size_t index);
    core::Status activate_selected();

    [[nodiscard]] size_t selected_index() const noexcept { return selected_index_; }
    [[nodiscard]] SwitcherStateMachine& state_machine() noexcept { return state_machine_; }
    [[nodiscard]] const SwitcherStateMachine& state_machine() const noexcept { return state_machine_; }

    // Input handlers
    bool handle_touch_down(int32_t x, int32_t y);
    bool handle_touch_motion(int32_t x, int32_t y);
    bool handle_touch_up(int32_t x, int32_t y);
    void handle_touch_cancel();

    bool handle_pointer_motion(int32_t x, int32_t y);
    bool handle_pointer_button(uint32_t button, uint32_t state, int32_t x, int32_t y);
    bool handle_pointer_axis(double delta_x, double delta_y);

    bool handle_key(uint32_t key_symbol, uint32_t state = 1, uint32_t modifiers = 0);

    void on_request_render(RenderRequestCallback cb) {
        on_request_render_ = std::move(cb);
    }

private:
    SwitcherStateMachine& state_machine_;
    SwitcherModel& model_;
    SwitcherLayout& layout_;
    window::WindowManager& window_manager_;

    size_t selected_index_ = 0;
    std::optional<window::WindowId> initial_active_wid_;

    bool is_touch_active_ = false;
    int32_t touch_start_x_ = 0;
    int32_t touch_start_y_ = 0;
    int32_t touch_last_x_ = 0;
    int32_t touch_last_y_ = 0;
    bool touch_dragged_ = false;

    RenderRequestCallback on_request_render_;

    void sync_selected_item();
    void request_render();
};

} // namespace ldde::switcher
