#pragma once

#include <memory>
#include <string_view>
#include "ldde/core/error.hpp"
#include "ldde/config/config.hpp"
#include "ldde/display/display_policy.hpp"
#include "ldde/shell/shm_buffer.hpp"
#include "ldde/shell/theme.hpp"
#include "ldde/shell/design_tokens.hpp"
#include "ldde/launcher/launcher_icon_resolver.hpp"
#include "ldde/switcher/switcher_state.hpp"
#include "ldde/switcher/switcher_item.hpp"
#include "ldde/switcher/switcher_mru.hpp"
#include "ldde/switcher/switcher_model.hpp"
#include "ldde/switcher/switcher_layout.hpp"
#include "ldde/switcher/switcher_view.hpp"
#include "ldde/switcher/switcher_controller.hpp"

namespace ldde::switcher {

class Switcher {
public:
    Switcher();
    ~Switcher();

    Switcher(const Switcher&) = delete;
    Switcher& operator=(const Switcher&) = delete;

    core::Status initialize(
        application::ApplicationCatalog& catalog,
        window::WindowRegistry& registry,
        window::WindowManager& window_manager,
        const display::DisplayPolicy& policy,
        const config::Config& config);

    void shutdown() noexcept;

    // Visibility & Lifecycle
    core::Status open();
    core::Status close();
    core::Status toggle();
    [[nodiscard]] bool is_open() const noexcept { return state_machine_.is_open(); }
    [[nodiscard]] SwitcherState state() const noexcept { return state_machine_.state(); }

    // Display & Layout updates
    void update_display_policy(const display::DisplayPolicy& policy);
    void render(shell::ShmBuffer& buffer,
                const shell::ShellTheme& theme,
                const shell::DesignTokens& tokens);

    // Input handlers
    bool handle_touch_down(int32_t x, int32_t y);
    bool handle_touch_motion(int32_t x, int32_t y);
    bool handle_touch_up(int32_t x, int32_t y);
    void handle_touch_cancel();

    bool handle_pointer_motion(int32_t x, int32_t y);
    bool handle_pointer_button(uint32_t button, uint32_t state, int32_t x, int32_t y);
    bool handle_pointer_axis(double delta_x, double delta_y);

    bool handle_key(uint32_t key_symbol, uint32_t state = 1, uint32_t modifiers = 0);

    // Accessors
    [[nodiscard]] SwitcherStateMachine& state_machine() noexcept { return state_machine_; }
    [[nodiscard]] const SwitcherStateMachine& state_machine() const noexcept { return state_machine_; }
    [[nodiscard]] SwitcherModel& model() noexcept { return *model_; }
    [[nodiscard]] const SwitcherModel& model() const noexcept { return *model_; }
    [[nodiscard]] const SwitcherLayout& layout() const noexcept { return layout_; }
    [[nodiscard]] SwitcherController& controller() noexcept { return *controller_; }
    [[nodiscard]] const SwitcherController& controller() const noexcept { return *controller_; }

private:
    SwitcherStateMachine state_machine_;
    std::unique_ptr<SwitcherModel> model_;
    SwitcherLayout layout_;
    SwitcherView view_;
    std::unique_ptr<SwitcherController> controller_;
    launcher::LauncherIconResolver icon_resolver_;

    display::DisplayPolicy display_policy_;
    bool enabled_ = true;

    void update_layout();
};

} // namespace ldde::switcher
