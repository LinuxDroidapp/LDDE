#pragma once

#include <string>

namespace ldde::launcher {

struct LauncherFilter {
    std::string search_query;
    std::string category = "All";

    [[nodiscard]] bool is_empty() const noexcept {
        return search_query.empty() && (category.empty() || category == "All");
    }

    void reset() {
        search_query.clear();
        category = "All";
    }
};

} // namespace ldde::launcher

