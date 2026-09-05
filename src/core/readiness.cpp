#include "ldde/core/readiness.hpp"
#include "ldde/core/logging.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <cerrno>

namespace ldde::core {

ReadinessManager::ReadinessManager() = default;

ReadinessManager::~ReadinessManager() {
    if (ready_fd_.has_value() && ready_fd_.value() >= 0) {
        close(ready_fd_.value());
        ready_fd_.reset();
    }
}

void ReadinessManager::set_ready_fd(int fd) noexcept {
    ready_fd_ = fd;
}

void ReadinessManager::set_notify_socket(std::string socket_path) noexcept {
    notify_socket_ = std::move(socket_path);
}

void ReadinessManager::detect_environment() {
    if (!ready_fd_.has_value()) {
        const char* ready_fd_env = std::getenv("LDDE_READY_FD");
        if (!ready_fd_env) {
            ready_fd_env = std::getenv("READY_FD");
        }
        if (ready_fd_env && *ready_fd_env) {
            char* end = nullptr;
            long val = std::strtol(ready_fd_env, &end, 10);
            if (end != ready_fd_env && val >= 0) {
                ready_fd_ = static_cast<int>(val);
                LDDE_LOG_DEBUG(Core, "Detected readiness fd from environment: " << *ready_fd_);
            }
        }
    }

    if (notify_socket_.empty()) {
        const char* notify_env = std::getenv("NOTIFY_SOCKET");
        if (notify_env && *notify_env) {
            notify_socket_ = notify_env;
            LDDE_LOG_DEBUG(Core, "Detected NOTIFY_SOCKET from environment: " << notify_socket_);
        }
    }
}

Status ReadinessManager::report_ready() {
    if (ready_reported_) {
        return Status::ok();
    }

    bool signaled = false;

    if (!notify_socket_.empty()) {
        Status s = notify_socket_ready();
        if (s.is_error()) {
            LDDE_LOG_WARN(Core, "Failed to send readiness to NOTIFY_SOCKET: " << s.to_string());
        } else {
            signaled = true;
            LDDE_LOG_INFO(Core, "Signaled readiness via NOTIFY_SOCKET (" << notify_socket_ << ")");
        }
    }

    if (ready_fd_.has_value()) {
        Status s = notify_fd_ready();
        if (s.is_error()) {
            LDDE_LOG_WARN(Core, "Failed to signal readiness via ready_fd: " << s.to_string());
        } else {
            signaled = true;
            LDDE_LOG_INFO(Core, "Signaled readiness via ready-fd (" << *ready_fd_ << ")");
        }
    }

    ready_reported_ = true;

    if (!signaled && notify_socket_.empty() && !ready_fd_.has_value()) {
        LDDE_LOG_DEBUG(Core, "No external readiness receiver configured; ready state established internally.");
    }

    return Status::ok();
}

Status ReadinessManager::notify_socket_ready() {
    int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        return LDDE_STATUS_ERROR(ErrorCategory::Session, ErrorCode::ReadinessNotificationFailed,
                                 std::string("Failed to create unix socket: ") + std::strerror(errno));
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;

    if (notify_socket_[0] == '@') {
        // Abstract socket
        addr.sun_path[0] = '\0';
        std::strncpy(addr.sun_path + 1, notify_socket_.c_str() + 1, sizeof(addr.sun_path) - 2);
    } else {
        std::strncpy(addr.sun_path, notify_socket_.c_str(), sizeof(addr.sun_path) - 1);
    }

    constexpr std::string_view msg = "READY=1\n";
    ssize_t sent = sendto(fd, msg.data(), msg.size(), 0,
                          reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    close(fd);

    if (sent < 0) {
        return LDDE_STATUS_ERROR(ErrorCategory::Session, ErrorCode::ReadinessNotificationFailed,
                                 std::string("sendto notify socket failed: ") + std::strerror(errno));
    }

    return Status::ok();
}

Status ReadinessManager::notify_fd_ready() {
    if (!ready_fd_.has_value() || ready_fd_.value() < 0) {
        return Status::ok();
    }

    int fd = ready_fd_.value();
    char ready_char = '\n';
    ssize_t written = write(fd, &ready_char, 1);
    close(fd);
    ready_fd_.reset();

    if (written < 0) {
        return LDDE_STATUS_ERROR(ErrorCategory::Session, ErrorCode::ReadinessNotificationFailed,
                                 std::string("write to ready fd failed: ") + std::strerror(errno));
    }

    return Status::ok();
}

} // namespace ldde::core

