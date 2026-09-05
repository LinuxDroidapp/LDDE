#include "ldde/window/window_manager.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::window {

namespace {
display::DisplayInfo get_active_display(display::DisplayManager& mgr) {
    auto disp = mgr.primary_display();
    if (disp.has_value()) return *disp;
    display::DisplayInfo fallback;
    fallback.width = 1080;
    fallback.height = 2400;
    return fallback;
}
} // namespace

WindowManager::WindowManager(
    WindowRegistry& registry,
    WindowTracker& tracker,
    display::DisplayManager& display_mgr,
    std::unique_ptr<WindowManagementBackend> backend)
    : registry_(registry),
      tracker_(tracker),
      display_mgr_(display_mgr),
      backend_(backend ? std::move(backend) : std::make_unique<DefaultWindowManagementBackend>(tracker_, registry_)),
      placement_(),
      state_ctrl_(*backend_, placement_),
      focus_(registry_, stacking_, *backend_),
      stacking_(),
      interaction_(),
      controls_() {}

WindowManager::~WindowManager() {
    shutdown();
}

Status WindowManager::initialize(const config::Config& config) {
    if (initialized_) {
        return Status::ok();
    }

    PlacementConstraints pc;
    pc.status_bar_height = static_cast<int32_t>(config.get_int_or("shell", "status_bar_height", 40));
    pc.dock_height = static_cast<int32_t>(config.get_int_or("shell", "dock_height", 68));
    pc.margin_top = static_cast<int32_t>(config.get_int_or("window", "margin_top", 8));
    pc.margin_bottom = static_cast<int32_t>(config.get_int_or("window", "margin_bottom", 8));
    pc.margin_horizontal = static_cast<int32_t>(config.get_int_or("window", "margin_horizontal", 8));
    pc.cascade_step = static_cast<int32_t>(config.get_int_or("window", "cascade_step", 32));
    placement_.set_constraints(pc);

    registry_listener_id_ = registry_.add_listener([this](const WindowEvent& event) {
        on_window_event(event);
    });

    initialized_ = true;
    LDDE_LOG_INFO(Window, "WindowManager initialized successfully");
    return Status::ok();
}

void WindowManager::shutdown() noexcept {
    if (!initialized_) return;

    if (registry_listener_id_ != 0) {
        registry_.remove_listener(registry_listener_id_);
        registry_listener_id_ = 0;
    }

    interaction_.reset();
    stacking_.clear();
    initialized_ = false;
    LDDE_LOG_INFO(Window, "WindowManager shut down successfully");
}

void WindowManager::setup_initial_window_placement(const std::shared_ptr<Window>& window) {
    if (!window) return;

    display::DisplayInfo disp = get_active_display(display_mgr_);

    size_t count = stacking_.size();
    core::Rect initial_geom = placement_.calculate_initial_geometry(
        disp,
        count,
        window->surface_size(),
        window->min_size(),
        window->max_size()
    );

    window->set_geometry(initial_geom);
    window->set_surface_size(core::Size{initial_geom.width, initial_geom.height});
    static_cast<void>(backend_->set_geometry(window->id(), initial_geom));
}

void WindowManager::on_window_event(const WindowEvent& event) {
    switch (event.type) {
        case WindowEventType::Created: {
            if (event.window) {
                stacking_.add(event.window_id, event.window->parent_id());
                setup_initial_window_placement(event.window);
                static_cast<void>(focus_.activate(event.window_id));
            }
            break;
        }
        case WindowEventType::ParentChanged: {
            if (event.window) {
                stacking_.add(event.window_id, event.window->parent_id());
            }
            break;
        }
        case WindowEventType::Closed:
        case WindowEventType::Destroyed: {
            if (interaction_.active_window_id() == event.window_id) {
                interaction_.reset();
            }
            stacking_.remove(event.window_id);
            focus_.handle_window_removed_or_hidden(event.window_id);
            break;
        }
        case WindowEventType::StateChanged: {
            if (event.window && event.window->state() == WindowState::Minimized) {
                focus_.handle_window_removed_or_hidden(event.window_id);
            }
            break;
        }
        default: break;
    }
}

Status WindowManager::activate(WindowId id) {
    return focus_.activate(id);
}

Status WindowManager::close(WindowId id) {
    return backend_->close(id);
}

Status WindowManager::maximize(WindowId id) {
    auto win = registry_.lookup(id);
    if (!win) return Status::error(core::ErrorCategory::Window, core::ErrorCode::WindowNotFound, "Window not found");

    display::DisplayInfo disp = get_active_display(display_mgr_);
    return state_ctrl_.maximize(win, disp);
}

Status WindowManager::restore(WindowId id) {
    auto win = registry_.lookup(id);
    if (!win) return Status::error(core::ErrorCategory::Window, core::ErrorCode::WindowNotFound, "Window not found");

    display::DisplayInfo disp = get_active_display(display_mgr_);
    return state_ctrl_.restore(win, disp);
}

Status WindowManager::minimize(WindowId id) {
    auto win = registry_.lookup(id);
    if (!win) return Status::error(core::ErrorCategory::Window, core::ErrorCode::WindowNotFound, "Window not found");

    auto status = state_ctrl_.minimize(win);
    if (status.is_ok()) {
        focus_.handle_window_removed_or_hidden(id);
    }
    return status;
}

Status WindowManager::fullscreen(WindowId id) {
    auto win = registry_.lookup(id);
    if (!win) return Status::error(core::ErrorCategory::Window, core::ErrorCode::WindowNotFound, "Window not found");

    display::DisplayInfo disp = get_active_display(display_mgr_);
    return state_ctrl_.fullscreen(win, disp);
}

Status WindowManager::toggle_maximize(WindowId id) {
    auto win = registry_.lookup(id);
    if (!win) return Status::error(core::ErrorCategory::Window, core::ErrorCode::WindowNotFound, "Window not found");

    display::DisplayInfo disp = get_active_display(display_mgr_);
    return state_ctrl_.toggle_maximize(win, disp);
}

Status WindowManager::toggle_fullscreen(WindowId id) {
    auto win = registry_.lookup(id);
    if (!win) return Status::error(core::ErrorCategory::Window, core::ErrorCode::WindowNotFound, "Window not found");

    display::DisplayInfo disp = get_active_display(display_mgr_);
    return state_ctrl_.toggle_fullscreen(win, disp);
}

Status WindowManager::raise(WindowId id) {
    if (!registry_.lookup(id)) {
        return Status::error(core::ErrorCategory::Window, core::ErrorCode::WindowNotFound, "Window not found");
    }
    stacking_.raise(id);
    return Status::ok();
}

Status WindowManager::lower(WindowId id) {
    if (!registry_.lookup(id)) {
        return Status::error(core::ErrorCategory::Window, core::ErrorCode::WindowNotFound, "Window not found");
    }
    stacking_.lower(id);
    return Status::ok();
}

Status WindowManager::set_geometry(WindowId id, const core::Rect& geom) {
    return backend_->set_geometry(id, geom);
}

bool WindowManager::start_move(WindowId id, const core::Point& start_pos, bool is_touch) {
    auto win = registry_.lookup(id);
    if (!win || win->state() == WindowState::Minimized || win->state() == WindowState::Fullscreen) {
        return false;
    }

    static_cast<void>(focus_.activate(id));
    display::DisplayInfo disp = get_active_display(display_mgr_);

    core::Rect usable = placement_.get_usable_area(disp);
    return interaction_.start_move(id, start_pos, win->geometry(), usable, is_touch);
}

core::Rect WindowManager::update_move(const core::Point& current_pos) {
    return interaction_.update_move(current_pos);
}

core::Rect WindowManager::end_move() {
    std::optional<WindowId> win_id = interaction_.active_window_id();
    core::Rect final_geom = interaction_.end_move();

    if (win_id.has_value()) {
        auto win = registry_.lookup(win_id.value());
        if (win) {
            win->set_geometry(final_geom);
        }
        static_cast<void>(backend_->set_geometry(win_id.value(), final_geom));
    }
    return final_geom;
}

core::Rect WindowManager::cancel_move() {
    std::optional<WindowId> win_id = interaction_.active_window_id();
    core::Rect initial_geom = interaction_.cancel_move();

    if (win_id.has_value()) {
        auto win = registry_.lookup(win_id.value());
        if (win) {
            win->set_geometry(initial_geom);
        }
        static_cast<void>(backend_->set_geometry(win_id.value(), initial_geom));
    }
    return initial_geom;
}

bool WindowManager::start_resize(WindowId id, ResizeEdge edge, const core::Point& start_pos, bool is_touch) {
    auto win = registry_.lookup(id);
    if (!win || win->state() == WindowState::Minimized || win->state() == WindowState::Fullscreen) {
        return false;
    }

    static_cast<void>(focus_.activate(id));
    display::DisplayInfo disp = get_active_display(display_mgr_);

    core::Rect usable = placement_.get_usable_area(disp);
    return interaction_.start_resize(id, edge, start_pos, win->geometry(), usable, win->min_size(), win->max_size(), is_touch);
}

core::Rect WindowManager::update_resize(const core::Point& current_pos) {
    return interaction_.update_resize(current_pos);
}

core::Rect WindowManager::end_resize() {
    std::optional<WindowId> win_id = interaction_.active_window_id();
    core::Rect final_geom = interaction_.end_resize();

    if (win_id.has_value()) {
        auto win = registry_.lookup(win_id.value());
        if (win) {
            win->set_geometry(final_geom);
            win->set_surface_size(core::Size{final_geom.width, final_geom.height});
        }
        static_cast<void>(backend_->set_geometry(win_id.value(), final_geom));
    }
    return final_geom;
}

core::Rect WindowManager::cancel_resize() {
    std::optional<WindowId> win_id = interaction_.active_window_id();
    core::Rect initial_geom = interaction_.cancel_resize();

    if (win_id.has_value()) {
        auto win = registry_.lookup(win_id.value());
        if (win) {
            win->set_geometry(initial_geom);
            win->set_surface_size(core::Size{initial_geom.width, initial_geom.height});
        }
        static_cast<void>(backend_->set_geometry(win_id.value(), initial_geom));
    }
    return initial_geom;
}

bool WindowManager::handle_pointer_click(const core::Point& pos, uint32_t timestamp_ms) {
    auto visible = stacking_.visible_stack(registry_);
    // Check top to bottom
    for (auto it = visible.rbegin(); it != visible.rend(); ++it) {
        auto win = registry_.lookup(*it);
        if (!win) continue;

        HeaderHitResult hit = controls_.hit_test(win->geometry(), pos, timestamp_ms, false);
        if (hit.button == WindowControlButton::Close) {
            static_cast<void>(close(win->id()));
            return true;
        }
        if (hit.button == WindowControlButton::MaximizeRestore) {
            static_cast<void>(toggle_maximize(win->id()));
            return true;
        }
        if (hit.button == WindowControlButton::Minimize) {
            static_cast<void>(minimize(win->id()));
            return true;
        }
        if (hit.button == WindowControlButton::TitleDragArea) {
            if (hit.is_double_tap) {
                static_cast<void>(toggle_maximize(win->id()));
            } else {
                static_cast<void>(activate(win->id()));
            }
            return true;
        }

        if (win->geometry().contains(pos)) {
            static_cast<void>(activate(win->id()));
            return true;
        }
    }
    return false;
}

bool WindowManager::handle_touch_tap(const core::Point& pos, uint32_t timestamp_ms) {
    auto visible = stacking_.visible_stack(registry_);
    // Check top to bottom with mobile-sized touch margins
    for (auto it = visible.rbegin(); it != visible.rend(); ++it) {
        auto win = registry_.lookup(*it);
        if (!win) continue;

        HeaderHitResult hit = controls_.hit_test(win->geometry(), pos, timestamp_ms, true);
        if (hit.button == WindowControlButton::Close) {
            static_cast<void>(close(win->id()));
            return true;
        }
        if (hit.button == WindowControlButton::MaximizeRestore) {
            static_cast<void>(toggle_maximize(win->id()));
            return true;
        }
        if (hit.button == WindowControlButton::Minimize) {
            static_cast<void>(minimize(win->id()));
            return true;
        }
        if (hit.button == WindowControlButton::TitleDragArea) {
            if (hit.is_double_tap) {
                static_cast<void>(toggle_maximize(win->id()));
            } else {
                static_cast<void>(activate(win->id()));
            }
            return true;
        }

        if (win->geometry().contains(pos)) {
            static_cast<void>(activate(win->id()));
            return true;
        }
    }
    return false;
}

void WindowManager::handle_display_change(const display::DisplayInfo& display) {
    for (const auto& win : registry_.windows()) {
        if (win) {
            state_ctrl_.adapt_to_display_change(win, display);
        }
    }
    LDDE_LOG_INFO(Window, "WindowManager adapted windows to display change: " << display.width << "x" << display.height);
}

std::vector<std::shared_ptr<Window>> WindowManager::visible_windows() const {
    std::vector<std::shared_ptr<Window>> result;
    auto visible_ids = stacking_.visible_stack(registry_);
    result.reserve(visible_ids.size());

    for (WindowId id : visible_ids) {
        auto win = registry_.lookup(id);
        if (win) result.push_back(win);
    }
    return result;
}

std::vector<std::shared_ptr<Window>> WindowManager::minimized_windows() const {
    std::vector<std::shared_ptr<Window>> result;
    for (const auto& win : registry_.windows()) {
        if (win && win->state() == WindowState::Minimized) {
            result.push_back(win);
        }
    }
    return result;
}

} // namespace ldde::window
