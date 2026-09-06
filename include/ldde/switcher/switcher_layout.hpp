#pragma once

#include <vector>
#include <optional>
#include "ldde/core/types.hpp"
#include "ldde/display/display_policy.hpp"

namespace ldde::switcher {

class SwitcherLayout {
public:
    SwitcherLayout() = default;
    ~SwitcherLayout() = default;

    void update(const display::DisplayPolicy& policy, size_t item_count);

    [[nodiscard]] const core::Rect& overlay_bounds() const noexcept { return overlay_bounds_; }
    [[nodiscard]] const core::Rect& header_rect() const noexcept { return header_rect_; }
    [[nodiscard]] const core::Rect& content_rect() const noexcept { return content_rect_; }
    [[nodiscard]] const std::vector<core::Rect>& item_rects() const noexcept { return item_rects_; }
    [[nodiscard]] const core::Rect* rect_at(size_t index) const noexcept;

    [[nodiscard]] int32_t scroll_offset() const noexcept { return scroll_offset_; }
    [[nodiscard]] int32_t max_scroll_offset() const noexcept { return max_scroll_offset_; }
    void set_scroll_offset(int32_t offset) noexcept;
    void scroll_by(int32_t delta) noexcept;

    [[nodiscard]] std::optional<size_t> hit_test(const core::Point& p) const noexcept;
    [[nodiscard]] bool hit_test_backdrop(const core::Point& p) const noexcept;

    [[nodiscard]] bool is_horizontal() const noexcept { return is_horizontal_; }
    [[nodiscard]] int32_t card_width() const noexcept { return card_width_; }
    [[nodiscard]] int32_t card_height() const noexcept { return card_height_; }

private:
    core::Rect overlay_bounds_{0, 0, 0, 0};
    core::Rect header_rect_{0, 0, 0, 0};
    core::Rect content_rect_{0, 0, 0, 0};
    std::vector<core::Rect> item_rects_;

    int32_t card_width_ = 0;
    int32_t card_height_ = 0;
    int32_t spacing_ = 12;
    bool is_horizontal_ = false;

    int32_t scroll_offset_ = 0;
    int32_t max_scroll_offset_ = 0;
};

} // namespace ldde::switcher
