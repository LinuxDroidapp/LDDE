#pragma once

#include <memory>
#include "ldde/core/error.hpp"
#include "ldde/config/config.hpp"
#include "ldde/display/display_policy.hpp"
#include "ldde/shell/shell.hpp"
#include "ldde/system/system_data_provider.hpp"
#include "ldde/system/quick_controls.hpp"
#include "ldde/system/system_ui_state.hpp"
#include "ldde/system/system_ui_layout.hpp"
#include "ldde/system/system_ui_view.hpp"
#include "ldde/system/system_ui_controller.hpp"

namespace ldde::system {

class SystemUI {
public:
    SystemUI();
    ~SystemUI();

    core::Status initialize(
        shell::Shell& shell,
        const display::DisplayPolicy& policy,
        const config::Config& config,
        std::shared_ptr<SystemDataProvider> data_provider = nullptr);

    void shutdown() noexcept;

    void update_display_policy(const display::DisplayPolicy& policy);

    void render_status_bar(
        shell::ShmBuffer& buffer,
        const shell::ShellTheme& theme,
        const shell::DesignTokens& tokens);

    void render_panel(
        shell::ShmBuffer& buffer,
        const shell::ShellTheme& theme,
        const shell::DesignTokens& tokens);

    // Panel state queries & controls
    [[nodiscard]] bool is_panel_open() const noexcept;
    core::Status open_panel();
    core::Status close_panel();
    core::Status toggle_panel();

    // Touch & Pointer input routing
    bool handle_status_touch_down(int32_t x, int32_t y);
    bool handle_status_touch_up(int32_t x, int32_t y);

    bool handle_panel_touch_down(int32_t x, int32_t y);
    bool handle_panel_touch_motion(int32_t x, int32_t y);
    bool handle_panel_touch_up(int32_t x, int32_t y);
    void handle_panel_touch_cancel();

    // Keyboard input routing
    bool handle_key(uint32_t key_symbol, uint32_t state = 1, uint32_t modifiers = 0);

    // Subcomponent accessors
    [[nodiscard]] SystemDataProvider& data_provider() noexcept { return *data_provider_; }
    [[nodiscard]] const SystemDataProvider& data_provider() const noexcept { return *data_provider_; }

    [[nodiscard]] QuickControlsManager& controls_manager() noexcept { return *controls_mgr_; }
    [[nodiscard]] const QuickControlsManager& controls_manager() const noexcept { return *controls_mgr_; }

    [[nodiscard]] SystemPanelStateMachine& state_machine() noexcept { return state_machine_; }
    [[nodiscard]] const SystemPanelStateMachine& state_machine() const noexcept { return state_machine_; }

    [[nodiscard]] const SystemUILayout& layout() const noexcept { return layout_; }

    [[nodiscard]] SystemUIController* controller() noexcept { return controller_.get(); }

private:
    void update_layout();

    shell::Shell* shell_ = nullptr;
    display::DisplayPolicy display_policy_;

    std::shared_ptr<SystemDataProvider> data_provider_;
    std::unique_ptr<QuickControlsManager> controls_mgr_;
    SystemPanelStateMachine state_machine_{SystemPanelState::Closed};
    SystemUILayout layout_;
    std::unique_ptr<SystemUIController> controller_;

    bool initialized_ = false;
};

} // namespace ldde::system

