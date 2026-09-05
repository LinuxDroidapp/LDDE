#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <optional>
#include "ldde/core/error.hpp"
#include "ldde/window/window.hpp"
#include "ldde/window/window_event.hpp"

namespace ldde::window {

using core::Status;

class WindowRegistry {
public:
    using ListenerId = uint64_t;
    using WindowEventListener = std::function<void(const WindowEvent&)>;

    WindowRegistry();
    ~WindowRegistry();

    WindowRegistry(const WindowRegistry&) = delete;
    WindowRegistry& operator=(const WindowRegistry&) = delete;

    Status add_window(std::shared_ptr<Window> window);
    Status remove_window(WindowId id);

    [[nodiscard]] std::shared_ptr<Window> lookup(WindowId id) const noexcept;
    [[nodiscard]] std::shared_ptr<Window> find_by_surface(wl_surface* surface) const noexcept;
    [[nodiscard]] std::shared_ptr<Window> find_by_toplevel(xdg_toplevel* toplevel) const noexcept;
    [[nodiscard]] std::vector<std::shared_ptr<Window>> windows() const;
    [[nodiscard]] std::vector<std::shared_ptr<Window>> windows_for_app(std::string_view app_id) const;
    [[nodiscard]] size_t count() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

    void for_each(const std::function<void(const std::shared_ptr<Window>&)>& fn) const;

    [[nodiscard]] std::optional<WindowId> active_window_id() const noexcept { return active_window_id_; }
    void set_active_window(std::optional<WindowId> id);

    ListenerId add_listener(WindowEventListener listener);
    void remove_listener(ListenerId id);
    void dispatch_event(const WindowEvent& event);

    void clear() noexcept;

private:
    std::unordered_map<WindowId, std::shared_ptr<Window>> windows_;
    std::vector<WindowId> window_order_;
    std::optional<WindowId> active_window_id_;

    std::unordered_map<ListenerId, WindowEventListener> listeners_;
    ListenerId next_listener_id_ = 1;
};

} // namespace ldde::window
