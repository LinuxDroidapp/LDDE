#include "ldde/core/error.hpp"

namespace ldde::core {

std::string_view error_category_name(ErrorCategory category) noexcept {
    switch (category) {
        case ErrorCategory::Configuration: return "Configuration";
        case ErrorCategory::Wayland:       return "Wayland";
        case ErrorCategory::Input:         return "Input";
        case ErrorCategory::Display:       return "Display";
        case ErrorCategory::Application:   return "Application";
        case ErrorCategory::Window:        return "Window";
        case ErrorCategory::Session:       return "Session";
        case ErrorCategory::Resource:      return "Resource";
        case ErrorCategory::Internal:      return "Internal";
    }
    return "Unknown";
}

std::string_view error_code_name(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::Ok:                           return "Ok";
        case ErrorCode::ConfigNotFound:               return "ConfigNotFound";
        case ErrorCode::ConfigParseError:             return "ConfigParseError";
        case ErrorCode::ConfigVersionMismatch:        return "ConfigVersionMismatch";
        case ErrorCode::ConfigValidationFailed:       return "ConfigValidationFailed";
        case ErrorCode::WaylandConnectionFailed:      return "WaylandConnectionFailed";
        case ErrorCode::WaylandDisconnected:          return "WaylandDisconnected";
        case ErrorCode::WaylandProtocolError:         return "WaylandProtocolError";
        case ErrorCode::WaylandGlobalMissing:         return "WaylandGlobalMissing";
        case ErrorCode::WaylandBindFailed:            return "WaylandBindFailed";
        case ErrorCode::DisplayInitFailed:            return "DisplayInitFailed";
        case ErrorCode::DisplayNotFound:              return "DisplayNotFound";
        case ErrorCode::DisplayInvalidGeometry:       return "DisplayInvalidGeometry";
        case ErrorCode::InputInitFailed:              return "InputInitFailed";
        case ErrorCode::SeatUnavailable:              return "SeatUnavailable";
        case ErrorCode::DeviceNotFound:               return "DeviceNotFound";
        case ErrorCode::SessionEnvironmentMissing:    return "SessionEnvironmentMissing";
        case ErrorCode::SessionInvalidState:          return "SessionInvalidState";
        case ErrorCode::ResourceExhausted:            return "ResourceExhausted";
        case ErrorCode::IoError:                      return "IoError";
        case ErrorCode::ApplicationAlreadyRunning:    return "ApplicationAlreadyRunning";
        case ErrorCode::InvalidLifecycleTransition:   return "InvalidLifecycleTransition";
        case ErrorCode::ReadinessNotificationFailed:  return "ReadinessNotificationFailed";
        case ErrorCode::InvalidArgument:              return "InvalidArgument";
        case ErrorCode::NotImplemented:               return "NotImplemented";
        case ErrorCode::Unknown:                      return "Unknown";
    }
    return "Unknown";
}

} // namespace ldde::core

