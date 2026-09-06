#pragma once

#include <memory>
#include <string>
#include <optional>
#include "ldde/core/error.hpp"
#include "ldde/core/lifecycle.hpp"
#include "ldde/core/logging.hpp"
#include "ldde/core/event_loop.hpp"
#include "ldde/core/readiness.hpp"
#include "ldde/config/config.hpp"
#include "ldde/wayland/connection.hpp"
#include "ldde/wayland/registry.hpp"
#include "ldde/display/display_manager.hpp"
#include "ldde/input/input_manager.hpp"
#include "ldde/input/touch_interaction_manager.hpp"
#include "ldde/shell/shell.hpp"
#include "ldde/window/window_registry.hpp"
#include "ldde/window/window_tracker.hpp"
#include "ldde/window/window_manager.hpp"
#include "ldde/application/application_catalog.hpp"
#include "ldde/application/application_discovery.hpp"
#include "ldde/application/application_change_monitor.hpp"
#include "ldde/launcher/launcher.hpp"
#include "ldde/dock/dock.hpp"
#include "ldde/switcher/switcher.hpp"
#include "ldde/desktop/desktop.hpp"

namespace ldde::core {

struct CommandLineOptions {
    std::optional<std::string> config_path;
    std::optional<std::string> log_level;
    std::optional<std::string> wayland_display;
    std::optional<int> ready_fd;
    bool show_help = false;
    bool show_version = false;
};

class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    Status initialize(int argc, char* argv[]);
    int run();
    void request_shutdown(int exit_code = 0);

    [[nodiscard]] LifecycleManager& lifecycle() noexcept { return lifecycle_; }
    [[nodiscard]] config::Config& config() noexcept { return config_; }
    [[nodiscard]] EventLoop& event_loop() noexcept { return event_loop_; }
    [[nodiscard]] wayland::WaylandConnection& wayland_connection() noexcept { return wayland_connection_; }
    [[nodiscard]] wayland::WaylandRegistry& wayland_registry() noexcept { return wayland_registry_; }
    [[nodiscard]] display::DisplayManager& display_manager() noexcept { return display_manager_; }
    [[nodiscard]] input::InputManager& input_manager() noexcept { return input_manager_; }
    [[nodiscard]] input::TouchInteractionManager* touch_interaction_manager() noexcept { return touch_interaction_manager_.get(); }
    [[nodiscard]] ReadinessManager& readiness_manager() noexcept { return readiness_manager_; }
    [[nodiscard]] shell::Shell& shell() noexcept { return shell_; }
    [[nodiscard]] window::WindowRegistry& window_registry() noexcept { return window_registry_; }
    [[nodiscard]] window::WindowTracker& window_tracker() noexcept { return window_tracker_; }
    [[nodiscard]] window::WindowManager& window_manager() noexcept { return window_manager_; }
    [[nodiscard]] application::ApplicationCatalog& application_catalog() noexcept { return application_catalog_; }
    [[nodiscard]] application::ApplicationDiscovery& application_discovery() noexcept { return application_discovery_; }
    [[nodiscard]] application::ApplicationChangeMonitor* application_change_monitor() noexcept { return application_change_monitor_.get(); }
    [[nodiscard]] launcher::Launcher& launcher() noexcept { return launcher_; }
    [[nodiscard]] const launcher::Launcher& launcher() const noexcept { return launcher_; }
    [[nodiscard]] dock::Dock& dock() noexcept { return dock_; }
    [[nodiscard]] const dock::Dock& dock() const noexcept { return dock_; }
    [[nodiscard]] switcher::Switcher& switcher() noexcept { return switcher_; }
    [[nodiscard]] const switcher::Switcher& switcher() const noexcept { return switcher_; }
    [[nodiscard]] desktop::Desktop& desktop() noexcept { return desktop_; }
    [[nodiscard]] const desktop::Desktop& desktop() const noexcept { return desktop_; }

    [[nodiscard]] static std::optional<CommandLineOptions> parse_args(int argc, char* argv[]);
    static void print_help(std::string_view program_name);
    static void print_version();

private:
    LifecycleManager lifecycle_;
    config::Config config_;
    EventLoop event_loop_;
    wayland::WaylandConnection wayland_connection_;
    wayland::WaylandRegistry wayland_registry_;
    display::DisplayManager display_manager_;
    input::InputManager input_manager_;
    ReadinessManager readiness_manager_;
    shell::Shell shell_;
    window::WindowRegistry window_registry_;
    window::WindowTracker window_tracker_;
    window::WindowManager window_manager_;
    std::unique_ptr<input::TouchInteractionManager> touch_interaction_manager_;
    application::ApplicationCatalog application_catalog_;
    application::ApplicationDiscovery application_discovery_;
    std::unique_ptr<application::ApplicationChangeMonitor> application_change_monitor_;
    launcher::Launcher launcher_;
    dock::Dock dock_;
    switcher::Switcher switcher_;
    desktop::Desktop desktop_;

    CommandLineOptions cli_options_;
    int exit_code_ = 0;
    bool wayland_fd_attached_ = false;
    bool server_fd_attached_ = false;

    Status setup_logging();
    Status setup_session_environment();
    Status connect_wayland();
    Status initialize_components();
    Status setup_event_loop();
    Status establish_readiness();
    void perform_shutdown();
};

} // namespace ldde::core

