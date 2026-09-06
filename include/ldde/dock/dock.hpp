#pragma once

#include <memory>
#include <string>
#include <string_view>
#include "ldde/core/error.hpp"
#include "ldde/config/config.hpp"
#include "ldde/display/display_policy.hpp"
#include "ldde/shell/shm_buffer.hpp"
#include "ldde/shell/theme.hpp"
#include "ldde/shell/design_tokens.hpp"
#include "ldde/dock/dock_state.hpp"
#include "ldde/dock/dock_item.hpp"
#include "ldde/dock/dock_layout.hpp"
#include "ldde/dock/dock_model.hpp"
#include "ldde/dock/dock_view.hpp"
#include "ldde/dock/dock_controller.hpp"
#include "ldde/launcher/launcher_icon_resolver.hpp"

namespace ldde::dock {

class Dock {
public:
    Dock();
    ~Dock();

    Dock(const Dock&) = delete;
    Dock& operator=(const Dock&) = delete;

    core::Status initialize(
        application::ApplicationCatalog& catalog,
        window::WindowRegistry& registry,
        window::WindowManager& window_manager,
        launcher::Launcher& launcher,
        const display::DisplayPolicy& policy,
        const config::Config& config,
        std::shared_ptr<launcher::ApplicationLauncher> app_launcher = nullptr);

    void shutdown() noexcept;

    // Visibility
    core::Status show();
    core::Status hide();
    core::Status toggle();
    [[nodiscard]] bool is_visible() const noexcept { return state_machine_.is_visible(); }
    [[nodiscard]] DockState state() const noexcept { return state_machine_.state(); }

    // Display & Layout updates
    void update_display_policy(const display::DisplayPolicy& policy);
    void update_geometry(const core::Rect& dock_geom);
    void render(shell::ShmBuffer& buffer,
                const shell::ShellTheme& theme,
                const shell::DesignTokens& tokens);

    // Input handlers (accept coordinates local to dock)
    bool handle_touch_down(int32_t local_x, int32_t local_y);
    bool handle_touch_motion(int32_t local_x, int32_t local_y);
    bool handle_touch_up(int32_t local_x, int32_t local_y);
    void handle_touch_cancel();

    bool handle_pointer_motion(int32_t local_x, int32_t local_y);
    bool handle_pointer_button(uint32_t button, uint32_t state, int32_t local_x, int32_t local_y);
    bool handle_pointer_axis(double delta_x, double delta_y);

    bool handle_key(uint32_t key_symbol);

    // Pinning
    bool pin(const application::ApplicationId& id);
    bool unpin(const application::ApplicationId& id);
    [[nodiscard]] bool is_pinned(const application::ApplicationId& id) const noexcept;

    // Accessors
    [[nodiscard]] DockStateMachine& state_machine() noexcept { return state_machine_; }
    [[nodiscard]] const DockStateMachine& state_machine() const noexcept { return state_machine_; }
    [[nodiscard]] DockModel& model() noexcept { return *model_; }
    [[nodiscard]] const DockModel& model() const noexcept { return *model_; }
    [[nodiscard]] const DockLayout& layout() const noexcept { return layout_; }
    [[nodiscard]] DockController& controller() noexcept { return *controller_; }
    [[nodiscard]] const DockController& controller() const noexcept { return *controller_; }

private:
    DockStateMachine state_machine_;
    std::unique_ptr<DockModel> model_;
    DockLayout layout_;
    DockView view_;
    std::unique_ptr<DockController> controller_;
    launcher::LauncherIconResolver icon_resolver_;

    display::DisplayPolicy display_policy_;
    core::Rect dock_geometry_{0, 0, 0, 0};
    int32_t custom_item_size_ = 0;
    int32_t custom_spacing_ = 0;
    bool enabled_ = true;

    void update_layout();
};

} // namespace ldde::dock
