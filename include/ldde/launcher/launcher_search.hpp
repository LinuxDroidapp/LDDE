#pragma once

#include <string>
#include <string_view>
#include <vector>
#include "ldde/application/application_metadata.hpp"

namespace ldde::launcher {

struct SearchResult {
    const application::ApplicationMetadata* application = nullptr;
    int score = 0;

    [[nodiscard]] bool operator==(const SearchResult& other) const noexcept {
        return application == other.application && score == other.score;
    }
};

class LauncherSearch {
public:
    [[nodiscard]] static int score_match(
        const application::ApplicationMetadata& app,
        std::string_view normalized_query);

    [[nodiscard]] static std::vector<SearchResult> search(
        const std::vector<application::ApplicationMetadata>& apps,
        std::string_view query);

    [[nodiscard]] static std::string normalize(std::string_view text);
};

} // namespace ldde::launcher

