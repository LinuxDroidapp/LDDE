#pragma once

#include <string>
#include <string_view>
#include <vector>
#include "ldde/settings/settings_schema.hpp"

namespace ldde::settings {

struct SearchResult {
    const SettingDefinition* definition = nullptr;
    int32_t score = 0;
};

class SettingsSearch {
public:
    explicit SettingsSearch(const SettingsSchema& schema);
    ~SettingsSearch() = default;

    [[nodiscard]] std::vector<const SettingDefinition*> search(std::string_view query) const;
    [[nodiscard]] std::vector<SearchResult> search_with_scores(std::string_view query) const;

private:
    const SettingsSchema& schema_;
};

} // namespace ldde::settings
