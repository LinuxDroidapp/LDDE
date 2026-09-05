#include "ldde/wayland/connection.hpp"
#include "ldde/core/logging.hpp"

#include <cerrno>
#include <cstring>
#include <cstdlib>

namespace ldde::wayland {

WaylandConnection::WaylandConnection() = default;

WaylandConnection::~WaylandConnection() {
    disconnect();
}

Status WaylandConnection::connect(const std::optional<std::string>& display_name) {
    if (display_) {
        return Status::ok();
    }

    const char* name = nullptr;
    if (display_name.has_value() && !display_name.value().empty()) {
        name = display_name.value().c_str();
        display_name_ = display_name.value();
    } else {
        const char* env_disp = std::getenv("WAYLAND_DISPLAY");
        if (env_disp && *env_disp) {
            name = env_disp;
            display_name_ = env_disp;
        }
    }

    wl_display* disp = wl_display_connect(name);
    if (!disp) {
        std::string err_msg = "Failed to connect to Wayland display";
        if (name) {
            err_msg += " (";
            err_msg += name;
            err_msg += ")";
        }
        err_msg += ": ";
        err_msg += std::strerror(errno);
        return LDDE_STATUS_ERROR(core::ErrorCategory::Wayland,
                                 core::ErrorCode::WaylandConnectionFailed,
                                 err_msg);
    }

    display_.reset(disp);
    LDDE_LOG_INFO(Wayland, "Connected to Wayland compositor" << (name ? (std::string(" (") + name + ")") : ""));
    return Status::ok();
}

void WaylandConnection::disconnect() noexcept {
    if (reading_) {
        cancel_read();
    }

    if (display_) {
        LDDE_LOG_INFO(Wayland, "Disconnecting from Wayland compositor");
        display_.reset();
    }
}

int WaylandConnection::fd() const noexcept {
    if (!display_) {
        return -1;
    }
    return wl_display_get_fd(display_.get());
}

Status WaylandConnection::roundtrip() {
    if (!display_) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Wayland,
                                 core::ErrorCode::WaylandDisconnected,
                                 "Not connected to Wayland");
    }

    int ret = wl_display_roundtrip(display_.get());
    if (ret < 0) {
        Status err = check_error();
        if (disconnect_callback_) {
            disconnect_callback_();
        }
        return err;
    }

    return Status::ok();
}

Status WaylandConnection::flush() {
    if (!display_) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Wayland,
                                 core::ErrorCode::WaylandDisconnected,
                                 "Not connected to Wayland");
    }

    while (true) {
        int ret = wl_display_flush(display_.get());
        if (ret >= 0) {
            return Status::ok();
        }
        if (errno == EAGAIN) {
            // Buffer full, would block
            return Status::ok();
        }
        if (errno == EINTR) {
            continue;
        }
        Status err = check_error();
        if (disconnect_callback_) {
            disconnect_callback_();
        }
        return err;
    }
}

Status WaylandConnection::dispatch_pending() {
    if (!display_) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Wayland,
                                 core::ErrorCode::WaylandDisconnected,
                                 "Not connected to Wayland");
    }

    int ret = wl_display_dispatch_pending(display_.get());
    if (ret < 0) {
        Status err = check_error();
        if (disconnect_callback_) {
            disconnect_callback_();
        }
        return err;
    }

    return Status::ok();
}

bool WaylandConnection::prepare_read() {
    if (!display_) {
        return false;
    }

    if (reading_) {
        return true;
    }

    while (wl_display_prepare_read(display_.get()) != 0) {
        if (wl_display_dispatch_pending(display_.get()) < 0) {
            return false;
        }
    }

    reading_ = true;
    return true;
}

Status WaylandConnection::read_events() {
    if (!display_) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Wayland,
                                 core::ErrorCode::WaylandDisconnected,
                                 "Not connected to Wayland");
    }

    if (!reading_) {
        return Status::ok();
    }

    reading_ = false;

    if (wl_display_read_events(display_.get()) < 0) {
        Status err = check_error();
        if (disconnect_callback_) {
            disconnect_callback_();
        }
        return err;
    }

    return dispatch_pending();
}

void WaylandConnection::cancel_read() {
    if (!display_ || !reading_) {
        return;
    }

    wl_display_cancel_read(display_.get());
    reading_ = false;
}

Status WaylandConnection::check_error() const {
    if (!display_) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Wayland,
                                 core::ErrorCode::WaylandDisconnected,
                                 "Wayland display null");
    }

    int err = wl_display_get_error(display_.get());
    if (err == 0) {
        return Status::ok();
    }

    if (err == EPIPE || err == ECONNRESET) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Wayland,
                                 core::ErrorCode::WaylandDisconnected,
                                 "Wayland compositor connection closed");
    }

    return LDDE_STATUS_ERROR(core::ErrorCategory::Wayland,
                             core::ErrorCode::WaylandProtocolError,
                             std::string("Wayland error: ") + std::strerror(err));
}

} // namespace ldde::wayland

