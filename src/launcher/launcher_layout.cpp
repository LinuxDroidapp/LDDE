#include "ldde/launcher/launcher_layout.hpp"
#include <algorithm>
#include <cmath>

namespace ldde::launcher {

void LauncherLayout::update(
    const display::DisplayPolicy& policy,
    int item_count,
    int category_count,
    int min_item_width) {
    const auto& info = policy.display_info();
    int disp_w = info.logical_width;
    int disp_h = info.logical_height;
    if (disp_w <= 0 || disp_h <= 0) return;

    const auto& metrics = policy.metrics();
    int margin = std::max(12, metrics.content_margin_px);
    spacing_ = std::max(8, metrics.shell_spacing_px);
    int touch_min = std::max(48, metrics.minimum_touch_target_px);

    // 1. Container rect
    if (policy.layout_class() == display::LayoutClass::Compact) {
        container_rect_ = policy.available_geometry().window_bounds;
    } else {
        int cw = std::min(disp_w - 48, 720);
        int ch = std::min(disp_h - 48, 640);
        int cx = (disp_w - cw) / 2;
        int cy = (disp_h - ch) / 2;
        container_rect_ = {cx, cy, cw, ch};
    }

    // 2. Search bar
    int inner_x = container_rect_.x + margin;
    int inner_y = container_rect_.y + margin;
    int inner_w = std::max(1, container_rect_.width - 2 * margin);

    int search_h = touch_min;
    search_bar_rect_ = {inner_x, inner_y, inner_w, search_h};

    int clear_btn_size = search_h - 8;
    clear_button_rect_ = {
        search_bar_rect_.x + search_bar_rect_.width - clear_btn_size - 4,
        search_bar_rect_.y + 4,
        clear_btn_size,
        clear_btn_size
    };

    // 3. Category bar
    int cat_y = search_bar_rect_.y + search_bar_rect_.height + spacing_;
    int cat_h = std::max(40, touch_min - 4);
    category_bar_rect_ = {inner_x, cat_y, inner_w, cat_h};

    category_rects_.clear();
    category_rects_.reserve(category_count);
    int cur_cat_x = category_bar_rect_.x;
    for (int i = 0; i < category_count; ++i) {
        int chip_w = 84; // responsive base chip width
        int chip_h = cat_h - 6;
        int chip_y = category_bar_rect_.y + 3;
        category_rects_.push_back(core::Rect{cur_cat_x, chip_y, chip_w, chip_h});
        cur_cat_x += chip_w + 8;
    }

    // 4. Grid rect
    int grid_y = category_bar_rect_.y + category_bar_rect_.height + spacing_;
    int grid_h = std::max(0, (container_rect_.y + container_rect_.height - margin) - grid_y);
    grid_rect_ = {inner_x, grid_y, inner_w, grid_h};

    // 5. Grid columns & items
    int min_w = std::max(64, min_item_width);
    columns_ = std::max(1, (grid_rect_.width + spacing_) / (min_w + spacing_));
    item_width_ = (grid_rect_.width - spacing_ * (columns_ - 1)) / columns_;
    item_height_ = std::max(item_width_, 88);

    int total_rows = (item_count <= 0) ? 0 : ((item_count + columns_ - 1) / columns_);
    total_content_height_ = total_rows * item_height_ + std::max(0, total_rows - 1) * spacing_;
    max_scroll_y_ = std::max(0, total_content_height_ - grid_rect_.height);
}

core::Rect LauncherLayout::item_rect(size_t index, int scroll_y) const noexcept {
    if (columns_ <= 0) return {0, 0, 0, 0};

    int row = static_cast<int>(index) / columns_;
    int col = static_cast<int>(index) % columns_;

    int x = grid_rect_.x + col * (item_width_ + spacing_);
    int y = grid_rect_.y + row * (item_height_ + spacing_) - scroll_y;

    return {x, y, item_width_, item_height_};
}

core::Rect LauncherLayout::category_chip_rect(size_t index) const noexcept {
    if (index >= category_rects_.size()) {
        return {0, 0, 0, 0};
    }
    return category_rects_[index];
}

LauncherHitTestResult LauncherLayout::hit_test(
    core::Point p,
    int scroll_y,
    size_t item_count,
    size_t category_count) const noexcept {
    if (!container_rect_.contains(p)) {
        return {LauncherHitAreaType::DismissScrim, 0};
    }

    if (search_bar_rect_.contains(p)) {
        if (clear_button_rect_.contains(p)) {
            return {LauncherHitAreaType::ClearSearchButton, 0};
        }
        return {LauncherHitAreaType::SearchBar, 0};
    }

    if (category_bar_rect_.contains(p)) {
        for (size_t i = 0; i < category_count && i < category_rects_.size(); ++i) {
            if (category_rects_[i].contains(p)) {
                return {LauncherHitAreaType::CategoryChip, i};
            }
        }
    }

    if (grid_rect_.contains(p)) {
        int rel_x = p.x - grid_rect_.x;
        int rel_y = p.y - grid_rect_.y + scroll_y;

        int col = rel_x / (item_width_ + spacing_);
        int row = rel_y / (item_height_ + spacing_);

        if (col >= 0 && col < columns_ && row >= 0) {
            size_t idx = static_cast<size_t>(row * columns_ + col);
            if (idx < item_count) {
                core::Rect r = item_rect(idx, scroll_y);
                if (r.contains(p)) {
                    return {LauncherHitAreaType::GridItem, idx};
                }
            }
        }
    }

    return {LauncherHitAreaType::None, 0};
}

} // namespace ldde::launcher
