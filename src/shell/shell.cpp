#include "ldde/shell/shell.hpp"
#include "ldde/core/logging.hpp"
#include <algorithm>

namespace ldde::shell {

namespace {

bool is_valid_transition(ShellLifecycleState from, ShellLifecycleState to) {
    if (to == ShellLifecycleState::Failed) return true;
    switch (from) {
        case ShellLifecycleState::Created:
            return to == ShellLifecycleState::Initializing || to == ShellLifecycleState::Stopping || to == ShellLifecycleState::Stopped;
        case ShellLifecycleState::Initializing:
            return to == ShellLifecycleState::CreatingSurfaces || to == ShellLifecycleState::Ready;
        case ShellLifecycleState::CreatingSurfaces:
            return to == ShellLifecycleState::LayoutReady || to == ShellLifecycleState::Ready;
        case ShellLifecycleState::LayoutReady:
            return to == ShellLifecycleState::Ready;
        case ShellLifecycleState::Ready:
            return to == ShellLifecycleState::Running || to == ShellLifecycleState::Stopping;
        case ShellLifecycleState::Running:
            return to == ShellLifecycleState::Stopping;
        case ShellLifecycleState::Stopping:
            return to == ShellLifecycleState::Stopped;
        case ShellLifecycleState::Stopped:
            return to == ShellLifecycleState::Initializing;
        case ShellLifecycleState::Failed:
            return to == ShellLifecycleState::Stopping || to == ShellLifecycleState::Stopped;
    }
    return false;
}

} // namespace

Shell::Shell() = default;

Shell::~Shell() {
    shutdown();
}

Status Shell::transition_to(ShellLifecycleState next_state) {
    if (state_ == next_state) {
        return Status::ok();
    }

    if (!is_valid_transition(state_, next_state)) {
        LDDE_LOG_ERROR(Shell, "Invalid shell state transition from "
                              << shell_state_name(state_) << " to "
                              << shell_state_name(next_state));
        return LDDE_STATUS_ERROR(core::ErrorCategory::Application,
                                 core::ErrorCode::InvalidLifecycleTransition,
                                 "Invalid shell state transition");
    }

    LDDE_LOG_INFO(Shell, "Shell lifecycle transition: "
                         << shell_state_name(state_) << " -> "
                         << shell_state_name(next_state));
    state_ = next_state;
    return Status::ok();
}

Status Shell::initialize(wayland::WaylandConnection& /*connection*/,
                         wayland::WaylandRegistry& registry,
                         display::DisplayManager& display_manager,
                         const config::Config& config) {
    if (state_ != ShellLifecycleState::Created && state_ != ShellLifecycleState::Stopped) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Application,
                                 core::ErrorCode::InvalidLifecycleTransition,
                                 "Shell must be in Created or Stopped state to initialize");
    }

    Status s = transition_to(ShellLifecycleState::Initializing);
    if (s.is_error()) return s;

    // 1. Read configuration
    status_bar_enabled_ = config.get_bool("shell", "status_bar_enabled").value_or(true);
    dock_enabled_ = config.get_bool("shell", "dock_enabled").value_or(true);
    std::string dock_pos = config.get_string("shell", "dock_position").value_or("bottom");
    if (dock_pos == "left") {
        dock_position_ = DockPosition::Left;
    } else if (dock_pos == "right") {
        dock_position_ = DockPosition::Right;
    } else if (dock_pos == "top") {
        dock_position_ = DockPosition::Top;
    } else {
        dock_position_ = DockPosition::Bottom;
    }

    // Configurable colors
    auto cfg_bg_top = config.get_string("shell", "desktop_bg_top");
    if (cfg_bg_top) {
        auto c = Color::from_hex(*cfg_bg_top);
        if (c) theme_.desktop_bg_top = *c;
    }
    auto cfg_bg_bot = config.get_string("shell", "desktop_bg_bottom");
    if (cfg_bg_bot) {
        auto c = Color::from_hex(*cfg_bg_bot);
        if (c) theme_.desktop_bg_bottom = *c;
    }
    auto cfg_dock_bg = config.get_string("shell", "dock_bg");
    if (cfg_dock_bg) {
        auto c = Color::from_hex(*cfg_dock_bg);
        if (c) theme_.dock_bg = *c;
    }
    auto cfg_status_bg = config.get_string("shell", "status_bg");
    if (cfg_status_bg) {
        auto c = Color::from_hex(*cfg_status_bg);
        if (c) theme_.status_bg = *c;
    }

    desktop_.set_theme(theme_);
    status_region_.set_theme(theme_);
    dock_region_.set_theme(theme_);
    overlay_.set_theme(theme_);

    // 2. Bind Wayland globals
    auto comp_info = registry.get_global("wl_compositor");
    if (!comp_info) {
        transition_to(ShellLifecycleState::Failed);
        return LDDE_STATUS_ERROR(core::ErrorCategory::Wayland,
                                 core::ErrorCode::WaylandGlobalMissing,
                                 "wl_compositor global missing");
    }
    compositor_ = registry.bind<wl_compositor>(comp_info->name, &wl_compositor_interface, std::min(comp_info->version, 4u));
    if (!compositor_) {
        transition_to(ShellLifecycleState::Failed);
        return LDDE_STATUS_ERROR(core::ErrorCategory::Wayland,
                                 core::ErrorCode::WaylandProtocolError,
                                 "Failed to bind wl_compositor");
    }

    auto shm_info = registry.get_global("wl_shm");
    if (!shm_info) {
        transition_to(ShellLifecycleState::Failed);
        return LDDE_STATUS_ERROR(core::ErrorCategory::Wayland,
                                 core::ErrorCode::WaylandGlobalMissing,
                                 "wl_shm global missing");
    }
    shm_ = registry.bind<wl_shm>(shm_info->name, &wl_shm_interface, 1);
    if (!shm_) {
        transition_to(ShellLifecycleState::Failed);
        return LDDE_STATUS_ERROR(core::ErrorCategory::Wayland,
                                 core::ErrorCode::WaylandProtocolError,
                                 "Failed to bind wl_shm");
    }

    auto subcomp_info = registry.get_global("wl_subcompositor");
    if (subcomp_info) {
        subcompositor_ = registry.bind<wl_subcompositor>(subcomp_info->name, &wl_subcompositor_interface, 1);
    } else {
        LDDE_LOG_WARN(Shell, "wl_subcompositor not available; child surfaces will run standalone");
    }

    buffer_pool_ = std::make_unique<ShmBufferPool>(shm_);

    // 3. Initialize display and layout
    auto primary = display_manager.primary_display();
    if (primary) {
        update_display(*primary);
    }

    display_manager.on_display_changed([this](const display::DisplayInfo& info) {
        update_display(info);
    });

    display_manager.on_display_added([this](const display::DisplayInfo& info) {
        if (layout_.screen_bounds().width <= 0) {
            update_display(info);
        }
    });

    if (state_ == ShellLifecycleState::Initializing) {
        s = transition_to(ShellLifecycleState::Ready);
        if (s.is_error()) return s;
    }

    LDDE_LOG_INFO(Shell, "Shell initialized successfully (status="
                         << (status_bar_enabled_ ? "enabled" : "disabled")
                         << ", dock=" << (dock_enabled_ ? "enabled" : "disabled") << ")");
    return Status::ok();
}

void Shell::shutdown() noexcept {
    if (state_ == ShellLifecycleState::Stopped) {
        return;
    }

    transition_to(ShellLifecycleState::Stopping);

    overlay_.destroy();
    dock_region_.destroy();
    status_region_.destroy();
    desktop_.destroy();

    if (buffer_pool_) {
        buffer_pool_->release_all();
        buffer_pool_.reset();
    }

    if (subcompositor_) {
        wl_subcompositor_destroy(subcompositor_);
        subcompositor_ = nullptr;
    }
    if (shm_) {
        wl_shm_destroy(shm_);
        shm_ = nullptr;
    }
    if (compositor_) {
        wl_compositor_destroy(compositor_);
        compositor_ = nullptr;
    }

    transition_to(ShellLifecycleState::Stopped);
    LDDE_LOG_INFO(Shell, "Shell shut down successfully");
}

void Shell::update_display(const display::DisplayInfo& info) {
    if (info.width <= 0 || info.height <= 0) {
        return;
    }

    double scale = info.scale > 0 ? static_cast<double>(info.scale) : 1.0;
    tokens_ = DesignTokens::create_scaled(scale);
    status_region_.set_tokens(tokens_);
    dock_region_.set_tokens(tokens_);

    layout_.update(info, tokens_, dock_position_);

    if (compositor_ && shm_) {
        if (!desktop_.is_created()) {
            transition_to(ShellLifecycleState::CreatingSurfaces);
            create_surfaces();
            transition_to(ShellLifecycleState::LayoutReady);
            transition_to(ShellLifecycleState::Ready);
        } else {
            desktop_.update_geometry(layout_.desktop_geometry());
            if (status_bar_enabled_) {
                status_region_.update_geometry(layout_.status_geometry());
            }
            if (dock_enabled_) {
                dock_region_.update_geometry(layout_.dock_geometry());
            }
            if (overlay_.is_active()) {
                overlay_.update_geometry(layout_.overlay_geometry());
            }
        }
        render_all();
    }
}

Status Shell::create_surfaces() {
    if (!compositor_) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Wayland,
                                 core::ErrorCode::WaylandProtocolError,
                                 "Cannot create surfaces without wl_compositor");
    }

    // 1. Root desktop surface
    Status s = desktop_.create(compositor_, nullptr, nullptr, layout_.desktop_geometry(), ShellLayer::Background);
    if (s.is_error()) {
        LDDE_LOG_ERROR(Shell, "Failed to create DesktopSurface: " << s.to_string());
        return s;
    }

    // 2. Status bar subsurface
    if (status_bar_enabled_) {
        s = status_region_.create(compositor_, subcompositor_, desktop_.surface(),
                                  layout_.status_geometry(), ShellLayer::Top);
        if (s.is_error()) {
            LDDE_LOG_WARN(Shell, "Failed to create StatusRegion: " << s.to_string());
        }
    }

    // 3. Dock subsurface
    if (dock_enabled_) {
        s = dock_region_.create(compositor_, subcompositor_, desktop_.surface(),
                                layout_.dock_geometry(), ShellLayer::Bottom);
        if (s.is_error()) {
            LDDE_LOG_WARN(Shell, "Failed to create DockRegion: " << s.to_string());
        }
    }

    // 4. Overlay subsurface
    s = overlay_.create(compositor_, subcompositor_, desktop_.surface(),
                        layout_.overlay_geometry(), ShellLayer::Overlay);
    if (s.is_error()) {
        LDDE_LOG_WARN(Shell, "Failed to create ShellOverlay: " << s.to_string());
    }

    LDDE_LOG_INFO(Shell, "Shell surfaces created (desktop: "
                         << layout_.desktop_geometry().width << "x" << layout_.desktop_geometry().height
                         << ", status: " << layout_.status_geometry().width << "x" << layout_.status_geometry().height
                         << ", dock: " << layout_.dock_geometry().width << "x" << layout_.dock_geometry().height << ")");

    return Status::ok();
}

void Shell::render_all() {
    if (!buffer_pool_) return;

    if (desktop_.is_created()) {
        desktop_.render(*buffer_pool_);
    }
    if (status_bar_enabled_ && status_region_.is_created()) {
        status_region_.render(*buffer_pool_);
    }
    if (dock_enabled_ && dock_region_.is_created()) {
        dock_region_.render(*buffer_pool_);
    }
    if (overlay_.is_active() && overlay_.is_created()) {
        overlay_.render(*buffer_pool_);
    }
}

ShellRegionType Shell::handle_pointer_motion(int32_t x, int32_t y) {
    auto target = layout_.hit_test(core::Point{x, y}, overlay_.is_active());
    focused_region_ = target;
    return target;
}

ShellRegionType Shell::handle_touch_down(int32_t x, int32_t y) {
    auto target = layout_.hit_test(core::Point{x, y}, overlay_.is_active());
    focused_region_ = target;
    return target;
}

void Shell::handle_key(uint32_t /*key*/, uint32_t /*state*/) {
    // Key event dispatch for shell shortcuts will be handled in later phases.
}

} // namespace ldde::shell
