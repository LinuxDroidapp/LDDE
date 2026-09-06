#pragma once

#include <optional>
#include "ldde/core/types.hpp"
#include "ldde/window/types.hpp"
#include "ldde/window/window_manager.hpp"

namespace ldde::input {

class WindowDragController {
public:
    WindowDragController() = default;

    bool start(
        window::WindowId id,
        const core::Point& touch_pos,
        const core::Rect& initial_geom,
        window::WindowManager& wm);

    core::Rect update(const core::Point& touch_pos, window::WindowManager& wm);
    core::Rect finish(window::WindowManager& wm);
    core::Rect cancel(window::WindowManager& wm);

    [[nodiscard]] bool is_active() const noexcept { return is_active_; }
    [[nodiscard]] std::optional<window::WindowId> window_id() const noexcept { return window_id_; }
    [[nodiscard]] const core::Point& start_touch_pos() const noexcept { return start_touch_pos_; }
    [[nodiscard]] const core::Rect& initial_geometry() const noexcept { return initial_geom_; }
    [[nodiscard]] const core::Rect& current_geometry() const noexcept { return current_geom_; }

    void reset() noexcept;

private:
    bool is_active_ = false;
    std::optional<window::WindowId> window_id_;
    core::Point start_touch_pos_{0, 0};
    core::Rect initial_geom_{0, 0, 0, 0};
    core::Rect current_geom_{0, 0, 0, 0};
};

} // namespace ldde::input
