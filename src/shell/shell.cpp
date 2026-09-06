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

    display_manager_ = &display_manager;

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

    if (display_manager_) {
        auto* policy = display_manager_->find_policy_by_id(info.id);
        if (policy) {
            update_display_policy(*policy);
            return;
        }
    }

    display::DisplayPolicy policy(info);
    update_display_policy(policy);
}

void Shell::update_display_policy(const display::DisplayPolicy& policy) {
    if (policy.display_info().width <= 0 || policy.display_info().height <= 0) {
        return;
    }

    double scale = policy.scale_policy().effective_scale();
    tokens_ = DesignTokens::create_scaled(scale);
    status_region_.set_tokens(tokens_);
    dock_region_.set_tokens(tokens_);

    layout_.update(policy, tokens_, dock_position_);

    // Register shell layout reservations into DisplayPolicy
    if (display_manager_) {
        auto* live_policy = display_manager_->find_policy_by_id(policy.display_info().id);
        if (live_policy) {
            live_policy->set_shell_reservations(layout_.shell_reservations());
        }
    }

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
            if (buffer_pool_) {
                std::vector<std::pair<int32_t, int32_t>> active_dims;
                active_dims.emplace_back(layout_.desktop_geometry().width, layout_.desktop_geometry().height);
                active_dims.emplace_back(layout_.status_geometry().width, layout_.status_geometry().height);
                active_dims.emplace_back(layout_.dock_geometry().width, layout_.dock_geometry().height);
                active_dims.emplace_back(layout_.overlay_geometry().width, layout_.overlay_geometry().height);
                buffer_pool_->prune_stale(active_dims);
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

void Shell::mark_dirty(ShellDirtyFlag flags) noexcept {
    dirty_flags_ |= flags;
}

void Shell::render_desktop() {
    if (!buffer_pool_) return;
    if (desktop_.is_created()) {
        desktop_.render(*buffer_pool_);
    }
}

void Shell::render_status_bar() {
    if (!buffer_pool_) return;
    if (status_bar_enabled_ && status_region_.is_created()) {
        status_region_.render(*buffer_pool_);
    }
}

void Shell::render_dock() {
    if (!buffer_pool_) return;
    if (dock_enabled_ && dock_region_.is_created()) {
        dock_region_.render(*buffer_pool_);
    }
}

void Shell::render_overlay() {
    if (!buffer_pool_) return;
    if (overlay_.is_created()) {
        overlay_.render(*buffer_pool_);
    }
}

void Shell::render_dirty() {
    if (!buffer_pool_ || dirty_flags_ == ShellDirtyFlag::None) return;

    if ((dirty_flags_ & ShellDirtyFlag::Desktop) != ShellDirtyFlag::None) {
        render_desktop();
    }
    if ((dirty_flags_ & ShellDirtyFlag::StatusBar) != ShellDirtyFlag::None) {
        render_status_bar();
    }
    if ((dirty_flags_ & ShellDirtyFlag::Dock) != ShellDirtyFlag::None) {
        render_dock();
    }
    if ((dirty_flags_ & ShellDirtyFlag::Overlay) != ShellDirtyFlag::None) {
        render_overlay();
    }

    dirty_flags_ = ShellDirtyFlag::None;
}

void Shell::render_all() {
    mark_dirty(ShellDirtyFlag::All);
    render_dirty();
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
