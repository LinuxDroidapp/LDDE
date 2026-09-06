#include "ldde/window/window_registry.hpp"
#include "ldde/core/logging.hpp"
#include <algorithm>

namespace ldde::window {

WindowRegistry::WindowRegistry() = default;

WindowRegistry::~WindowRegistry() {
    clear();
}

Status WindowRegistry::add_window(std::shared_ptr<Window> window) {
    if (!window) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Internal,
                                 core::ErrorCode::InvalidArgument,
                                 "Cannot add null window to registry");
    }

    WindowId id = window->id();
    if (windows_.find(id) != windows_.end()) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Application,
                                 core::ErrorCode::InvalidArgument,
                                 "Window ID already exists in registry");
    }

    windows_[id] = window;
    window_order_.push_back(id);
    if (window->surface()) {
        surface_to_id_[window->surface()] = id;
    }
    if (window->toplevel()) {
        toplevel_to_id_[window->toplevel()] = id;
    }

    LDDE_LOG_INFO(Window, "Window registered [id=" << id
                          << ", app=\"" << window->app_id()
                          << "\", title=\"" << window->title() << "\"] (total: " << windows_.size() << ")");

    dispatch_event(WindowEvent{
        .type = WindowEventType::Created,
        .window_id = id,
        .window = window,
        .property_name = "created"
    });

    return Status::ok();
}

Status WindowRegistry::remove_window(WindowId id) {
    auto it = windows_.find(id);
    if (it == windows_.end()) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Application,
                                 core::ErrorCode::InvalidArgument,
                                 "Window ID not found in registry");
    }

    auto window = it->second;
    window->mark_destroyed();

    if (window->surface()) {
        surface_to_id_.erase(window->surface());
    }
    if (window->toplevel()) {
        toplevel_to_id_.erase(window->toplevel());
    }

    dispatch_event(WindowEvent{
        .type = WindowEventType::Destroyed,
        .window_id = id,
        .window = window,
        .property_name = "destroyed"
    });

    windows_.erase(it);

    auto order_it = std::remove(window_order_.begin(), window_order_.end(), id);
    window_order_.erase(order_it, window_order_.end());

    if (active_window_id_ == id) {
        active_window_id_ = std::nullopt;
        if (!window_order_.empty()) {
            set_active_window(window_order_.back());
        }
    }

    LDDE_LOG_INFO(Window, "Window unregistered [id=" << id << "] (remaining: " << windows_.size() << ")");
    return Status::ok();
}

std::shared_ptr<Window> WindowRegistry::lookup(WindowId id) const noexcept {
    auto it = windows_.find(id);
    if (it != windows_.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<Window> WindowRegistry::find_by_surface(wl_surface* surface) const noexcept {
    if (!surface) return nullptr;
    auto it = surface_to_id_.find(surface);
    if (it != surface_to_id_.end()) {
        return lookup(it->second);
    }
    return nullptr;
}

std::shared_ptr<Window> WindowRegistry::find_by_toplevel(xdg_toplevel* toplevel) const noexcept {
    if (!toplevel) return nullptr;
    auto it = toplevel_to_id_.find(toplevel);
    if (it != toplevel_to_id_.end()) {
        return lookup(it->second);
    }
    return nullptr;
}

std::vector<std::shared_ptr<Window>> WindowRegistry::windows() const {
    std::vector<std::shared_ptr<Window>> result;
    result.reserve(window_order_.size());
    for (WindowId id : window_order_) {
        auto it = windows_.find(id);
        if (it != windows_.end()) {
            result.push_back(it->second);
        }
    }
    return result;
}

std::vector<std::shared_ptr<Window>> WindowRegistry::windows_for_app(std::string_view app_id) const {
    std::vector<std::shared_ptr<Window>> result;
    for (WindowId id : window_order_) {
        auto it = windows_.find(id);
        if (it != windows_.end() && it->second && it->second->app_id() == app_id) {
            result.push_back(it->second);
        }
    }
    return result;
}

size_t WindowRegistry::count() const noexcept {
    return windows_.size();
}

bool WindowRegistry::empty() const noexcept {
    return windows_.empty();
}

void WindowRegistry::for_each(const std::function<void(const std::shared_ptr<Window>&)>& fn) const {
    for (WindowId id : window_order_) {
        auto it = windows_.find(id);
        if (it != windows_.end() && it->second) {
            fn(it->second);
        }
    }
}

void WindowRegistry::set_active_window(std::optional<WindowId> id) {
    if (active_window_id_ == id) {
        return;
    }

    if (active_window_id_) {
        auto prev = lookup(*active_window_id_);
        if (prev) {
            prev->set_active(false);
            dispatch_event(WindowEvent{
                .type = WindowEventType::FocusChanged,
                .window_id = prev->id(),
                .window = prev,
                .property_name = "unfocused"
            });
        }
    }

    active_window_id_ = id;

    if (active_window_id_) {
        auto cur = lookup(*active_window_id_);
        if (cur) {
            cur->set_active(true);
            dispatch_event(WindowEvent{
                .type = WindowEventType::FocusChanged,
                .window_id = cur->id(),
                .window = cur,
                .property_name = "focused"
            });
        }
    }
}

WindowRegistry::ListenerId WindowRegistry::add_listener(WindowEventListener listener) {
    ListenerId id = next_listener_id_++;
    listeners_[id] = std::move(listener);
    return id;
}

void WindowRegistry::remove_listener(ListenerId id) {
    listeners_.erase(id);
}

void WindowRegistry::dispatch_event(const WindowEvent& event) {
    for (const auto& [id, listener] : listeners_) {
        if (listener) {
            listener(event);
        }
    }
}

void WindowRegistry::clear() noexcept {
    for (auto& [id, win] : windows_) {
        if (win) {
            win->mark_destroyed();
        }
    }
    windows_.clear();
    window_order_.clear();
    surface_to_id_.clear();
    toplevel_to_id_.clear();
    active_window_id_ = std::nullopt;
}

} // namespace ldde::window
