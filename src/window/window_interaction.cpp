#include "ldde/window/window_interaction.hpp"
#include <algorithm>

namespace ldde::window {

WindowInteraction::WindowInteraction(InteractionConfig config)
    : config_(config) {}

ResizeEdge WindowInteraction::detect_resize_edge(
    const core::Rect& geom,
    const core::Point& pt,
    bool is_touch) const noexcept {

    int32_t margin = is_touch ? config_.touch_edge_margin : config_.pointer_edge_margin;

    // Must be near the window border
    if (pt.x < geom.x - margin || pt.x > geom.x + geom.width + margin ||
        pt.y < geom.y - margin || pt.y > geom.y + geom.height + margin) {
        return ResizeEdge::None;
    }

    bool on_left = (pt.x <= geom.x + margin);
    bool on_right = (pt.x >= geom.x + geom.width - margin);
    bool on_top = (pt.y <= geom.y + margin);
    bool on_bottom = (pt.y >= geom.y + geom.height - margin);

    if (on_top && on_left) return ResizeEdge::TopLeft;
    if (on_top && on_right) return ResizeEdge::TopRight;
    if (on_bottom && on_left) return ResizeEdge::BottomLeft;
    if (on_bottom && on_right) return ResizeEdge::BottomRight;

    if (on_top) return ResizeEdge::Top;
    if (on_bottom) return ResizeEdge::Bottom;
    if (on_left) return ResizeEdge::Left;
    if (on_right) return ResizeEdge::Right;

    return ResizeEdge::None;
}

bool WindowInteraction::start_move(
    WindowId id,
    const core::Point& start_pos,
    const core::Rect& initial_geom,
    const core::Rect& usable_area,
    bool is_touch) {

    if (id == kInvalidWindowId) return false;

    type_ = InteractionType::Moving;
    window_id_ = id;
    resize_edge_ = ResizeEdge::None;
    start_point_ = start_pos;
    initial_geom_ = initial_geom;
    current_geom_ = initial_geom;
    usable_area_ = usable_area;
    is_touch_ = is_touch;
    return true;
}

core::Rect WindowInteraction::clamp_moved_geometry(const core::Rect& geom) const noexcept {
    core::Rect clamped = geom;

    // Top titlebar edge must not go above usable_area.y
    if (clamped.y < usable_area_.y) {
        clamped.y = usable_area_.y;
    }
    // Bottom of titlebar must be at least min_visible_titlebar inside usable area
    int32_t max_y = usable_area_.y + usable_area_.height - config_.min_visible_titlebar;
    if (clamped.y > max_y) {
        clamped.y = max_y;
    }

    // Horizontally keep at least 48px visible inside usable area
    int32_t min_x = usable_area_.x - clamped.width + 48;
    int32_t max_x = usable_area_.x + usable_area_.width - 48;
    if (clamped.x < min_x) clamped.x = min_x;
    if (clamped.x > max_x) clamped.x = max_x;

    return clamped;
}

core::Rect WindowInteraction::update_move(const core::Point& current_pos) noexcept {
    if (type_ != InteractionType::Moving) return current_geom_;

    int32_t dx = current_pos.x - start_point_.x;
    int32_t dy = current_pos.y - start_point_.y;

    core::Rect candidate{
        initial_geom_.x + dx,
        initial_geom_.y + dy,
        initial_geom_.width,
        initial_geom_.height
    };

    current_geom_ = clamp_moved_geometry(candidate);
    return current_geom_;
}

core::Rect WindowInteraction::end_move() noexcept {
    core::Rect final_geom = current_geom_;
    reset();
    return final_geom;
}

core::Rect WindowInteraction::cancel_move() noexcept {
    core::Rect initial = initial_geom_;
    reset();
    return initial;
}

bool WindowInteraction::start_resize(
    WindowId id,
    ResizeEdge edge,
    const core::Point& start_pos,
    const core::Rect& initial_geom,
    const core::Rect& usable_area,
    const core::Size& min_size,
    const core::Size& max_size,
    bool is_touch) {

    if (id == kInvalidWindowId || edge == ResizeEdge::None) return false;

    type_ = InteractionType::Resizing;
    window_id_ = id;
    resize_edge_ = edge;
    start_point_ = start_pos;
    initial_geom_ = initial_geom;
    current_geom_ = initial_geom;
    usable_area_ = usable_area;
    min_size_ = min_size;
    max_size_ = max_size;
    is_touch_ = is_touch;
    return true;
}

core::Rect WindowInteraction::apply_resize_delta(int32_t dx, int32_t dy) const noexcept {
    int32_t new_x = initial_geom_.x;
    int32_t new_y = initial_geom_.y;
    int32_t new_w = initial_geom_.width;
    int32_t new_h = initial_geom_.height;

    int32_t min_w = std::max(100, min_size_.width);
    int32_t min_h = std::max(100, min_size_.height);

    // Horizontal adjustments
    switch (resize_edge_) {
        case ResizeEdge::Left:
        case ResizeEdge::TopLeft:
        case ResizeEdge::BottomLeft: {
            new_w = initial_geom_.width - dx;
            if (new_w < min_w) new_w = min_w;
            if (max_size_.width > 0 && new_w > max_size_.width) new_w = max_size_.width;
            if (new_w > usable_area_.width) new_w = usable_area_.width;
            new_x = initial_geom_.x + (initial_geom_.width - new_w);
            break;
        }
        case ResizeEdge::Right:
        case ResizeEdge::TopRight:
        case ResizeEdge::BottomRight: {
            new_w = initial_geom_.width + dx;
            if (new_w < min_w) new_w = min_w;
            if (max_size_.width > 0 && new_w > max_size_.width) new_w = max_size_.width;
            if (new_w > usable_area_.width) new_w = usable_area_.width;
            break;
        }
        default: break;
    }

    // Vertical adjustments
    switch (resize_edge_) {
        case ResizeEdge::Top:
        case ResizeEdge::TopLeft:
        case ResizeEdge::TopRight: {
            new_h = initial_geom_.height - dy;
            if (new_h < min_h) new_h = min_h;
            if (max_size_.height > 0 && new_h > max_size_.height) new_h = max_size_.height;
            if (new_h > usable_area_.height) new_h = usable_area_.height;
            new_y = initial_geom_.y + (initial_geom_.height - new_h);
            break;
        }
        case ResizeEdge::Bottom:
        case ResizeEdge::BottomLeft:
        case ResizeEdge::BottomRight: {
            new_h = initial_geom_.height + dy;
            if (new_h < min_h) new_h = min_h;
            if (max_size_.height > 0 && new_h > max_size_.height) new_h = max_size_.height;
            if (new_h > usable_area_.height) new_h = usable_area_.height;
            break;
        }
        default: break;
    }

    // Enforce top border constraint
    if (new_y < usable_area_.y) {
        new_h -= (usable_area_.y - new_y);
        new_y = usable_area_.y;
        if (new_h < min_h) new_h = min_h;
    }

    return core::Rect{new_x, new_y, new_w, new_h};
}

core::Rect WindowInteraction::update_resize(const core::Point& current_pos) noexcept {
    if (type_ != InteractionType::Resizing) return current_geom_;

    int32_t dx = current_pos.x - start_point_.x;
    int32_t dy = current_pos.y - start_point_.y;

    current_geom_ = apply_resize_delta(dx, dy);
    return current_geom_;
}

core::Rect WindowInteraction::end_resize() noexcept {
    core::Rect final_geom = current_geom_;
    reset();
    return final_geom;
}

core::Rect WindowInteraction::cancel_resize() noexcept {
    core::Rect initial = initial_geom_;
    reset();
    return initial;
}

void WindowInteraction::reset() noexcept {
    type_ = InteractionType::None;
    window_id_ = std::nullopt;
    resize_edge_ = ResizeEdge::None;
    start_point_ = {0, 0};
    initial_geom_ = {0, 0, 0, 0};
    current_geom_ = {0, 0, 0, 0};
    usable_area_ = {0, 0, 0, 0};
    is_touch_ = false;
}

} // namespace ldde::window

