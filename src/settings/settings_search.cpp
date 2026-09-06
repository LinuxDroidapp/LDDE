#include "ldde/settings/settings_search.hpp"
#include <algorithm>
#include <cctype>

namespace ldde::settings {

namespace {

std::string to_lower(std::string_view sv) {
    std::string res;
    res.reserve(sv.size());
    for (char c : sv) {
        res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return res;
}

std::string trim(std::string_view sv) {
    auto start = sv.begin();
    while (start != sv.end() && std::isspace(static_cast<unsigned char>(*start))) {
        ++start;
    }
    auto end = sv.end();
    while (end != start && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    return std::string(start, end);
}

int32_t score_definition(const SettingDefinition& def, const std::string& query) {
    if (query.empty()) return 0;

    std::string title_lower = to_lower(def.title);
    std::string desc_lower = to_lower(def.description);
    std::string cat_lower = to_lower(category_name(def.category));
    std::string key_lower = to_lower(def.key);

    int32_t score = 0;

    // 1. Exact match on title
    if (title_lower == query) {
        score = std::max(score, 1000);
    }
    // 2. Prefix match on title
    else if (title_lower.rfind(query, 0) == 0) {
        score = std::max(score, 800);
    }
    // 3. Word prefix in title
    else {
        auto pos = title_lower.find(query);
        if (pos != std::string::npos) {
            if (pos > 0 && title_lower[pos - 1] == ' ') {
                score = std::max(score, 600);
            } else {
                score = std::max(score, 400);
            }
        }
    }

    // 4. Keyword match
    for (const auto& kw : def.keywords) {
        std::string kw_lower = to_lower(kw);
        if (kw_lower == query) {
            score = std::max(score, 350);
        } else if (kw_lower.rfind(query, 0) == 0) {
            score = std::max(score, 300);
        } else if (kw_lower.find(query) != std::string::npos) {
            score = std::max(score, 250);
        }
    }

    // 5. Key match (e.g. "dock.position")
    if (key_lower.find(query) != std::string::npos) {
        score = std::max(score, 250);
    }

    // 6. Substring in description
    if (desc_lower.find(query) != std::string::npos) {
        score = std::max(score, 200);
    }

    // 7. Category match
    if (cat_lower.find(query) != std::string::npos) {
        score = std::max(score, 100);
    }

    return score;
}

} // namespace

SettingsSearch::SettingsSearch(const SettingsSchema& schema)
    : schema_(schema) {}

std::vector<SearchResult> SettingsSearch::search_with_scores(std::string_view query) const {
    std::string q = to_lower(trim(query));
    if (q.empty()) {
        return {};
    }

    std::vector<SearchResult> results;
    for (const auto& def : schema_.all_settings()) {
        int32_t score = score_definition(def, q);
        if (score > 0) {
            results.push_back({&def, score});
        }
    }

    std::sort(results.begin(), results.end(), [](const SearchResult& a, const SearchResult& b) {
        if (a.score != b.score) {
            return a.score > b.score;
        }
        return a.definition->title < b.definition->title;
    });

    return results;
}

std::vector<const SettingDefinition*> SettingsSearch::search(std::string_view query) const {
    auto results = search_with_scores(query);
    std::vector<const SettingDefinition*> defs;
    defs.reserve(results.size());
    for (const auto& r : results) {
        defs.push_back(r.definition);
    }
    return defs;
}

} // namespace ldde::settings
