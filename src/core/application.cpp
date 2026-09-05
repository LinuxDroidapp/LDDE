#include "ldde/core/application.hpp"
#include <iostream>
#include <csignal>
#include <cstdlib>
#include <unistd.h>

namespace ldde::core {

Application::Application() = default;

Application::~Application() {
    perform_shutdown();
}

std::optional<CommandLineOptions> Application::parse_args(int argc, char* argv[]) {
    CommandLineOptions options;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            options.show_help = true;
            return options;
        }
        if (arg == "-v" || arg == "--version") {
            options.show_version = true;
            return options;
        }
        if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                options.config_path = argv[++i];
            } else {
                std::cerr << "Error: --config requires a path argument\n";
                return std::nullopt;
            }
        } else if (arg == "-l" || arg == "--log-level") {
            if (i + 1 < argc) {
                options.log_level = argv[++i];
            } else {
                std::cerr << "Error: --log-level requires a level argument\n";
                return std::nullopt;
            }
        } else if (arg == "-d" || arg == "--wayland-display") {
            if (i + 1 < argc) {
                options.wayland_display = argv[++i];
            } else {
                std::cerr << "Error: --wayland-display requires a display name\n";
                return std::nullopt;
            }
        } else if (arg == "--ready-fd") {
            if (i + 1 < argc) {
                char* end = nullptr;
                long fd = std::strtol(argv[++i], &end, 10);
                if (end == argv[i] || fd < 0) {
                    std::cerr << "Error: --ready-fd requires a non-negative integer\n";
                    return std::nullopt;
                }
                options.ready_fd = static_cast<int>(fd);
            } else {
                std::cerr << "Error: --ready-fd requires a file descriptor argument\n";
                return std::nullopt;
            }
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            return std::nullopt;
        }
    }

    return options;
}

void Application::print_help(std::string_view program_name) {
    std::cout << "LinuxDroid Desktop Environment (LDDE) - Version " << kVersion << "\n\n"
              << "Usage: " << program_name << " [options]\n\n"
              << "Options:\n"
              << "  -h, --help                  Show this help text and exit\n"
              << "  -v, --version               Show version and exit\n"
              << "  -c, --config <path>         Specify path to configuration file\n"
              << "  -l, --log-level <level>     Log level (TRACE, DEBUG, INFO, WARN, ERROR, FATAL)\n"
              << "  -d, --wayland-display <name> Wayland display socket name to connect to\n"
              << "      --ready-fd <fd>         File descriptor to signal readiness to LDDM\n\n";
}

void Application::print_version() {
    std::cout << "LDDE " << kVersion << " (LinuxDroid Desktop Environment Foundation)\n";
}

Status Application::setup_logging() {
    if (cli_options_.log_level.has_value()) {
        auto lvl = parse_log_level(cli_options_.log_level.value());
        if (lvl.has_value()) {
            Logger::instance().set_level(lvl.value());
        }
    } else {
        auto cfg_lvl = config_.get_string("logging", "level");
        if (cfg_lvl.has_value()) {
            auto lvl = parse_log_level(cfg_lvl.value());
            if (lvl.has_value()) {
                Logger::instance().set_level(lvl.value());
            }
        }
    }
    return Status::ok();
}

Status Application::setup_session_environment() {
    // Standard Linux Wayland environment
    if (!std::getenv("XDG_SESSION_TYPE")) {
        setenv("XDG_SESSION_TYPE", "wayland", 0);
    }
    if (!std::getenv("XDG_CURRENT_DESKTOP")) {
        setenv("XDG_CURRENT_DESKTOP", "LDDE", 0);
    }

    const char* runtime_dir = std::getenv("XDG_RUNTIME_DIR");
    if (!runtime_dir || !*runtime_dir) {
        return LDDE_STATUS_ERROR(ErrorCategory::Session,
                                 ErrorCode::SessionEnvironmentMissing,
                                 "XDG_RUNTIME_DIR environment variable is not set");
    }

    return Status::ok();
}

Status Application::connect_wayland() {
    Status s = lifecycle_.transition_to(LifecycleState::ConnectingWayland);
    if (s.is_error()) return s;

    s = wayland_connection_.connect(cli_options_.wayland_display);
    if (s.is_error()) {
        lifecycle_.transition_to(LifecycleState::Failed);
        return s;
    }

    wayland_connection_.set_disconnect_callback([this]() {
        LDDE_LOG_WARN(Wayland, "Wayland disconnected by compositor; initiating shutdown.");
        request_shutdown(1);
    });

    s = wayland_registry_.initialize(wayland_connection_.display());
    if (s.is_error()) {
        lifecycle_.transition_to(LifecycleState::Failed);
        return s;
    }

    return Status::ok();
}

Status Application::initialize_components() {
    Status s = lifecycle_.transition_to(LifecycleState::InitializingComponents);
    if (s.is_error()) return s;

    s = display_manager_.initialize(wayland_registry_);
    if (s.is_error()) {
        lifecycle_.transition_to(LifecycleState::Failed);
        return s;
    }

    s = input_manager_.initialize(wayland_registry_);
    if (s.is_error()) {
        lifecycle_.transition_to(LifecycleState::Failed);
        return s;
    }

    // Roundtrip to process initial globals, outputs, seats
    s = wayland_connection_.roundtrip();
    if (s.is_error()) {
        lifecycle_.transition_to(LifecycleState::Failed);
        return s;
    }

    // Verify required globals (wl_compositor, wl_shm)
    s = wayland_registry_.verify_required_globals();
    if (s.is_error()) {
        lifecycle_.transition_to(LifecycleState::Failed);
        return s;
    }

    // Second roundtrip to ensure initial output/seat events are processed
    wayland_connection_.roundtrip();

    return Status::ok();
}

Status Application::setup_event_loop() {
    Status s = event_loop_.initialize();
    if (s.is_error()) return s;

    // Register POSIX shutdown signals (SIGINT, SIGTERM, SIGHUP)
    s = event_loop_.register_signals({SIGINT, SIGTERM, SIGHUP}, [this](int signum) {
        LDDE_LOG_INFO(Core, "Received shutdown signal (" << signum << ")");
        request_shutdown(0);
    });
    if (s.is_error()) return s;

    // Hook Wayland display fd into event loop
    int wayland_fd = wayland_connection_.fd();
    if (wayland_fd >= 0) {
        s = event_loop_.add_fd(wayland_fd, FdEvent::Readable | FdEvent::Error | FdEvent::Hangup,
            [this](int, FdEvent events) {
                if (events & (FdEvent::Error | FdEvent::Hangup)) {
                    LDDE_LOG_WARN(Wayland, "Wayland display socket error / hangup");
                    request_shutdown(1);
                    return;
                }

                if (events & FdEvent::Readable) {
                    Status read_status = wayland_connection_.read_events();
                    if (read_status.is_error()) {
                        LDDE_LOG_WARN(Wayland, "Wayland read_events failed: " << read_status.to_string());
                        request_shutdown(1);
                        return;
                    }
                }
            });

        if (s.is_ok()) {
            wayland_fd_attached_ = true;
        }
    }

    return s;
}

Status Application::establish_readiness() {
    Status s = lifecycle_.transition_to(LifecycleState::Ready);
    if (s.is_error()) return s;

    readiness_manager_.detect_environment();
    if (cli_options_.ready_fd.has_value()) {
        readiness_manager_.set_ready_fd(cli_options_.ready_fd.value());
    }

    s = readiness_manager_.report_ready();
    if (s.is_error()) {
        LDDE_LOG_WARN(Core, "Readiness reporting warning: " << s.to_string());
    }

    LDDE_LOG_INFO(Core, "LDDE D0 Foundation reached READY state");
    return Status::ok();
}

Status Application::initialize(int argc, char* argv[]) {
    auto parsed = parse_args(argc, argv);
    if (!parsed.has_value()) {
        return LDDE_STATUS_ERROR(ErrorCategory::Application,
                                 ErrorCode::InvalidArgument,
                                 "Failed to parse command-line options");
    }
    cli_options_ = parsed.value();

    if (cli_options_.show_help) {
        print_help(argv[0]);
        return Status::ok();
    }
    if (cli_options_.show_version) {
        print_version();
        return Status::ok();
    }

    // Step 1: Initialize logging
    setup_logging();
    LDDE_LOG_INFO(Core, "Starting LinuxDroid Desktop Environment (LDDE) v" << kVersion);

    // Step 2: Transition to INITIALIZING
    Status s = lifecycle_.transition_to(LifecycleState::Initializing);
    if (s.is_error()) return s;

    // Step 3: Validate and setup session environment
    s = setup_session_environment();
    if (s.is_error()) {
        lifecycle_.transition_to(LifecycleState::Failed);
        return s;
    }

    // Step 4: Load configuration
    s = config_.load_with_precedence(cli_options_.config_path);
    if (s.is_error()) {
        LDDE_LOG_ERROR(Config, "Configuration error: " << s.to_string());
        lifecycle_.transition_to(LifecycleState::Failed);
        return s;
    }
    setup_logging(); // Update logging level from config if needed

    // Step 5: Setup event loop
    s = setup_event_loop();
    if (s.is_error()) {
        lifecycle_.transition_to(LifecycleState::Failed);
        return s;
    }

    // Step 6: Connect Wayland and initialize registry
    s = connect_wayland();
    if (s.is_error()) {
        return s;
    }

    // Step 7: Initialize components (Display, Input)
    s = initialize_components();
    if (s.is_error()) {
        return s;
    }

    // Step 8: Establish readiness and notify LDDM
    s = establish_readiness();
    if (s.is_error()) {
        return s;
    }

    return Status::ok();
}

int Application::run() {
    if (cli_options_.show_help || cli_options_.show_version) {
        return 0;
    }

    if (lifecycle_.state() != LifecycleState::Ready) {
        LDDE_LOG_FATAL(Core, "Cannot run LDDE: application not in READY state (current: "
                             << lifecycle_state_name(lifecycle_.state()) << ")");
        return 1;
    }

    Status s = lifecycle_.transition_to(LifecycleState::Running);
    if (s.is_error()) {
        LDDE_LOG_FATAL(Core, "Transition to RUNNING failed: " << s.to_string());
        return 1;
    }

    LDDE_LOG_INFO(Core, "Entering LDDE main event loop");

    while (lifecycle_.state() == LifecycleState::Running) {
        // Prepare Wayland read before polling
        if (wayland_connection_.is_connected()) {
            if (!wayland_connection_.prepare_read()) {
                wayland_connection_.dispatch_pending();
            }
            wayland_connection_.flush();
        }

        Status ds = event_loop_.dispatch(100);
        if (ds.is_error()) {
            LDDE_LOG_ERROR(Core, "Event dispatch error: " << ds.to_string());
            break;
        }

        if (wayland_connection_.is_connected()) {
            wayland_connection_.dispatch_pending();
        }
    }

    perform_shutdown();
    return exit_code_;
}

void Application::request_shutdown(int exit_code) {
    exit_code_ = exit_code;
    auto cur = lifecycle_.state();
    if (cur != LifecycleState::Stopping && cur != LifecycleState::Stopped) {
        LDDE_LOG_INFO(Core, "Shutdown requested with exit code " << exit_code);
        lifecycle_.transition_to(LifecycleState::Stopping);
        event_loop_.stop();
    }
}

void Application::perform_shutdown() {
    auto cur = lifecycle_.state();
    if (cur == LifecycleState::Stopped) {
        return;
    }

    LDDE_LOG_INFO(Core, "Performing deterministic LDDE shutdown");
    lifecycle_.transition_to(LifecycleState::Stopping);

    if (wayland_fd_attached_) {
        event_loop_.remove_fd(wayland_connection_.fd());
        wayland_fd_attached_ = false;
    }

    // Release components in reverse initialization order
    input_manager_.reset();
    display_manager_.reset();
    wayland_registry_.reset();
    wayland_connection_.disconnect();

    Logger::instance().flush();
    lifecycle_.transition_to(LifecycleState::Stopped);
    LDDE_LOG_INFO(Core, "LDDE stopped successfully");
}

} // namespace ldde::core

