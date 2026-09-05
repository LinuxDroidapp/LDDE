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
    [[nodiscard]] ReadinessManager& readiness_manager() noexcept { return readiness_manager_; }

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

    CommandLineOptions cli_options_;
    int exit_code_ = 0;
    bool wayland_fd_attached_ = false;

    Status setup_logging();
    Status setup_session_environment();
    Status connect_wayland();
    Status initialize_components();
    Status setup_event_loop();
    Status establish_readiness();
    void perform_shutdown();
};

} // namespace ldde::core

