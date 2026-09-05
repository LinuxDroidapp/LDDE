#pragma once

#include <string>
#include <string_view>
#include <variant>
#include <utility>
#include <optional>
#include <sstream>

namespace ldde::core {

enum class ErrorCategory {
    Configuration,
    Wayland,
    Input,
    Display,
    Application,
    Window,
    Session,
    Resource,
    Internal
};

enum class ErrorCode {
    Ok = 0,
    // Configuration
    ConfigNotFound,
    ConfigParseError,
    ConfigVersionMismatch,
    ConfigValidationFailed,
    // Wayland
    WaylandConnectionFailed,
    WaylandDisconnected,
    WaylandProtocolError,
    WaylandGlobalMissing,
    WaylandBindFailed,
    // Display
    DisplayInitFailed,
    DisplayNotFound,
    DisplayInvalidGeometry,
    // Input
    InputInitFailed,
    SeatUnavailable,
    DeviceNotFound,
    // Session
    SessionEnvironmentMissing,
    SessionInvalidState,
    // Resource
    ResourceExhausted,
    IoError,
    // Application
    ApplicationAlreadyRunning,
    InvalidLifecycleTransition,
    ReadinessNotificationFailed,
    // Window
    WindowNotFound,
    WindowManagementUnavailable,
    InvalidWindowState,
    // Internal
    InvalidArgument,
    NotImplemented,
    Unknown
};

[[nodiscard]] std::string_view error_category_name(ErrorCategory category) noexcept;
[[nodiscard]] std::string_view error_code_name(ErrorCode code) noexcept;

class Error {
public:
    Error() = default;
    Error(ErrorCategory category, ErrorCode code, std::string message,
          std::string_view file = {}, int line = 0)
        : category_(category), code_(code), message_(std::move(message)),
          file_(file), line_(line) {}

    [[nodiscard]] ErrorCategory category() const noexcept { return category_; }
    [[nodiscard]] ErrorCode code() const noexcept { return code_; }
    [[nodiscard]] const std::string& message() const noexcept { return message_; }
    [[nodiscard]] std::string_view file() const noexcept { return file_; }
    [[nodiscard]] int line() const noexcept { return line_; }

    [[nodiscard]] std::string to_string() const {
        std::ostringstream oss;
        oss << "[" << error_category_name(category_) << ":" << error_code_name(code_) << "] "
            << message_;
        if (!file_.empty()) {
            oss << " (" << file_ << ":" << line_ << ")";
        }
        return oss.str();
    }

private:
    ErrorCategory category_ = ErrorCategory::Internal;
    ErrorCode code_ = ErrorCode::Unknown;
    std::string message_;
    std::string_view file_;
    int line_ = 0;
};

class Status {
public:
    Status() : is_ok_(true) {}
    Status(Error error) : error_(std::move(error)), is_ok_(false) {} // NOLINT

    [[nodiscard]] static Status ok() noexcept { return Status(); }
    [[nodiscard]] static Status error(ErrorCategory category, ErrorCode code, std::string message,
                                      std::string_view file = {}, int line = 0) {
        return Status(Error(category, code, std::move(message), file, line));
    }

    [[nodiscard]] bool is_ok() const noexcept { return is_ok_; }
    [[nodiscard]] bool is_error() const noexcept { return !is_ok_; }
    [[nodiscard]] explicit operator bool() const noexcept { return is_ok_; }

    [[nodiscard]] const Error& error() const noexcept { return error_; }
    [[nodiscard]] std::string to_string() const {
        return is_ok_ ? "OK" : error_.to_string();
    }

private:
    Error error_;
    bool is_ok_ = true;
};

template <typename T>
class Result {
public:
    Result(const T& value) : data_(value) {} // NOLINT
    Result(T&& value) : data_(std::move(value)) {} // NOLINT
    Result(Error error) : data_(std::move(error)) {} // NOLINT

    [[nodiscard]] bool is_ok() const noexcept {
        return std::holds_alternative<T>(data_);
    }
    [[nodiscard]] bool is_error() const noexcept {
        return std::holds_alternative<Error>(data_);
    }
    [[nodiscard]] explicit operator bool() const noexcept {
        return is_ok();
    }

    [[nodiscard]] const T& value() const & {
        return std::get<T>(data_);
    }
    [[nodiscard]] T& value() & {
        return std::get<T>(data_);
    }
    [[nodiscard]] T&& value() && {
        return std::get<T>(std::move(data_));
    }

    [[nodiscard]] const Error& error() const {
        return std::get<Error>(data_);
    }

private:
    std::variant<T, Error> data_;
};

#define LDDE_MAKE_ERROR(category, code, msg) \
    ::ldde::core::Error(category, code, (msg), __FILE__, __LINE__)

#define LDDE_STATUS_ERROR(category, code, msg) \
    ::ldde::core::Status::error(category, code, (msg), __FILE__, __LINE__)

} // namespace ldde::core

