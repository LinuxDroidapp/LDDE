#pragma once

#include <string>
#include <optional>
#include <functional>
#include "ldde/core/error.hpp"
#include "ldde/wayland/wrappers.hpp"

namespace ldde::wayland {

using core::Status;

class WaylandConnection {
public:
    using DisconnectCallback = std::function<void()>;

    WaylandConnection();
    ~WaylandConnection();

    WaylandConnection(const WaylandConnection&) = delete;
    WaylandConnection& operator=(const WaylandConnection&) = delete;

    Status connect(const std::optional<std::string>& display_name = std::nullopt);
    void disconnect() noexcept;

    [[nodiscard]] bool is_connected() const noexcept { return display_ != nullptr; }
    [[nodiscard]] wl_display* display() const noexcept { return display_.get(); }
    [[nodiscard]] int fd() const noexcept;

    Status roundtrip();
    Status flush();
    Status dispatch_pending();

    [[nodiscard]] bool prepare_read();
    Status read_events();
    void cancel_read();

    [[nodiscard]] Status check_error() const;

    void set_disconnect_callback(DisconnectCallback callback) {
        disconnect_callback_ = std::move(callback);
    }

private:
    UniqueDisplay display_;
    std::string display_name_;
    bool reading_ = false;
    DisconnectCallback disconnect_callback_;
};

} // namespace ldde::wayland
