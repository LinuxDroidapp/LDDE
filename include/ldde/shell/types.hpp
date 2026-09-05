#pragma once

#include <string_view>
#include <cstdint>

namespace ldde::shell {

enum class ShellRegionType {
    None = 0,
    Desktop,
    Status,
    Dock,
    Overlay
};

enum class ShellLayer {
    Background = 0,
    Bottom,
    Top,
    Overlay
};

enum class ShellLifecycleState {
    Created = 0,
    Initializing,
    CreatingSurfaces,
    LayoutReady,
    Ready,
    Running,
    Stopping,
    Stopped,
    Failed
};

enum class DockPosition {
    Bottom = 0,
    Top,
    Left,
    Right
};

[[nodiscard]] std::string_view shell_region_name(ShellRegionType region) noexcept;
[[nodiscard]] std::string_view shell_layer_name(ShellLayer layer) noexcept;
[[nodiscard]] std::string_view shell_state_name(ShellLifecycleState state) noexcept;
[[nodiscard]] std::string_view dock_position_name(DockPosition pos) noexcept;

} // namespace ldde::shell
