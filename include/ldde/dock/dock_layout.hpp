#pragma once

#include <vector>
#include <cstdint>
#include <algorithm>
#include "ldde/core/types.hpp"
#include "ldde/display/display_policy.hpp"
#include "ldde/shell/design_tokens.hpp"
#include "ldde/dock/dock_item.hpp"

namespace ldde::dock {

enum class DockHitType {
    None = 0,
    LauncherButton,
    Item,
    Background
};

struct DockHitResult {
    DockHitType type = DockHitType::None;
    int32_t item_index = -1;
};

class DockLayout {
public:
    DockLayout() = default;

    void update(const display::DisplayPolicy& policy,
                const shell::DesignTokens& tokens,
                const core::Rect& dock_geometry,
                size_t item_count,
                int32_t custom_item_size = 0,
                int32_t custom_spacing = 0);

    [[nodiscard]] const core::Rect& dock_geometry() const noexcept { return dock_geometry_; }
    [[nodiscard]] const core::Rect& launcher_button_rect() const noexcept { return launcher_button_rect_; }
    [[nodiscard]] const std::vector<core::Rect>& item_rects() const noexcept { return item_rects_; }
    [[nodiscard]] int32_t item_size() const noexcept { return item_size_; }
    [[nodiscard]] int32_t item_spacing() const noexcept { return item_spacing_; }
    [[nodiscard]] int32_t scroll_offset_x() const noexcept { return scroll_offset_x_; }
    [[nodiscard]] int32_t max_scroll_x() const noexcept { return max_scroll_x_; }
    [[nodiscard]] bool has_overflow() const noexcept { return max_scroll_x_ > 0; }

    void set_scroll_offset_x(int32_t offset) noexcept {
        scroll_offset_x_ = std::clamp(offset, 0, max_scroll_x_);
    }

    void scroll_by(int32_t delta) noexcept {
        set_scroll_offset_x(scroll_offset_x_ + delta);
    }

    [[nodiscard]] DockHitResult hit_test(int32_t local_x, int32_t local_y) const noexcept;
    [[nodiscard]] core::Rect visible_item_rect(size_t index) const noexcept;

private:
    core::Rect dock_geometry_{0, 0, 0, 0};
    core::Rect launcher_button_rect_{0, 0, 0, 0};
    std::vector<core::Rect> item_rects_;

    int32_t item_size_ = 48;
    int32_t item_spacing_ = 8;
    int32_t padding_h_ = 12;
    int32_t padding_v_ = 8;
    int32_t separator_w_ = 8;

    int32_t total_content_width_ = 0;
    int32_t scroll_offset_x_ = 0;
    int32_t max_scroll_x_ = 0;
};

} // namespace ldde::dock
