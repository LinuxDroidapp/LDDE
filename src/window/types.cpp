#include "ldde/window/types.hpp"

namespace ldde::window {

std::string_view window_lifecycle_name(WindowLifecycleState state) noexcept {
    switch (state) {
        case WindowLifecycleState::Discovered:   return "DISCOVERED";
        case WindowLifecycleState::Initializing: return "INITIALIZING";
        case WindowLifecycleState::Ready:        return "READY";
        case WindowLifecycleState::Visible:      return "VISIBLE";
        case WindowLifecycleState::Closing:      return "CLOSING";
        case WindowLifecycleState::Destroyed:    return "DESTROYED";
        case WindowLifecycleState::Failed:       return "FAILED";
    }
    return "UNKNOWN";
}

std::string_view window_state_name(WindowState state) noexcept {
    switch (state) {
        case WindowState::Normal:     return "Normal";
        case WindowState::Maximized:  return "Maximized";
        case WindowState::Fullscreen: return "Fullscreen";
        case WindowState::Minimized:  return "Minimized";
    }
    return "Unknown";
}

std::string_view window_event_name(WindowEventType event) noexcept {
    switch (event) {
        case WindowEventType::Created:           return "Created";
        case WindowEventType::TitleChanged:      return "TitleChanged";
        case WindowEventType::AppIdChanged:      return "AppIdChanged";
        case WindowEventType::GeometryChanged:   return "GeometryChanged";
        case WindowEventType::StateChanged:      return "StateChanged";
        case WindowEventType::FocusChanged:      return "FocusChanged";
        case WindowEventType::VisibilityChanged: return "VisibilityChanged";
        case WindowEventType::ParentChanged:     return "ParentChanged";
        case WindowEventType::Closed:            return "Closed";
        case WindowEventType::Destroyed:         return "Destroyed";
    }
    return "Unknown";
}

} // namespace ldde::window
