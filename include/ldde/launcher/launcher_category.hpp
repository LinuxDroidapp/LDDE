#pragma once

#include <string>
#include <string_view>
#include <vector>
#include "ldde/application/application_metadata.hpp"

namespace ldde::launcher {

struct LauncherCategoryInfo {
    std::string id;
    std::string display_name;
    size_t count = 0;
};

class LauncherCategory {
public:
    static const std::string kAllCategory;

    [[nodiscard]] static std::vector<std::string> standard_category_ids();
    [[nodiscard]] static std::string display_name_for_category(std::string_view category_id);
    [[nodiscard]] static std::string canonicalize_category(std::string_view raw_category);

    [[nodiscard]] static bool matches_category(
        const application::ApplicationMetadata& meta,
        std::string_view category_id);

    [[nodiscard]] static std::vector<LauncherCategoryInfo> compute_categories(
        const std::vector<application::ApplicationMetadata>& apps,
        bool include_empty = false);
};

} // namespace ldde::launcher

