#pragma once

#include <cstdint>
#include <string_view>

namespace ldde::window {

using WindowId = uint64_t;
inline constexpr WindowId kInvalidWindowId = 0;

enum class WindowLifecycleState {
    Discovered = 0,
    Initializing,
    Ready,
    Visible,
    Closing,
    Destroyed,
    Failed
};

enum class WindowState {
    Normal = 0,
    Maximized,
    Fullscreen,
    Minimized
};

enum class WindowEventType {
    Created = 0,
    TitleChanged,
    AppIdChanged,
    GeometryChanged,
    StateChanged,
    FocusChanged,
    VisibilityChanged,
    ParentChanged,
    Closed,
    Destroyed
};

enum class ResizeEdge {
    None = 0,
    Top,
    Bottom,
    Left,
    Right,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
};

enum class WindowManagementEvent {
    Activated = 0,
    Deactivated,
    MoveStarted,
    Moved,
    MoveFinished,
    ResizeStarted,
    Resized,
    ResizeFinished,
    MaximizeRequested,
    Maximized,
    RestoreRequested,
    Restored,
    MinimizeRequested,
    Minimized,
    FullscreenRequested,
    FullscreenChanged,
    CloseRequested
};

[[nodiscard]] std::string_view window_lifecycle_name(WindowLifecycleState state) noexcept;
[[nodiscard]] std::string_view window_state_name(WindowState state) noexcept;
[[nodiscard]] std::string_view window_event_name(WindowEventType event) noexcept;
[[nodiscard]] std::string_view resize_edge_name(ResizeEdge edge) noexcept;
[[nodiscard]] std::string_view management_event_name(WindowManagementEvent event) noexcept;

} // namespace ldde::window
