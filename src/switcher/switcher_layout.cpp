#include "ldde/switcher/switcher_layout.hpp"
#include <algorithm>

namespace ldde::switcher {

void SwitcherLayout::update(const display::DisplayPolicy& policy, size_t item_count) {
    const auto& geom = policy.display_info().geometry;
    const auto& info = policy.display_info();
    int32_t disp_w = info.logical_width > 0 ? info.logical_width : (geom.width > 0 ? geom.width : 720);
    int32_t disp_h = info.logical_height > 0 ? info.logical_height : (geom.height > 0 ? geom.height : 1280);
    overlay_bounds_ = {0, 0, disp_w, disp_h};

    const auto& safe_bounds = policy.available_geometry().safe_bounds;
    int32_t safe_x = safe_bounds.width > 0 ? safe_bounds.x : 0;
    int32_t safe_y = safe_bounds.height > 0 ? safe_bounds.y : 0;
    int32_t safe_w = safe_bounds.width > 0 ? safe_bounds.width : overlay_bounds_.width;
    int32_t safe_h = safe_bounds.height > 0 ? safe_bounds.height : overlay_bounds_.height;

    double scale = policy.scale_policy().effective_scale();
    if (scale <= 0.0) scale = 1.0;

    int32_t touch_min = std::max(48, static_cast<int32_t>(policy.metrics().minimum_touch_target_px));

    int32_t header_h = std::max(56, static_cast<int32_t>(56 * scale));
    header_rect_ = {safe_x, safe_y, safe_w, header_h};

    int32_t margin_x = static_cast<int32_t>(16 * scale);
    int32_t margin_y = static_cast<int32_t>(8 * scale);
    content_rect_ = {
        safe_x + margin_x,
        safe_y + header_h + margin_y,
        std::max(10, safe_w - 2 * margin_x),
        std::max(10, safe_h - header_h - 2 * margin_y - static_cast<int32_t>(16 * scale))
    };

    is_horizontal_ = policy.is_landscape() || policy.layout_class() == display::LayoutClass::Expanded;
    item_rects_.clear();
    item_rects_.reserve(item_count);

    if (!is_horizontal_) {
        // Vertical list for phone portrait
        card_width_ = content_rect_.width;
        card_height_ = std::max(touch_min, static_cast<int32_t>(76 * scale));
        spacing_ = std::max(8, static_cast<int32_t>(12 * scale));

        for (size_t i = 0; i < item_count; ++i) {
            int32_t y = content_rect_.y + static_cast<int32_t>(i) * (card_height_ + spacing_);
            item_rects_.push_back({content_rect_.x, y, card_width_, card_height_});
        }

        int32_t total_h = item_count > 0 ? (static_cast<int32_t>(item_count) * (card_height_ + spacing_) - spacing_) : 0;
        max_scroll_offset_ = std::max(0, total_h - content_rect_.height);
    } else {
        // Horizontal row for landscape / tablet
        card_width_ = std::max(touch_min, static_cast<int32_t>(200 * scale));
        card_height_ = std::max(touch_min, std::min(content_rect_.height, static_cast<int32_t>(160 * scale)));
        spacing_ = std::max(8, static_cast<int32_t>(16 * scale));

        int32_t total_w = item_count > 0 ? (static_cast<int32_t>(item_count) * (card_width_ + spacing_) - spacing_) : 0;
        int32_t start_x = content_rect_.x;
        if (total_w < content_rect_.width) {
            start_x = content_rect_.x + (content_rect_.width - total_w) / 2;
        }

        int32_t card_y = content_rect_.y + (content_rect_.height - card_height_) / 2;

        for (size_t i = 0; i < item_count; ++i) {
            int32_t x = start_x + static_cast<int32_t>(i) * (card_width_ + spacing_);
            item_rects_.push_back({x, card_y, card_width_, card_height_});
        }

        max_scroll_offset_ = std::max(0, total_w - content_rect_.width);
    }

    set_scroll_offset(scroll_offset_);
}

const core::Rect* SwitcherLayout::rect_at(size_t index) const noexcept {
    if (index >= item_rects_.size()) return nullptr;
    return &item_rects_[index];
}

void SwitcherLayout::set_scroll_offset(int32_t offset) noexcept {
    scroll_offset_ = std::clamp(offset, 0, max_scroll_offset_);
}

void SwitcherLayout::scroll_by(int32_t delta) noexcept {
    set_scroll_offset(scroll_offset_ + delta);
}

std::optional<size_t> SwitcherLayout::hit_test(const core::Point& p) const noexcept {
    if (!content_rect_.contains(p)) {
        return std::nullopt;
    }

    for (size_t i = 0; i < item_rects_.size(); ++i) {
        core::Rect r = item_rects_[i];
        if (!is_horizontal_) {
            r.y -= scroll_offset_;
        } else {
            r.x -= scroll_offset_;
        }
        if (r.contains(p)) {
            return i;
        }
    }
    return std::nullopt;
}

bool SwitcherLayout::hit_test_backdrop(const core::Point& p) const noexcept {
    if (!overlay_bounds_.contains(p)) {
        return false;
    }
    return !hit_test(p).has_value();
}

} // namespace ldde::switcher
