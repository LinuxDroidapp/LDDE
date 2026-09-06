#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include "ldde/core/types.hpp"
#include "ldde/display/display_policy.hpp"
#include "ldde/settings/settings_types.hpp"
#include "ldde/settings/settings_schema.hpp"
#include "ldde/settings/settings_navigation.hpp"

namespace ldde::settings {

enum class SettingsHitType {
    None,
    Titlebar,
    CloseButton,
    MinimizeButton,
    MaximizeButton,
    BackButton,
    SearchBar,
    SearchClear,
    CategoryItem,
    SettingToggle,
    SettingSlider,
    SettingRadioOption,
    SettingAction,
    ContentArea
};

struct SettingsHitResult {
    SettingsHitType type = SettingsHitType::None;
    SettingsCategory category = SettingsCategory::Appearance;
    std::string setting_key;
    size_t option_index = 0;
    double slider_fraction = 0.0;
    core::Rect rect{0, 0, 0, 0};
};

struct SettingRowGeometry {
    std::string key;
    core::Rect row_rect{0, 0, 0, 0};
    core::Rect control_rect{0, 0, 0, 0};
    std::vector<core::Rect> option_rects; // For Enum radio segments
};

class SettingsLayout {
public:
    SettingsLayout();
    ~SettingsLayout() = default;

    void update(const display::DisplayPolicy& policy,
                const SettingsNavigation& navigation,
                const std::vector<const SettingDefinition*>& visible_settings,
                bool is_maximized = false);

    [[nodiscard]] bool is_portrait() const noexcept { return is_portrait_; }
    [[nodiscard]] bool is_split_view() const noexcept { return !is_portrait_; }

    [[nodiscard]] const core::Rect& window_rect() const noexcept { return window_rect_; }
    [[nodiscard]] const core::Rect& titlebar_rect() const noexcept { return titlebar_rect_; }
    [[nodiscard]] const core::Rect& close_button_rect() const noexcept { return close_btn_rect_; }
    [[nodiscard]] const core::Rect& minimize_button_rect() const noexcept { return min_btn_rect_; }
    [[nodiscard]] const core::Rect& maximize_button_rect() const noexcept { return max_btn_rect_; }
    [[nodiscard]] const core::Rect& back_button_rect() const noexcept { return back_btn_rect_; }
    [[nodiscard]] const core::Rect& search_bar_rect() const noexcept { return search_bar_rect_; }
    [[nodiscard]] const core::Rect& search_clear_rect() const noexcept { return search_clear_rect_; }
    [[nodiscard]] const core::Rect& sidebar_rect() const noexcept { return sidebar_rect_; }
    [[nodiscard]] const core::Rect& content_rect() const noexcept { return content_rect_; }

    [[nodiscard]] const std::vector<std::pair<SettingsCategory, core::Rect>>& category_rects() const noexcept {
        return category_rects_;
    }
    [[nodiscard]] const std::vector<SettingRowGeometry>& setting_rows() const noexcept {
        return setting_rows_;
    }

    [[nodiscard]] SettingsHitResult hit_test(int32_t x, int32_t y) const;

    // Scrolling
    [[nodiscard]] int32_t scroll_y() const noexcept { return scroll_y_; }
    [[nodiscard]] int32_t max_scroll_y() const noexcept { return max_scroll_y_; }
    void set_scroll_y(int32_t y) noexcept;
    void scroll_by(int32_t dy) noexcept;
    void reset_scroll() noexcept { scroll_y_ = 0; }

    void set_window_geometry(const core::Rect& geom) { window_rect_ = geom; }

private:
    bool is_portrait_ = true;
    bool is_maximized_ = false;

    core::Rect window_rect_{0, 0, 480, 800};
    core::Rect titlebar_rect_{0, 0, 480, 48};
    core::Rect close_btn_rect_{0, 0, 0, 0};
    core::Rect min_btn_rect_{0, 0, 0, 0};
    core::Rect max_btn_rect_{0, 0, 0, 0};
    core::Rect back_btn_rect_{0, 0, 0, 0};
    core::Rect search_bar_rect_{0, 0, 0, 0};
    core::Rect search_clear_rect_{0, 0, 0, 0};
    core::Rect sidebar_rect_{0, 0, 0, 0};
    core::Rect content_rect_{0, 0, 480, 752};

    std::vector<std::pair<SettingsCategory, core::Rect>> category_rects_;
    std::vector<SettingRowGeometry> setting_rows_;

    int32_t scroll_y_ = 0;
    int32_t max_scroll_y_ = 0;
};

} // namespace ldde::settings
