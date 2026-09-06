#include "ldde/core/application.hpp"
#include <iostream>
#include <csignal>
#include <cstdlib>
#include <unistd.h>

namespace ldde::core {

Application::Application()
    : window_manager_(window_registry_, window_tracker_, display_manager_),
      application_discovery_(application_catalog_, application::ApplicationDiscoveryPolicy::from_config_and_env(config_)) {}

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

    s = display_manager_.initialize(wayland_registry_, &config_);
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

    // Initialize shell subsystem
    s = shell_.initialize(wayland_connection_, wayland_registry_, display_manager_, config_);
    if (s.is_error()) {
        LDDE_LOG_ERROR(Core, "Failed to initialize shell: " << s.to_string());
        lifecycle_.transition_to(LifecycleState::Failed);
        return s;
    }

    // Initialize window tracking subsystem
    s = window_tracker_.initialize(wayland_connection_, wayland_registry_, window_registry_, config_);
    if (s.is_error()) {
        LDDE_LOG_ERROR(Core, "Failed to initialize window tracker: " << s.to_string());
        lifecycle_.transition_to(LifecycleState::Failed);
        return s;
    }

    // Initialize window manager subsystem
    s = window_manager_.initialize(config_);
    if (s.is_error()) {
        LDDE_LOG_ERROR(Core, "Failed to initialize window manager: " << s.to_string());
        lifecycle_.transition_to(LifecycleState::Failed);
        return s;
    }

    // Initialize D5 Touch Interaction Manager
    touch_interaction_manager_ = std::make_unique<input::TouchInteractionManager>(
        window_manager_, window_registry_, display_manager_, config_, &shell_);

    // Helper to connect a seat's touch device to TouchInteractionManager
    auto connect_touch_device = [this](input::Seat* seat) {
        if (!seat || !seat->touch() || !touch_interaction_manager_) return;
        auto* tch = seat->touch();
        tch->on_down([this](const input::TouchDownEvent& ev) {
            if (launcher_.is_open()) {
                launcher_.handle_touch_down(static_cast<int32_t>(ev.x), static_cast<int32_t>(ev.y));
                return;
            }
            if (touch_interaction_manager_) {
                touch_interaction_manager_->handle_touch_down(
                    ev.id,
                    core::Point{static_cast<int32_t>(ev.x), static_cast<int32_t>(ev.y)},
                    ev.time_ms);
            }
        });
        tch->on_motion([this](const input::TouchMotionEvent& ev) {
            if (launcher_.is_open()) {
                launcher_.handle_touch_motion(static_cast<int32_t>(ev.x), static_cast<int32_t>(ev.y));
                return;
            }
            if (touch_interaction_manager_) {
                touch_interaction_manager_->handle_touch_motion(
                    ev.id,
                    core::Point{static_cast<int32_t>(ev.x), static_cast<int32_t>(ev.y)},
                    ev.time_ms);
            }
        });
        tch->on_up([this](const input::TouchUpEvent& ev) {
            if (launcher_.is_open()) {
                launcher_.handle_touch_up(0, 0);
                return;
            }
            if (touch_interaction_manager_) {
                touch_interaction_manager_->handle_touch_up(ev.id, ev.time_ms);
            }
        });
        tch->on_cancel([this]() {
            if (launcher_.is_open()) {
                launcher_.handle_touch_cancel();
                return;
            }
            if (touch_interaction_manager_) {
                touch_interaction_manager_->cancel_active_interaction();
            }
        });
        tch->on_frame([this]() {
            if (touch_interaction_manager_) {
                touch_interaction_manager_->handle_touch_frame();
            }
        });
    };

    if (input_manager_.primary_seat()) {
        connect_touch_device(input_manager_.primary_seat());
    }
    input_manager_.on_seat_added([connect_touch_device](input::Seat* seat) {
        connect_touch_device(seat);
    });

    // Window destruction observer for TouchInteractionManager
    window_registry_.add_listener([this](const window::WindowEvent& ev) {
        if (ev.type == window::WindowEventType::Destroyed && touch_interaction_manager_) {
            touch_interaction_manager_->handle_window_destroyed(ev.window_id);
        }
    });

    // Attach display change observer to adapt shell, window manager, and touch interaction
    display_manager_.on_display_changed([this](const display::DisplayInfo& disp) {
        shell_.update_display(disp);
        auto* policy = display_manager_.find_policy_by_id(disp.id);
        if (policy) {
            window_manager_.handle_display_change(*policy);
            launcher_.update_display_policy(*policy);
            if (touch_interaction_manager_) {
                touch_interaction_manager_->handle_display_change(*policy);
            }
        } else {
            window_manager_.handle_display_change(disp);
        }
    });

    display_manager_.on_display_removed([this](const display::DisplayInfo& disp) {
        window_manager_.handle_display_removed(disp.id);
        auto primary = display_manager_.primary_display();
        if (primary) {
            shell_.update_display(*primary);
            auto* policy = display_manager_.find_policy_by_id(primary->id);
            if (policy) {
                launcher_.update_display_policy(*policy);
                if (touch_interaction_manager_) {
                    touch_interaction_manager_->handle_display_change(*policy);
                }
            }
        }
    });

    // Initialize D6 Application Discovery
    application_discovery_.policy() = application::ApplicationDiscoveryPolicy::from_config_and_env(config_);
    s = application_discovery_.scan_and_refresh();
    if (s.is_error()) {
        LDDE_LOG_WARN(Application, "Initial application discovery failed: " << s.to_string());
    }

    if (config_.get_bool_or("application", "watch_filesystem", true)) {
        application_change_monitor_ = std::make_unique<application::ApplicationChangeMonitor>(
            application_discovery_, &event_loop_);
        application_change_monitor_->start();
    }

    // Initialize D7 Application Launcher
    display::DisplayPolicy default_policy;
    auto primary = display_manager_.primary_display();
    if (primary) {
        auto* pol = display_manager_.find_policy_by_id(primary->id);
        if (pol) {
            default_policy = *pol;
        } else {
            default_policy = display::DisplayPolicy(*primary);
        }
    }
    s = launcher_.initialize(application_catalog_, default_policy, config_);
    if (s.is_error()) {
        LDDE_LOG_WARN(Launcher, "Failed to initialize launcher: " << s.to_string());
    }

    // Connect launcher with shell overlay rendering
    shell_.overlay().set_render_callback([this](shell::ShmBuffer& buf, const shell::ShellTheme& theme) {
        launcher_.render(buf, theme, shell_.tokens());
    });

    launcher_.controller().state_machine().on_state_changed([this](launcher::LauncherState /*old_state*/, launcher::LauncherState new_state) {
        bool active = (new_state != launcher::LauncherState::Closed);
        shell_.overlay().set_active(active);
        shell_.render_all();
    });

    launcher_.controller().on_request_render([this]() {
        if (launcher_.is_open()) {
            shell_.render_all();
        }
    });

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

    // Hook application tracking server fd into event loop
    int server_fd = window_tracker_.server_fd();
    if (server_fd >= 0) {
        s = event_loop_.add_fd(server_fd, FdEvent::Readable, [this](int, FdEvent) {
            window_tracker_.dispatch_server();
        });
        if (s.is_ok()) {
            server_fd_attached_ = true;
        } else {
            LDDE_LOG_WARN(Core, "Failed to add window tracking server fd to event loop: " << s.to_string());
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

        if (server_fd_attached_) {
            window_tracker_.dispatch_server();
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

    if (server_fd_attached_) {
        event_loop_.remove_fd(window_tracker_.server_fd());
        server_fd_attached_ = false;
    }

    if (wayland_fd_attached_) {
        event_loop_.remove_fd(wayland_connection_.fd());
        wayland_fd_attached_ = false;
    }

    // Release components in reverse initialization order
    launcher_.shutdown();

    if (application_change_monitor_) {
        application_change_monitor_->stop();
        application_change_monitor_.reset();
    }
    application_catalog_.clear();

    if (touch_interaction_manager_) {
        touch_interaction_manager_->reset();
        touch_interaction_manager_.reset();
    }
    window_manager_.shutdown();
    window_tracker_.shutdown();
    window_registry_.clear();
    shell_.shutdown();
    input_manager_.reset();
    display_manager_.reset();
    wayland_registry_.reset();
    wayland_connection_.disconnect();

    Logger::instance().flush();
    lifecycle_.transition_to(LifecycleState::Stopped);
    LDDE_LOG_INFO(Core, "LDDE stopped successfully");
}

} // namespace ldde::core

