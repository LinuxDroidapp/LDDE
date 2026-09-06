#pragma once

#include "ldde/core/error.hpp"
#include "ldde/desktop/desktop_state.hpp"
#include "ldde/desktop/desktop_background.hpp"
#include "ldde/desktop/desktop_layout.hpp"
#include "ldde/desktop/desktop_model.hpp"
#include "ldde/desktop/desktop_view.hpp"
#include "ldde/desktop/desktop_controller.hpp"
#include "ldde/shell/shell.hpp"
#include "ldde/shell/shm_buffer.hpp"
#include "ldde/window/window_registry.hpp"
#include "ldde/window/window_manager.hpp"
#include "ldde/launcher/launcher.hpp"
#include "ldde/dock/dock.hpp"
#include "ldde/switcher/switcher.hpp"
#include "ldde/display/display_policy.hpp"
#include "ldde/config/config.hpp"
#include <memory>

namespace ldde::desktop {

class Desktop {
public:
    Desktop();
    ~Desktop();

    Desktop(const Desktop&) = delete;
    Desktop& operator=(const Desktop&) = delete;

    core::Status initialize(
        shell::Shell& shell,
        window::WindowRegistry& window_registry,
        window::WindowManager& window_manager,
        launcher::Launcher& launcher,
        dock::Dock& dock,
        switcher::Switcher& switcher,
        const display::DisplayPolicy& policy,
        const config::Config& config);

    void shutdown() noexcept;

    core::Status activate();
    core::Status suspend();
    core::Status resume();

    [[nodiscard]] DesktopState state() const noexcept { return state_machine_.state(); }
    [[nodiscard]] bool is_ready() const noexcept { return state_machine_.is_ready(); }
    [[nodiscard]] bool is_active() const noexcept { return state_machine_.is_active(); }
    [[nodiscard]] bool is_suspended() const noexcept { return state_machine_.is_suspended(); }

    void update_display_policy(const display::DisplayPolicy& policy);

    void render(shell::ShmBuffer& buffer, const shell::ShellTheme& theme);

    bool handle_touch_down(int32_t x, int32_t y);
    bool handle_touch_up(int32_t x, int32_t y);
    bool handle_touch_motion(int32_t x, int32_t y);
    void handle_touch_cancel();

    bool handle_pointer_button(int32_t x, int32_t y, uint32_t button, uint32_t state);

    [[nodiscard]] const DesktopLayout& layout() const noexcept { return layout_; }
    [[nodiscard]] const DesktopBackground& background() const noexcept { return background_; }
    [[nodiscard]] DesktopBackground& background() noexcept { return background_; }
    [[nodiscard]] const DesktopModel* model() const noexcept { return model_.get(); }
    [[nodiscard]] DesktopModel* model() noexcept { return model_.get(); }
    [[nodiscard]] DesktopController* controller() noexcept { return controller_.get(); }

private:
    DesktopStateMachine state_machine_;
    DesktopBackground background_;
    DesktopLayout layout_;
    DesktopView view_;

    std::unique_ptr<DesktopModel> model_;
    std::unique_ptr<DesktopController> controller_;

    display::DisplayPolicy display_policy_;
    shell::Shell* shell_ = nullptr;

    void update_layout();
};

} // namespace ldde::desktop
