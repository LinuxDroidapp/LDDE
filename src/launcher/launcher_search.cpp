#include "ldde/launcher/launcher_search.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace ldde::launcher {

namespace {

bool word_starts_with(std::string_view text, std::string_view prefix) {
    if (prefix.empty()) return true;
    size_t pos = 0;
    while (pos < text.size()) {
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) {
            ++pos;
        }
        if (pos >= text.size()) break;
        if (text.substr(pos).starts_with(prefix)) {
            return true;
        }
        while (pos < text.size() && !std::isspace(static_cast<unsigned char>(text[pos]))) {
            ++pos;
        }
    }
    return false;
}

} // namespace

std::string LauncherSearch::normalize(std::string_view text) {
    auto start = text.begin();
    while (start != text.end() && std::isspace(static_cast<unsigned char>(*start))) {
        ++start;
    }
    auto end = text.end();
    while (end != start && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }

    std::string result;
    result.reserve(std::distance(start, end));
    for (auto it = start; it != end; ++it) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(*it))));
    }
    return result;
}

int LauncherSearch::score_match(
    const application::ApplicationMetadata& app,
    std::string_view normalized_query) {
    if (normalized_query.empty()) {
        return 0;
    }

    int best_score = 0;

    // 1. Name matches
    std::string norm_name = normalize(app.name());
    if (norm_name == normalized_query) {
        best_score = std::max(best_score, 1000);
    } else if (norm_name.starts_with(normalized_query)) {
        best_score = std::max(best_score, 800);
    } else if (word_starts_with(norm_name, normalized_query)) {
        best_score = std::max(best_score, 700);
    } else if (norm_name.find(normalized_query) != std::string::npos) {
        best_score = std::max(best_score, 600);
    }

    // 2. Generic Name matches
    if (!app.generic_name().empty()) {
        std::string norm_generic = normalize(app.generic_name());
        if (norm_generic == normalized_query) {
            best_score = std::max(best_score, 550);
        } else if (norm_generic.starts_with(normalized_query)) {
            best_score = std::max(best_score, 500);
        } else if (norm_generic.find(normalized_query) != std::string::npos) {
            best_score = std::max(best_score, 400);
        }
    }

    // 3. Keywords
    for (const auto& kw : app.keywords()) {
        std::string norm_kw = normalize(kw);
        if (norm_kw == normalized_query) {
            best_score = std::max(best_score, 350);
        } else if (norm_kw.starts_with(normalized_query)) {
            best_score = std::max(best_score, 300);
        } else if (norm_kw.find(normalized_query) != std::string::npos) {
            best_score = std::max(best_score, 250);
        }
    }

    // 4. Categories
    for (const auto& cat : app.categories()) {
        std::string norm_cat = normalize(cat);
        if (norm_cat.find(normalized_query) != std::string::npos) {
            best_score = std::max(best_score, 200);
        }
    }

    // 5. Comment
    if (!app.comment().empty()) {
        std::string norm_comment = normalize(app.comment());
        if (norm_comment.find(normalized_query) != std::string::npos) {
            best_score = std::max(best_score, 100);
        }
    }

    return best_score;
}

std::vector<SearchResult> LauncherSearch::search(
    const std::vector<application::ApplicationMetadata>& apps,
    std::string_view query) {
    std::string norm_query = normalize(query);
    std::vector<SearchResult> results;
    results.reserve(apps.size());

    if (norm_query.empty()) {
        for (const auto& app : apps) {
            results.push_back(SearchResult{&app, 0});
        }
    } else {
        for (const auto& app : apps) {
            int sc = score_match(app, norm_query);
            if (sc > 0) {
                results.push_back(SearchResult{&app, sc});
            }
        }
    }

    // Deterministic sorting
    std::sort(results.begin(), results.end(), [](const SearchResult& a, const SearchResult& b) {
        if (a.score != b.score) {
            return a.score > b.score; // Higher score first
        }
        std::string na = normalize(a.application->name());
        std::string nb = normalize(b.application->name());
        if (na != nb) {
            return na < nb; // Alphabetical localized name
        }
        return a.application->id() < b.application->id(); // ApplicationId tie-break
    });

    return results;
}

} // namespace ldde::launcher
