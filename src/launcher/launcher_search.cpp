#include "ldde/launcher/launcher_search.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace ldde::launcher {

namespace {

std::string_view trim_view(std::string_view text) noexcept {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    return text;
}

bool iequal(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

bool istarts_with(std::string_view str, std::string_view prefix) noexcept {
    if (prefix.empty()) return true;
    if (str.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(str[i])) !=
            std::tolower(static_cast<unsigned char>(prefix[i]))) {
            return false;
        }
    }
    return true;
}

bool iword_starts_with(std::string_view text, std::string_view prefix) noexcept {
    if (prefix.empty()) return true;
    size_t pos = 0;
    while (pos < text.size()) {
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) {
            ++pos;
        }
        if (pos >= text.size()) break;
        if (istarts_with(text.substr(pos), prefix)) {
            return true;
        }
        while (pos < text.size() && !std::isspace(static_cast<unsigned char>(text[pos]))) {
            ++pos;
        }
    }
    return false;
}

bool icontains(std::string_view str, std::string_view sub) noexcept {
    if (sub.empty()) return true;
    if (str.size() < sub.size()) return false;
    auto it = std::search(
        str.begin(), str.end(),
        sub.begin(), sub.end(),
        [](char ch1, char ch2) {
            return std::tolower(static_cast<unsigned char>(ch1)) ==
                   std::tolower(static_cast<unsigned char>(ch2));
        }
    );
    return it != str.end();
}

int icompare(std::string_view a, std::string_view b) noexcept {
    size_t min_len = std::min(a.size(), b.size());
    for (size_t i = 0; i < min_len; ++i) {
        char ca = static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
        char cb = static_cast<char>(std::tolower(static_cast<unsigned char>(b[i])));
        if (ca != cb) {
            return (ca < cb) ? -1 : 1;
        }
    }
    if (a.size() != b.size()) {
        return (a.size() < b.size()) ? -1 : 1;
    }
    return 0;
}

} // namespace

std::string LauncherSearch::normalize(std::string_view text) {
    std::string_view trimmed = trim_view(text);
    std::string result;
    result.reserve(trimmed.size());
    for (char ch : trimmed) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return result;
}

int LauncherSearch::score_match(
    const application::ApplicationMetadata& app,
    std::string_view query) {
    std::string_view trimmed_query = trim_view(query);
    if (trimmed_query.empty()) {
        return 0;
    }

    int best_score = 0;

    // 1. Name matches (highest priority)
    std::string_view name = trim_view(app.name());
    if (iequal(name, trimmed_query)) {
        best_score = std::max(best_score, 1000);
    } else if (istarts_with(name, trimmed_query)) {
        best_score = std::max(best_score, 800);
    } else if (iword_starts_with(name, trimmed_query)) {
        best_score = std::max(best_score, 700);
    } else if (icontains(name, trimmed_query)) {
        best_score = std::max(best_score, 600);
    }

    // 2. Generic Name matches
    if (!app.generic_name().empty()) {
        std::string_view generic_name = trim_view(app.generic_name());
        if (iequal(generic_name, trimmed_query)) {
            best_score = std::max(best_score, 550);
        } else if (istarts_with(generic_name, trimmed_query)) {
            best_score = std::max(best_score, 500);
        } else if (icontains(generic_name, trimmed_query)) {
            best_score = std::max(best_score, 400);
        }
    }

    // 3. Keywords
    for (const auto& kw : app.keywords()) {
        std::string_view kw_view = trim_view(kw);
        if (iequal(kw_view, trimmed_query)) {
            best_score = std::max(best_score, 350);
        } else if (istarts_with(kw_view, trimmed_query)) {
            best_score = std::max(best_score, 300);
        } else if (icontains(kw_view, trimmed_query)) {
            best_score = std::max(best_score, 250);
        }
    }

    // 4. Categories
    for (const auto& cat : app.categories()) {
        if (icontains(trim_view(cat), trimmed_query)) {
            best_score = std::max(best_score, 200);
        }
    }

    // 5. Comment
    if (!app.comment().empty()) {
        if (icontains(trim_view(app.comment()), trimmed_query)) {
            best_score = std::max(best_score, 100);
        }
    }

    return best_score;
}

std::vector<SearchResult> LauncherSearch::search(
    const std::vector<application::ApplicationMetadata>& apps,
    std::string_view query) {
    std::string_view trimmed_query = trim_view(query);
    std::vector<SearchResult> results;
    results.reserve(apps.size());

    if (trimmed_query.empty()) {
        for (const auto& app : apps) {
            results.push_back(SearchResult{&app, 0});
        }
    } else {
        for (const auto& app : apps) {
            int sc = score_match(app, trimmed_query);
            if (sc > 0) {
                results.push_back(SearchResult{&app, sc});
            }
        }
    }

    // Deterministic sorting with zero-allocation comparator
    std::sort(results.begin(), results.end(), [](const SearchResult& a, const SearchResult& b) {
        if (a.score != b.score) {
            return a.score > b.score; // Higher score first
        }
        int cmp = icompare(a.application->name(), b.application->name());
        if (cmp != 0) {
            return cmp < 0; // Alphabetical localized name
        }
        return a.application->id() < b.application->id(); // ApplicationId tie-break
    });

    return results;
}

} // namespace ldde::launcher
