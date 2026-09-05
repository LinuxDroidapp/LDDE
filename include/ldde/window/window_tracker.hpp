#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <wayland-client.h>
#include <wayland-server.h>
#include "ldde/core/error.hpp"
#include "ldde/config/config.hpp"
#include "ldde/wayland/connection.hpp"
#include "ldde/wayland/registry.hpp"
#include "ldde/window/types.hpp"
#include "ldde/window/window.hpp"
#include "ldde/window/window_registry.hpp"
#include "ldde/wayland/xdg-shell-client-protocol.h"

namespace ldde::window {

using core::Status;

class WindowTracker {
public:
    WindowTracker();
    ~WindowTracker();

    WindowTracker(const WindowTracker&) = delete;
    WindowTracker& operator=(const WindowTracker&) = delete;

    Status initialize(wayland::WaylandConnection& connection,
                      wayland::WaylandRegistry& registry,
                      WindowRegistry& window_registry,
                      const config::Config& config);
    void shutdown() noexcept;

    [[nodiscard]] bool is_initialized() const noexcept { return initialized_; }
    [[nodiscard]] uint32_t xdg_wm_base_version() const noexcept { return xdg_version_; }
    [[nodiscard]] xdg_wm_base* wm_base() const noexcept { return wm_base_; }

    std::shared_ptr<Window> create_tracked_window(wl_surface* surface,
                                                  std::string_view title = {},
                                                  std::string_view app_id = {});
    void destroy_window(WindowId id);

    // Application tracking server
    Status start_application_server(std::string_view socket_name = {});
    void stop_application_server() noexcept;
    [[nodiscard]] const std::string& application_socket_name() const noexcept { return app_socket_name_; }
    [[nodiscard]] int server_fd() const noexcept { return server_fd_; }
    void dispatch_server();
    void send_close_to_client(WindowId id);
    void send_configure_to_client(WindowId id, int32_t width, int32_t height, const std::vector<uint32_t>& states = {});

    [[nodiscard]] WindowRegistry* registry() noexcept { return registry_; }
    [[nodiscard]] wl_display* server_display() noexcept { return server_display_; }

    // Protocol callbacks
    void on_wm_base_ping(uint32_t serial);
    void on_toplevel_configure(xdg_toplevel* toplevel, int32_t width, int32_t height, wl_array* states);
    void on_toplevel_close(xdg_toplevel* toplevel);
    void on_surface_configure(xdg_surface* xdg_surface, uint32_t serial);

    WindowId generate_id() noexcept;
    void register_server_toplevel(WindowId id, void* toplevel_data);
    void unregister_server_toplevel(WindowId id);

private:
    bool initialized_ = false;
    WindowRegistry* registry_ = nullptr;
    xdg_wm_base* wm_base_ = nullptr;
    uint32_t xdg_version_ = 0;
    WindowId next_window_id_ = 1;

    // Application server tracking state
    wl_display* server_display_ = nullptr;
    std::string app_socket_name_;
    int server_fd_ = -1;
    wl_global* compositor_global_ = nullptr;
    wl_global* wm_base_global_ = nullptr;
    std::unordered_map<WindowId, void*> server_toplevels_;

    static const xdg_wm_base_listener wm_base_listener_;
    static const xdg_surface_listener surface_listener_;
    static const xdg_toplevel_listener toplevel_listener_;

    void setup_server_globals();
};

} // namespace ldde::window
