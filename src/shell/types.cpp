#include "ldde/shell/types.hpp"

namespace ldde::shell {

std::string_view shell_region_name(ShellRegionType region) noexcept {
    switch (region) {
        case ShellRegionType::None:    return "None";
        case ShellRegionType::Desktop: return "Desktop";
        case ShellRegionType::Status:  return "Status";
        case ShellRegionType::Dock:    return "Dock";
        case ShellRegionType::Overlay: return "Overlay";
    }
    return "Unknown";
}

std::string_view shell_layer_name(ShellLayer layer) noexcept {
    switch (layer) {
        case ShellLayer::Background: return "Background";
        case ShellLayer::Bottom:     return "Bottom";
        case ShellLayer::Top:        return "Top";
        case ShellLayer::Overlay:    return "Overlay";
    }
    return "Unknown";
}

std::string_view shell_state_name(ShellLifecycleState state) noexcept {
    switch (state) {
        case ShellLifecycleState::Created:          return "CREATED";
        case ShellLifecycleState::Initializing:     return "INITIALIZING";
        case ShellLifecycleState::CreatingSurfaces: return "CREATING_SURFACES";
        case ShellLifecycleState::LayoutReady:      return "LAYOUT_READY";
        case ShellLifecycleState::Ready:            return "READY";
        case ShellLifecycleState::Running:          return "RUNNING";
        case ShellLifecycleState::Stopping:         return "STOPPING";
        case ShellLifecycleState::Stopped:          return "STOPPED";
        case ShellLifecycleState::Failed:           return "FAILED";
    }
    return "UNKNOWN";
}

std::string_view dock_position_name(DockPosition pos) noexcept {
    switch (pos) {
        case DockPosition::Bottom: return "bottom";
        case DockPosition::Top:    return "top";
        case DockPosition::Left:   return "left";
        case DockPosition::Right:  return "right";
    }
    return "unknown";
}

} // namespace ldde::shell
