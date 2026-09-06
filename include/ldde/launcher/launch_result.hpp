#pragma once

#include <string>
#include <string_view>
#include <sys/types.h>

namespace ldde::launcher {

enum class LaunchStatus {
    Success = 0,
    NotFound,
    InvalidMetadata,
    BackendUnavailable,
    ExecutionFailed,
    PermissionFailure,
    EnvironmentFailure,
    UnknownFailure
};

[[nodiscard]] std::string_view launch_status_name(LaunchStatus status) noexcept;

struct LaunchResult {
    LaunchStatus status = LaunchStatus::Success;
    std::string error_message;
    pid_t pid = -1;

    [[nodiscard]] bool is_success() const noexcept {
        return status == LaunchStatus::Success;
    }

    [[nodiscard]] static LaunchResult success(pid_t pid = -1) {
        LaunchResult r;
        r.status = LaunchStatus::Success;
        r.pid = pid;
        return r;
    }

    [[nodiscard]] static LaunchResult failure(LaunchStatus st, std::string msg) {
        LaunchResult r;
        r.status = st;
        r.error_message = std::move(msg);
        return r;
    }
};

} // namespace ldde::launcher

