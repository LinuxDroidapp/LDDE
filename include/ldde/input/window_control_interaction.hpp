#pragma once

#include <optional>
#include "ldde/core/types.hpp"
#include "ldde/window/types.hpp"
#include "ldde/window/window_manager.hpp"
#include "ldde/input/touch_hit_testing.hpp"

namespace ldde::input {

class WindowControlInteraction {
public:
    WindowControlInteraction() = default;

    void press(
        window::WindowId id,
        HitTargetType target,
        const core::Rect& touch_rect);

    bool update_point(const core::Point& current_pos);

    bool release(const core::Point& release_pos, window::WindowManager& wm);
    void cancel() noexcept;
    void reset() noexcept;

    [[nodiscard]] bool is_active() const noexcept { return window_id_.has_value(); }
    [[nodiscard]] bool is_pressed() const noexcept { return is_pressed_; }
    [[nodiscard]] std::optional<window::WindowId> window_id() const noexcept { return window_id_; }
    [[nodiscard]] HitTargetType target_type() const noexcept { return target_; }
    [[nodiscard]] const core::Rect& target_rect() const noexcept { return target_rect_; }

private:
    std::optional<window::WindowId> window_id_;
    HitTargetType target_ = HitTargetType::None;
    core::Rect target_rect_{0, 0, 0, 0};
    bool is_pressed_ = false;
};

} // namespace ldde::input
