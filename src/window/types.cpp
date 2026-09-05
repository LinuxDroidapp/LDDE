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

std::string_view resize_edge_name(ResizeEdge edge) noexcept {
    switch (edge) {
        case ResizeEdge::None:        return "None";
        case ResizeEdge::Top:         return "Top";
        case ResizeEdge::Bottom:      return "Bottom";
        case ResizeEdge::Left:        return "Left";
        case ResizeEdge::Right:       return "Right";
        case ResizeEdge::TopLeft:     return "TopLeft";
        case ResizeEdge::TopRight:    return "TopRight";
        case ResizeEdge::BottomLeft:  return "BottomLeft";
        case ResizeEdge::BottomRight: return "BottomRight";
    }
    return "Unknown";
}

std::string_view management_event_name(WindowManagementEvent event) noexcept {
    switch (event) {
        case WindowManagementEvent::Activated:           return "Activated";
        case WindowManagementEvent::Deactivated:         return "Deactivated";
        case WindowManagementEvent::MoveStarted:         return "MoveStarted";
        case WindowManagementEvent::Moved:               return "Moved";
        case WindowManagementEvent::MoveFinished:        return "MoveFinished";
        case WindowManagementEvent::ResizeStarted:       return "ResizeStarted";
        case WindowManagementEvent::Resized:             return "Resized";
        case WindowManagementEvent::ResizeFinished:      return "ResizeFinished";
        case WindowManagementEvent::MaximizeRequested:   return "MaximizeRequested";
        case WindowManagementEvent::Maximized:           return "Maximized";
        case WindowManagementEvent::RestoreRequested:    return "RestoreRequested";
        case WindowManagementEvent::Restored:            return "Restored";
        case WindowManagementEvent::MinimizeRequested:   return "MinimizeRequested";
        case WindowManagementEvent::Minimized:           return "Minimized";
        case WindowManagementEvent::FullscreenRequested: return "FullscreenRequested";
        case WindowManagementEvent::FullscreenChanged:   return "FullscreenChanged";
        case WindowManagementEvent::CloseRequested:      return "CloseRequested";
    }
    return "Unknown";
}

} // namespace ldde::window
