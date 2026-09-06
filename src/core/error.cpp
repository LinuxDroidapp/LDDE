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
        case ErrorCategory::Dock:          return "Dock";
        case ErrorCategory::Switcher:      return "Switcher";
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
        case ErrorCode::DisplayUnavailable:           return "DisplayUnavailable";
        case ErrorCode::InvalidOutput:                return "InvalidOutput";
        case ErrorCode::InvalidScale:                 return "InvalidScale";
        case ErrorCode::InvalidGeometry:              return "InvalidGeometry";
        case ErrorCode::UnsupportedTransform:         return "UnsupportedTransform";
        case ErrorCode::InvalidSafeArea:              return "InvalidSafeArea";
        case ErrorCode::NoPrimaryDisplay:             return "NoPrimaryDisplay";
        case ErrorCode::DisplayPolicyError:           return "DisplayPolicyError";
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
        case ErrorCode::WindowNotFound:               return "WindowNotFound";
        case ErrorCode::WindowManagementUnavailable:  return "WindowManagementUnavailable";
        case ErrorCode::InvalidWindowState:           return "InvalidWindowState";
        case ErrorCode::InvalidTouchTarget:           return "InvalidTouchTarget";
        case ErrorCode::WindowDestroyedDuringInteraction: return "WindowDestroyedDuringInteraction";
        case ErrorCode::InteractionCancelled:         return "InteractionCancelled";
        case ErrorCode::UnsupportedResize:            return "UnsupportedResize";
        case ErrorCode::InvalidCoordinateSpace:       return "InvalidCoordinateSpace";
        case ErrorCode::InteractionStateError:        return "InteractionStateError";
        case ErrorCode::DiscoveryPathUnavailable:     return "DiscoveryPathUnavailable";
        case ErrorCode::DesktopEntryParseError:       return "DesktopEntryParseError";
        case ErrorCode::DesktopEntryInvalid:          return "DesktopEntryInvalid";
        case ErrorCode::ApplicationMetadataInvalid:   return "ApplicationMetadataInvalid";
        case ErrorCode::CatalogRefreshFailed:         return "CatalogRefreshFailed";
        case ErrorCode::FilesystemError:              return "FilesystemError";
        case ErrorCode::UnsupportedDesktopEntry:      return "UnsupportedDesktopEntry";
        case ErrorCode::ApplicationNotFound:          return "ApplicationNotFound";
        case ErrorCode::LauncherNotOpen:              return "LauncherNotOpen";
        case ErrorCode::LauncherAlreadyOpen:          return "LauncherAlreadyOpen";
        case ErrorCode::LauncherInvalidState:         return "LauncherInvalidState";
        case ErrorCode::LaunchFailed:                 return "LaunchFailed";
        case ErrorCode::LaunchBackendUnavailable:     return "LaunchBackendUnavailable";
        case ErrorCode::LaunchExecutableNotFound:     return "LaunchExecutableNotFound";
        case ErrorCode::LaunchPermissionDenied:       return "LaunchPermissionDenied";
        case ErrorCode::IconNotFound:                 return "IconNotFound";
        case ErrorCode::DockNotVisible:               return "DockNotVisible";
        case ErrorCode::DockItemNotFound:             return "DockItemNotFound";
        case ErrorCode::DockInvalidState:             return "DockInvalidState";
        case ErrorCode::DockActivationFailed:         return "DockActivationFailed";
        case ErrorCode::SwitcherNotOpen:              return "SwitcherNotOpen";
        case ErrorCode::SwitcherAlreadyOpen:          return "SwitcherAlreadyOpen";
        case ErrorCode::SwitcherInvalidState:         return "SwitcherInvalidState";
        case ErrorCode::SwitcherNoWindows:            return "SwitcherNoWindows";
        case ErrorCode::SwitcherWindowNotFound:       return "SwitcherWindowNotFound";
        case ErrorCode::SwitcherActivationFailed:     return "SwitcherActivationFailed";
        case ErrorCode::SwitcherRestoreFailed:        return "SwitcherRestoreFailed";
        case ErrorCode::InvalidArgument:              return "InvalidArgument";
        case ErrorCode::NotImplemented:               return "NotImplemented";
        case ErrorCode::Unknown:                      return "Unknown";
    }
    return "Unknown";
}

} // namespace ldde::core

