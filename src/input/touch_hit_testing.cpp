#include "ldde/input/touch_hit_testing.hpp"

namespace ldde::input {

std::string_view hit_target_type_name(HitTargetType type) noexcept {
    switch (type) {
        case HitTargetType::None:            return "None";
        case HitTargetType::Shell:           return "Shell";
        case HitTargetType::CloseControl:    return "CloseControl";
        case HitTargetType::MaximizeControl: return "MaximizeControl";
        case HitTargetType::MinimizeControl: return "MinimizeControl";
        case HitTargetType::TitleBar:        return "TitleBar";
        case HitTargetType::ResizeEdge:      return "ResizeEdge";
        case HitTargetType::WindowContent:   return "WindowContent";
    }
    return "Unknown";
}

TouchHitTesting::TouchHitTesting(
    const window::WindowRegistry& registry,
    const window::WindowStacking& stacking,
    const TouchInteractionPolicy& policy,
    const shell::Shell* shell)
    : registry_(registry), stacking_(stacking), policy_(policy), shell_(shell) {}

window::ResizeEdge TouchHitTesting::detect_resize_edge(
    const core::Rect& geom,
    const core::Point& pt,
    int32_t margin) const noexcept {

    // Must be near the window border
    if (pt.x < geom.x - margin || pt.x > geom.x + geom.width + margin ||
        pt.y < geom.y - margin || pt.y > geom.y + geom.height + margin) {
        return window::ResizeEdge::None;
    }

    bool on_left = (pt.x <= geom.x + margin);
    bool on_right = (pt.x >= geom.x + geom.width - margin);
    bool on_top_corner = (pt.y <= geom.y + margin);
    bool on_top_edge = (pt.y <= geom.y + 6);
    bool on_bottom = (pt.y >= geom.y + geom.height - margin);

    if (on_top_corner && on_left) return window::ResizeEdge::TopLeft;
    if (on_top_corner && on_right) return window::ResizeEdge::TopRight;
    if (on_bottom && on_left) return window::ResizeEdge::BottomLeft;
    if (on_bottom && on_right) return window::ResizeEdge::BottomRight;

    if (on_top_edge) return window::ResizeEdge::Top;
    if (on_bottom) return window::ResizeEdge::Bottom;
    if (on_left) return window::ResizeEdge::Left;
    if (on_right) return window::ResizeEdge::Right;

    return window::ResizeEdge::None;
}

HitTestResult TouchHitTesting::hit_test_window(
    const window::Window& window,
    const core::Point& screen_point) const {

    HitTestResult res;
    res.window_id = window.id();
    const core::Rect& geom = window.geometry();
    res.window_local_pos = core::Point{screen_point.x - geom.x, screen_point.y - geom.y};

    // Fullscreen: window controls and resize are suppressed
    if (window.state() == window::WindowState::Fullscreen) {
        if (geom.contains(screen_point)) {
            res.type = HitTargetType::WindowContent;
            res.target_rect = geom;
            return res;
        }
        return res;
    }

    int32_t header_h = policy_.header_touch_height_px;
    int32_t btn_w = policy_.control_touch_target_px;

    // 1. Controls (highest priority within window)
    core::Rect close_rect{geom.x + geom.width - btn_w, geom.y, btn_w, header_h};
    if (close_rect.contains(screen_point)) {
        res.type = HitTargetType::CloseControl;
        res.target_rect = close_rect;
        return res;
    }

    core::Rect max_rect{geom.x + geom.width - (2 * btn_w), geom.y, btn_w, header_h};
    if (max_rect.contains(screen_point)) {
        res.type = HitTargetType::MaximizeControl;
        res.target_rect = max_rect;
        return res;
    }

    core::Rect min_rect{geom.x + geom.width - (3 * btn_w), geom.y, btn_w, header_h};
    if (min_rect.contains(screen_point)) {
        res.type = HitTargetType::MinimizeControl;
        res.target_rect = min_rect;
        return res;
    }

    // 2. Resize edges (only if not maximized)
    if (window.state() != window::WindowState::Maximized) {
        window::ResizeEdge edge = detect_resize_edge(geom, screen_point, policy_.resize_touch_target_px);
        if (edge != window::ResizeEdge::None) {
            res.type = HitTargetType::ResizeEdge;
            res.resize_edge = edge;
            return res;
        }
    }

    // 3. Title bar
    core::Rect header_rect{geom.x, geom.y, geom.width, header_h};
    if (header_rect.contains(screen_point)) {
        res.type = HitTargetType::TitleBar;
        res.target_rect = header_rect;
        return res;
    }

    // 4. Window content
    if (geom.contains(screen_point)) {
        res.type = HitTargetType::WindowContent;
        res.target_rect = geom;
        return res;
    }

    return res;
}

HitTestResult TouchHitTesting::hit_test(const core::Point& screen_point) const {
    // 1. Priority: System/Shell overlay
    if (shell_ && shell_->is_ready()) {
        auto region = shell_->layout().hit_test(screen_point, shell_->overlay().is_active());
        if (region == shell::ShellRegionType::Status ||
            region == shell::ShellRegionType::Dock ||
            region == shell::ShellRegionType::Overlay) {
            HitTestResult res;
            res.type = HitTargetType::Shell;
            return res;
        }
    }

    // 2. Priority: Topmost window first
    const auto& visible = stacking_.visible_stack(registry_);
    for (auto it = visible.rbegin(); it != visible.rend(); ++it) {
        auto win = registry_.lookup(*it);
        if (!win || !win->is_visible() || win->state() == window::WindowState::Minimized) {
            continue;
        }

        HitTestResult res = hit_test_window(*win, screen_point);
        if (res.type != HitTargetType::None) {
            return res;
        }
    }

    return HitTestResult{};
}

} // namespace ldde::input
