#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <optional>
#include <chrono>
#include <wayland-client.h>
#include "ldde/core/types.hpp"
#include "ldde/core/error.hpp"
#include "ldde/window/types.hpp"
#include "ldde/wayland/xdg-shell-client-protocol.h"

namespace ldde::window {

using core::Status;

class Window {
public:
    Window(WindowId id,
           wl_surface* surface,
           xdg_surface* xdg_surf,
           xdg_toplevel* toplevel);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    [[nodiscard]] WindowId id() const noexcept { return id_; }
    [[nodiscard]] wl_surface* surface() const noexcept { return surface_; }
    [[nodiscard]] xdg_surface* xdg_surf() const noexcept { return xdg_surface_; }
    [[nodiscard]] xdg_toplevel* toplevel() const noexcept { return toplevel_; }

    [[nodiscard]] const std::string& title() const noexcept { return title_; }
    [[nodiscard]] const std::string& app_id() const noexcept { return app_id_; }
    [[nodiscard]] const core::Rect& geometry() const noexcept { return geometry_; }
    [[nodiscard]] const core::Size& surface_size() const noexcept { return surface_size_; }

    [[nodiscard]] WindowState state() const noexcept { return state_; }
    [[nodiscard]] WindowState requested_state() const noexcept { return requested_state_; }
    [[nodiscard]] WindowLifecycleState lifecycle_state() const noexcept { return lifecycle_state_; }
    [[nodiscard]] bool is_active() const noexcept { return is_active_; }
    [[nodiscard]] bool is_visible() const noexcept { return is_visible_; }
    [[nodiscard]] std::optional<WindowId> parent_id() const noexcept { return parent_id_; }
    [[nodiscard]] std::chrono::steady_clock::time_point creation_time() const noexcept { return creation_time_; }
    [[nodiscard]] uint32_t last_configure_serial() const noexcept { return last_configure_serial_; }

    void set_title(std::string_view title);
    void set_app_id(std::string_view app_id);
    void set_geometry(const core::Rect& geom);
    void set_surface_size(const core::Size& size);
    void set_state(WindowState state);
    void set_requested_state(WindowState state);
    void set_active(bool active);
    void set_visible(bool visible);
    void set_parent_id(std::optional<WindowId> parent_id);
    void set_last_configure_serial(uint32_t serial) noexcept { last_configure_serial_ = serial; }

    Status transition_to(WindowLifecycleState next_state);

    void request_close();
    void ack_configure(uint32_t serial);

    void mark_destroyed() noexcept;

private:
    WindowId id_;
    wl_surface* surface_ = nullptr;
    xdg_surface* xdg_surface_ = nullptr;
    xdg_toplevel* toplevel_ = nullptr;

    std::string title_;
    std::string app_id_;
    core::Rect geometry_{0, 0, 0, 0};
    core::Size surface_size_{0, 0};

    WindowState state_ = WindowState::Normal;
    WindowState requested_state_ = WindowState::Normal;
    WindowLifecycleState lifecycle_state_ = WindowLifecycleState::Discovered;
    bool is_active_ = false;
    bool is_visible_ = false;
    std::optional<WindowId> parent_id_;
    std::chrono::steady_clock::time_point creation_time_;
    uint32_t last_configure_serial_ = 0;
};

} // namespace ldde::window
