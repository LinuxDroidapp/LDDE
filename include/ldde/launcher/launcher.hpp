#pragma once

#include <memory>
#include <string_view>
#include "ldde/core/error.hpp"
#include "ldde/config/config.hpp"
#include "ldde/display/display_policy.hpp"
#include "ldde/application/application_catalog.hpp"
#include "ldde/launcher/launcher_controller.hpp"
#include "ldde/launcher/launcher_view.hpp"
#include "ldde/shell/shm_buffer.hpp"
#include "ldde/shell/theme.hpp"
#include "ldde/shell/design_tokens.hpp"

namespace ldde::launcher {

class Launcher {
public:
    Launcher();
    explicit Launcher(std::shared_ptr<ApplicationLauncher> backend);
    ~Launcher();

    Launcher(const Launcher&) = delete;
    Launcher& operator=(const Launcher&) = delete;

    core::Status initialize(
        application::ApplicationCatalog& catalog,
        const display::DisplayPolicy& policy,
        const config::Config& config,
        std::shared_ptr<ApplicationLauncher> backend = nullptr);

    void shutdown() noexcept;

    // Visibility API
    core::Status open();
    core::Status close();
    core::Status toggle();
    [[nodiscard]] bool is_open() const noexcept { return controller_.is_open(); }
    [[nodiscard]] LauncherState state() const noexcept { return controller_.state(); }

    // Display & Shell updates
    void update_display_policy(const display::DisplayPolicy& policy);
    void render(shell::ShmBuffer& buffer,
                const shell::ShellTheme& theme,
                const shell::DesignTokens& tokens);

    // Input routing
    bool handle_touch_down(int32_t x, int32_t y);
    bool handle_touch_motion(int32_t x, int32_t y);
    bool handle_touch_up(int32_t x, int32_t y);
    void handle_touch_cancel();
    bool handle_key(uint32_t key_symbol, uint32_t unicode_codepoint = 0);

    // Catalog update
    void refresh_catalog();

    // Accessors
    [[nodiscard]] LauncherController& controller() noexcept { return controller_; }
    [[nodiscard]] const LauncherController& controller() const noexcept { return controller_; }
    [[nodiscard]] LauncherModel& model() noexcept { return controller_.model(); }
    [[nodiscard]] const LauncherModel& model() const noexcept { return controller_.model(); }
    [[nodiscard]] const LauncherLayout& layout() const noexcept { return controller_.layout(); }

private:
    LauncherController controller_;
    application::ApplicationCatalog* catalog_ = nullptr;
    display::DisplayPolicy display_policy_;
    bool initialized_ = false;
    std::string desktop_identity_ = "LinuxDroid";
};

} // namespace ldde::launcher

