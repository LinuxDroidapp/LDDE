#include "ldde/settings/settings_layout.hpp"
#include <algorithm>

namespace ldde::settings {

SettingsLayout::SettingsLayout() = default;

void SettingsLayout::update(const display::DisplayPolicy& policy,
                            const SettingsNavigation& navigation,
                            const std::vector<const SettingDefinition*>& visible_settings,
                            bool is_maximized) {
    is_maximized_ = is_maximized;
    const auto& info = policy.display_info();
    const auto& geom = info.geometry;
    is_portrait_ = policy.is_portrait();
    const auto& insets = info.safe_insets;

    // 1. Calculate window geometry
    if (is_portrait_ || is_maximized_) {
        // Mobile portrait: fill work area beneath status bar, above dock
        int32_t top = std::max(40, insets.top);
        int32_t bottom_margin = std::max(76, insets.bottom);
        int32_t w = geom.width - (insets.left + insets.right);
        int32_t h = geom.height - top - bottom_margin;
        window_rect_ = core::Rect{insets.left, top, std::max(320, w), std::max(400, h)};
    } else {
        // Landscape / Tablet window
        int32_t target_w = std::min(760, geom.width - 64);
        int32_t target_h = std::min(580, geom.height - 100);
        int32_t win_x = (geom.width - target_w) / 2;
        int32_t win_y = (geom.height - target_h) / 2;
        window_rect_ = core::Rect{win_x, win_y, target_w, target_h};
    }

    // 2. Titlebar & Window controls (48dp height)
    titlebar_rect_ = core::Rect{window_rect_.x, window_rect_.y, window_rect_.width, 48};

    int32_t btn_size = 40;
    int32_t btn_y = window_rect_.y + (48 - btn_size) / 2;
    int32_t right_x = window_rect_.x + window_rect_.width - 8 - btn_size;

    close_btn_rect_ = core::Rect{right_x, btn_y, btn_size, btn_size};
    right_x -= (btn_size + 4);
    max_btn_rect_ = core::Rect{right_x, btn_y, btn_size, btn_size};
    right_x -= (btn_size + 4);
    min_btn_rect_ = core::Rect{right_x, btn_y, btn_size, btn_size};

    if (is_portrait_ && (navigation.is_in_category_detail() || navigation.is_searching())) {
        back_btn_rect_ = core::Rect{window_rect_.x + 8, btn_y, btn_size, btn_size};
    } else {
        back_btn_rect_ = core::Rect{0, 0, 0, 0};
    }

    // 3. Search Bar
    int32_t search_h = 42;
    int32_t search_y = titlebar_rect_.y + titlebar_rect_.height + 6;
    int32_t search_margin = 12;
    search_bar_rect_ = core::Rect{
        window_rect_.x + search_margin,
        search_y,
        window_rect_.width - (search_margin * 2),
        search_h
    };
    search_clear_rect_ = core::Rect{
        search_bar_rect_.x + search_bar_rect_.width - 36,
        search_bar_rect_.y + (search_h - 28) / 2,
        28,
        28
    };

    int32_t body_top = search_bar_rect_.y + search_bar_rect_.height + 8;
    int32_t body_h = window_rect_.y + window_rect_.height - body_top - 8;

    category_rects_.clear();
    setting_rows_.clear();

    if (!is_portrait_) {
        // Dual-pane split view for Landscape / Tablet
        int32_t sidebar_w = 210;
        sidebar_rect_ = core::Rect{window_rect_.x + 8, body_top, sidebar_w, body_h};
        content_rect_ = core::Rect{window_rect_.x + sidebar_w + 16, body_top, window_rect_.width - sidebar_w - 24, body_h};

        // Sidebar categories
        std::vector<SettingsCategory> cats = {
            SettingsCategory::Appearance, SettingsCategory::Display, SettingsCategory::Windows,
            SettingsCategory::Desktop, SettingsCategory::Dock, SettingsCategory::Launcher,
            SettingsCategory::Input, SettingsCategory::Notifications, SettingsCategory::SystemUI,
            SettingsCategory::About
        };

        int32_t cat_y = sidebar_rect_.y;
        int32_t cat_h = 44;
        for (auto c : cats) {
            category_rects_.push_back({c, core::Rect{sidebar_rect_.x, cat_y, sidebar_rect_.width, cat_h}});
            cat_y += cat_h + 4;
        }

        // Right pane: setting rows
        int32_t row_y = content_rect_.y - scroll_y_;
        for (const auto* def : visible_settings) {
            if (!def) continue;
            int32_t row_h = (def->type == SettingType::Enum || def->type == SettingType::Int || def->type == SettingType::Double) ? 80 : 64;
            core::Rect row_r{content_rect_.x, row_y, content_rect_.width, row_h};

            SettingRowGeometry srg;
            srg.key = def->key;
            srg.row_rect = row_r;

            int32_t ctrl_w = (def->type == SettingType::Enum) ? 220 : 120;
            srg.control_rect = core::Rect{
                row_r.x + row_r.width - ctrl_w - 12,
                row_r.y + (row_h - 36) / 2,
                ctrl_w,
                36
            };

            if (def->type == SettingType::Enum && !def->enum_values.empty()) {
                size_t n = def->enum_values.size();
                int32_t seg_w = ctrl_w / static_cast<int32_t>(n);
                for (size_t i = 0; i < n; ++i) {
                    srg.option_rects.push_back(core::Rect{
                        srg.control_rect.x + static_cast<int32_t>(i) * seg_w,
                        srg.control_rect.y,
                        seg_w,
                        36
                    });
                }
            }

            setting_rows_.push_back(std::move(srg));
            row_y += row_h + 8;
        }

        int32_t total_content_h = (row_y + scroll_y_) - content_rect_.y;
        max_scroll_y_ = std::max(0, total_content_h - content_rect_.height);
    } else {
        // Portrait view
        sidebar_rect_ = core::Rect{0, 0, 0, 0};
        content_rect_ = core::Rect{window_rect_.x + 8, body_top, window_rect_.width - 16, body_h};

        if (!navigation.is_in_category_detail() && !navigation.is_searching()) {
            // Category List view
            std::vector<SettingsCategory> cats = {
                SettingsCategory::Appearance, SettingsCategory::Display, SettingsCategory::Windows,
                SettingsCategory::Desktop, SettingsCategory::Dock, SettingsCategory::Launcher,
                SettingsCategory::Input, SettingsCategory::Notifications, SettingsCategory::SystemUI,
                SettingsCategory::About
            };

            int32_t cat_y = content_rect_.y - scroll_y_;
            int32_t cat_h = 56;
            for (auto c : cats) {
                category_rects_.push_back({c, core::Rect{content_rect_.x, cat_y, content_rect_.width, cat_h}});
                cat_y += cat_h + 8;
            }
            int32_t total_h = (cat_y + scroll_y_) - content_rect_.y;
            max_scroll_y_ = std::max(0, total_h - content_rect_.height);
        } else {
            // Category Detail or Search Results view
            int32_t row_y = content_rect_.y - scroll_y_;
            for (const auto* def : visible_settings) {
                if (!def) continue;
                int32_t row_h = (def->type == SettingType::Enum || def->type == SettingType::Int || def->type == SettingType::Double) ? 84 : 68;
                core::Rect row_r{content_rect_.x, row_y, content_rect_.width, row_h};

                SettingRowGeometry srg;
                srg.key = def->key;
                srg.row_rect = row_r;

                int32_t ctrl_w = (def->type == SettingType::Enum) ? 180 : 100;
                srg.control_rect = core::Rect{
                    row_r.x + row_r.width - ctrl_w - 12,
                    row_r.y + (row_h - 36) / 2,
                    ctrl_w,
                    36
                };

                if (def->type == SettingType::Enum && !def->enum_values.empty()) {
                    size_t n = def->enum_values.size();
                    int32_t seg_w = ctrl_w / static_cast<int32_t>(n);
                    for (size_t i = 0; i < n; ++i) {
                        srg.option_rects.push_back(core::Rect{
                            srg.control_rect.x + static_cast<int32_t>(i) * seg_w,
                            srg.control_rect.y,
                            seg_w,
                            36
                        });
                    }
                }

                setting_rows_.push_back(std::move(srg));
                row_y += row_h + 8;
            }

            int32_t total_h = (row_y + scroll_y_) - content_rect_.y;
            max_scroll_y_ = std::max(0, total_h - content_rect_.height);
        }
    }

    set_scroll_y(scroll_y_);
}

void SettingsLayout::set_scroll_y(int32_t y) noexcept {
    scroll_y_ = std::clamp(y, 0, max_scroll_y_);
}

void SettingsLayout::scroll_by(int32_t dy) noexcept {
    set_scroll_y(scroll_y_ + dy);
}

SettingsHitResult SettingsLayout::hit_test(int32_t x, int32_t y) const {
    SettingsHitResult hit;
    hit.type = SettingsHitType::None;

    core::Point pt{x, y};

    // Check outside window
    if (!window_rect_.contains(pt)) {
        return hit;
    }

    // 1. Titlebar buttons
    if (close_btn_rect_.contains(pt)) {
        hit.type = SettingsHitType::CloseButton;
        hit.rect = close_btn_rect_;
        return hit;
    }
    if (max_btn_rect_.contains(pt)) {
        hit.type = SettingsHitType::MaximizeButton;
        hit.rect = max_btn_rect_;
        return hit;
    }
    if (min_btn_rect_.contains(pt)) {
        hit.type = SettingsHitType::MinimizeButton;
        hit.rect = min_btn_rect_;
        return hit;
    }
    if (back_btn_rect_.width > 0 && back_btn_rect_.contains(pt)) {
        hit.type = SettingsHitType::BackButton;
        hit.rect = back_btn_rect_;
        return hit;
    }
    if (titlebar_rect_.contains(pt)) {
        hit.type = SettingsHitType::Titlebar;
        hit.rect = titlebar_rect_;
        return hit;
    }

    // 2. Search Bar
    if (search_clear_rect_.contains(pt)) {
        hit.type = SettingsHitType::SearchClear;
        hit.rect = search_clear_rect_;
        return hit;
    }
    if (search_bar_rect_.contains(pt)) {
        hit.type = SettingsHitType::SearchBar;
        hit.rect = search_bar_rect_;
        return hit;
    }

    // 3. Category Items (sidebar or portrait list)
    for (const auto& [cat, rect] : category_rects_) {
        if (rect.contains(pt)) {
            hit.type = SettingsHitType::CategoryItem;
            hit.category = cat;
            hit.rect = rect;
            return hit;
        }
    }

    // 4. Setting Rows
    if (content_rect_.contains(pt)) {
        for (const auto& row : setting_rows_) {
            if (row.row_rect.contains(pt)) {
                hit.setting_key = row.key;
                hit.rect = row.row_rect;

                // Check option segments for radio
                for (size_t i = 0; i < row.option_rects.size(); ++i) {
                    if (row.option_rects[i].contains(pt)) {
                        hit.type = SettingsHitType::SettingRadioOption;
                        hit.option_index = i;
                        hit.rect = row.option_rects[i];
                        return hit;
                    }
                }

                // Check control rect (toggle switch / slider / action)
                if (row.control_rect.contains(pt)) {
                    if (row.key.find("enabled") != std::string::npos ||
                        row.key.find("show_") != std::string::npos ||
                        row.key.find("tap_to_click") != std::string::npos ||
                        row.key.find("ambient_glow") != std::string::npos) {
                        hit.type = SettingsHitType::SettingToggle;
                    } else if (row.key.find("scale") != std::string::npos ||
                               row.key.find("timeout") != std::string::npos ||
                               row.key.find("threshold") != std::string::npos ||
                               row.key.find("duration") != std::string::npos ||
                               row.key.find("size") != std::string::npos ||
                               row.key.find("margin") != std::string::npos ||
                               row.key.find("step") != std::string::npos) {
                        hit.type = SettingsHitType::SettingSlider;
                        double frac = static_cast<double>(x - row.control_rect.x) / static_cast<double>(row.control_rect.width);
                        hit.slider_fraction = std::clamp(frac, 0.0, 1.0);
                    } else {
                        hit.type = SettingsHitType::SettingAction;
                    }
                    hit.rect = row.control_rect;
                    return hit;
                }

                hit.type = SettingsHitType::SettingAction;
                return hit;
            }
        }
        hit.type = SettingsHitType::ContentArea;
        hit.rect = content_rect_;
        return hit;
    }

    return hit;
}

} // namespace ldde::settings
