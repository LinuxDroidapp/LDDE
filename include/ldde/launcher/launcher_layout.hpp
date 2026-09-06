#pragma once

#include <cstdint>
#include <vector>
#include "ldde/core/types.hpp"
#include "ldde/display/display_policy.hpp"

namespace ldde::launcher {

enum class LauncherHitAreaType {
    None = 0,
    DismissScrim,
    SearchBar,
    ClearSearchButton,
    CategoryChip,
    GridItem
};

struct LauncherHitTestResult {
    LauncherHitAreaType type = LauncherHitAreaType::None;
    size_t index = 0;
};

class LauncherLayout {
public:
    LauncherLayout() = default;

    void update(
        const display::DisplayPolicy& policy,
        int item_count,
        int category_count,
        int min_item_width = 80);

    // Geometry queries
    [[nodiscard]] const core::Rect& container_rect() const noexcept { return container_rect_; }
    [[nodiscard]] const core::Rect& search_bar_rect() const noexcept { return search_bar_rect_; }
    [[nodiscard]] const core::Rect& clear_button_rect() const noexcept { return clear_button_rect_; }
    [[nodiscard]] const core::Rect& category_bar_rect() const noexcept { return category_bar_rect_; }
    [[nodiscard]] const core::Rect& grid_rect() const noexcept { return grid_rect_; }

    [[nodiscard]] int columns() const noexcept { return columns_; }
    [[nodiscard]] int item_width() const noexcept { return item_width_; }
    [[nodiscard]] int item_height() const noexcept { return item_height_; }
    [[nodiscard]] int spacing() const noexcept { return spacing_; }
    [[nodiscard]] int total_content_height() const noexcept { return total_content_height_; }
    [[nodiscard]] int max_scroll_y() const noexcept { return max_scroll_y_; }

    [[nodiscard]] core::Rect item_rect(size_t index, int scroll_y = 0) const noexcept;
    [[nodiscard]] core::Rect category_chip_rect(size_t index) const noexcept;

    [[nodiscard]] LauncherHitTestResult hit_test(
        core::Point p,
        int scroll_y,
        size_t item_count,
        size_t category_count) const noexcept;

private:
    core::Rect container_rect_;
    core::Rect search_bar_rect_;
    core::Rect clear_button_rect_;
    core::Rect category_bar_rect_;
    core::Rect grid_rect_;

    int columns_ = 4;
    int item_width_ = 80;
    int item_height_ = 96;
    int spacing_ = 12;
    int total_content_height_ = 0;
    int max_scroll_y_ = 0;

    std::vector<core::Rect> category_rects_;
};

} // namespace ldde::launcher

