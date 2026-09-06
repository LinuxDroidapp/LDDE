#pragma once

#include <string>
#include <string_view>
#include "ldde/settings/settings_types.hpp"

namespace ldde::settings {

class SettingsNavigation {
public:
    SettingsNavigation() = default;
    ~SettingsNavigation() = default;

    [[nodiscard]] SettingsCategory active_category() const noexcept { return active_category_; }
    void select_category(SettingsCategory category) noexcept;
    void drill_down(SettingsCategory category) noexcept;

    [[nodiscard]] bool is_in_category_detail() const noexcept { return in_category_detail_; }
    void set_in_category_detail(bool detail) noexcept { in_category_detail_ = detail; }

    [[nodiscard]] bool navigate_back() noexcept;

    [[nodiscard]] const std::string& search_query() const noexcept { return search_query_; }
    [[nodiscard]] bool is_searching() const noexcept { return !search_query_.empty(); }
    void set_search_query(std::string query);
    void clear_search();

    [[nodiscard]] size_t selected_index() const noexcept { return selected_index_; }
    void set_selected_index(size_t index) noexcept { selected_index_ = index; }
    void select_next(size_t total_items) noexcept;
    void select_prev(size_t total_items) noexcept;

    void reset() noexcept;

private:
    SettingsCategory active_category_ = SettingsCategory::Appearance;
    bool in_category_detail_ = false;
    std::string search_query_;
    size_t selected_index_ = 0;
};

} // namespace ldde::settings
