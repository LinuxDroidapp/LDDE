#pragma once

#include <string>
#include <optional>
#include <functional>
#include "ldde/core/error.hpp"

namespace ldde::core {

class ReadinessManager {
public:
    ReadinessManager();
    ~ReadinessManager();

    ReadinessManager(const ReadinessManager&) = delete;
    ReadinessManager& operator=(const ReadinessManager&) = delete;

    void set_ready_fd(int fd) noexcept;
    void set_notify_socket(std::string socket_path) noexcept;

    void detect_environment();

    Status report_ready();

    [[nodiscard]] bool is_ready_reported() const noexcept { return ready_reported_; }
    [[nodiscard]] std::optional<int> ready_fd() const noexcept { return ready_fd_; }
    [[nodiscard]] const std::string& notify_socket() const noexcept { return notify_socket_; }

private:
    std::optional<int> ready_fd_;
    std::string notify_socket_;
    bool ready_reported_ = false;

    Status notify_socket_ready();
    Status notify_fd_ready();
};

} // namespace ldde::core

