#pragma once

#include <optional>
#include <vector>
#include <memory>
#include <string_view>
#include "ldde/core/types.hpp"
#include "ldde/window/types.hpp"
#include "ldde/window/window.hpp"
#include "ldde/window/window_registry.hpp"
#include "ldde/window/window_stacking.hpp"
#include "ldde/shell/shell.hpp"
#include "ldde/input/touch_interaction_policy.hpp"

namespace ldde::input {

enum class HitTargetType {
    None = 0,
    Shell,
    CloseControl,
    MaximizeControl,
    MinimizeControl,
    TitleBar,
    ResizeEdge,
    WindowContent
};

[[nodiscard]] std::string_view hit_target_type_name(HitTargetType type) noexcept;

struct HitTestResult {
    HitTargetType type = HitTargetType::None;
    std::optional<window::WindowId> window_id;
    window::ResizeEdge resize_edge = window::ResizeEdge::None;
    core::Point window_local_pos{0, 0};
    core::Rect target_rect{0, 0, 0, 0};
};

class TouchHitTesting {
public:
    TouchHitTesting(
        const window::WindowRegistry& registry,
        const window::WindowStacking& stacking,
        const TouchInteractionPolicy& policy,
        const shell::Shell* shell = nullptr);

    [[nodiscard]] HitTestResult hit_test(const core::Point& screen_point) const;

    [[nodiscard]] HitTestResult hit_test_window(
        const window::Window& window,
        const core::Point& screen_point) const;

    [[nodiscard]] window::ResizeEdge detect_resize_edge(
        const core::Rect& geom,
        const core::Point& pt,
        int32_t margin) const noexcept;

    void set_shell(const shell::Shell* shell) noexcept { shell_ = shell; }

private:
    const window::WindowRegistry& registry_;
    const window::WindowStacking& stacking_;
    const TouchInteractionPolicy& policy_;
    const shell::Shell* shell_ = nullptr;
};

} // namespace ldde::input
