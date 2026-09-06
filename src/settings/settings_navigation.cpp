#include "ldde/settings/settings_navigation.hpp"

namespace ldde::settings {

void SettingsNavigation::select_category(SettingsCategory category) noexcept {
    active_category_ = category;
    selected_index_ = 0;
}

void SettingsNavigation::drill_down(SettingsCategory category) noexcept {
    active_category_ = category;
    in_category_detail_ = true;
    selected_index_ = 0;
}

bool SettingsNavigation::navigate_back() noexcept {
    if (is_searching()) {
        clear_search();
        return true;
    }
    if (in_category_detail_) {
        in_category_detail_ = false;
        selected_index_ = 0;
        return true;
    }
    return false;
}

void SettingsNavigation::set_search_query(std::string query) {
    search_query_ = std::move(query);
    selected_index_ = 0;
}

void SettingsNavigation::clear_search() {
    search_query_.clear();
    selected_index_ = 0;
}

void SettingsNavigation::select_next(size_t total_items) noexcept {
    if (total_items == 0) return;
    if (selected_index_ + 1 < total_items) {
        ++selected_index_;
    } else {
        selected_index_ = 0;
    }
}

void SettingsNavigation::select_prev(size_t total_items) noexcept {
    if (total_items == 0) return;
    if (selected_index_ > 0) {
        --selected_index_;
    } else {
        selected_index_ = total_items - 1;
    }
}

void SettingsNavigation::reset() noexcept {
    active_category_ = SettingsCategory::Appearance;
    in_category_detail_ = false;
    search_query_.clear();
    selected_index_ = 0;
}

} // namespace ldde::settings
