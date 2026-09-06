#pragma once

#include <cstdint>
#include <functional>
#include "ldde/core/types.hpp"
#include "ldde/system/system_ui_state.hpp"
#include "ldde/system/system_ui_layout.hpp"
#include "ldde/system/quick_controls.hpp"
#include "ldde/system/system_data_provider.hpp"

namespace ldde::system {

class SystemUIController {
public:
    using RequestRenderCallback = std::function<void()>;

    SystemUIController(
        SystemPanelStateMachine& state_machine,
        SystemUILayout& layout,
        QuickControlsManager& controls_mgr,
        SystemDataProvider& data_provider);

    [[nodiscard]] SystemPanelStateMachine& state_machine() noexcept { return state_machine_; }
    [[nodiscard]] const SystemPanelStateMachine& state_machine() const noexcept { return state_machine_; }

    [[nodiscard]] bool is_panel_open() const noexcept { return state_machine_.is_open(); }

    core::Status open_panel();
    core::Status close_panel();
    core::Status toggle_panel();

    // Status Bar Input
    bool handle_status_touch_down(int32_t x, int32_t y);
    bool handle_status_touch_up(int32_t x, int32_t y);

    // System Panel Input
    bool handle_panel_touch_down(int32_t x, int32_t y);
    bool handle_panel_touch_motion(int32_t x, int32_t y);
    bool handle_panel_touch_up(int32_t x, int32_t y);
    void handle_panel_touch_cancel();

    // Keyboard Input
    bool handle_key(uint32_t key_symbol, uint32_t state = 1, uint32_t modifiers = 0);

    void on_request_render(RequestRenderCallback callback);

private:
    void request_render();

    SystemPanelStateMachine& state_machine_;
    SystemUILayout& layout_;
    QuickControlsManager& controls_mgr_;
    SystemDataProvider& data_provider_;

    bool status_pressed_ = false;
    core::Point touch_down_pos_{0, 0};
    core::Point last_touch_pos_{0, 0};
    bool is_touch_active_ = false;

    RequestRenderCallback render_cb_;
};

} // namespace ldde::system
