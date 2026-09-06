#include "ldde/application/desktop_entry.hpp"
#include <algorithm>
#include <cctype>

namespace ldde::application {

namespace {

struct ParsedLocale {
    std::string lang;
    std::string country;
    std::string modifier;
};

ParsedLocale parse_locale(std::string_view loc) {
    ParsedLocale pl;
    if (loc.empty()) return pl;

    // Format: [language[_territory][.codeset][@modifier]]
    std::string_view rem = loc;

    // Check for @modifier
    auto at_pos = rem.find('@');
    if (at_pos != std::string_view::npos) {
        pl.modifier = rem.substr(at_pos + 1);
        rem = rem.substr(0, at_pos);
    }

    // Strip .codeset
    auto dot_pos = rem.find('.');
    if (dot_pos != std::string_view::npos) {
        rem = rem.substr(0, dot_pos);
    }

    // Check for _territory
    auto under_pos = rem.find('_');
    if (under_pos != std::string_view::npos) {
        pl.lang = rem.substr(0, under_pos);
        pl.country = rem.substr(under_pos + 1);
    } else {
        pl.lang = rem;
    }

    return pl;
}

std::vector<std::string> generate_locale_candidates(std::string_view loc) {
    std::vector<std::string> candidates;
    if (loc.empty()) {
        candidates.emplace_back("");
        return candidates;
    }

    ParsedLocale pl = parse_locale(loc);
    if (!pl.lang.empty()) {
        if (!pl.country.empty() && !pl.modifier.empty()) {
            candidates.push_back(pl.lang + "_" + pl.country + "@" + pl.modifier);
        }
        if (!pl.country.empty()) {
            candidates.push_back(pl.lang + "_" + pl.country);
        }
        if (!pl.modifier.empty()) {
            candidates.push_back(pl.lang + "@" + pl.modifier);
        }
        candidates.push_back(pl.lang);
    }

    // Finally, unlocalized fallback
    candidates.emplace_back("");
    return candidates;
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

} // namespace

void DesktopEntry::set_value(
    std::string group, std::string key, std::string locale, std::string value) {
    data_[std::move(group)][std::move(key)][std::move(locale)] = std::move(value);
}

bool DesktopEntry::has_group(std::string_view group) const noexcept {
    return data_.contains(std::string(group));
}

std::vector<std::string> DesktopEntry::groups() const {
    std::vector<std::string> res;
    res.reserve(data_.size());
    for (const auto& [grp, _] : data_) {
        res.push_back(grp);
    }
    return res;
}

bool DesktopEntry::has_key(std::string_view group, std::string_view key) const noexcept {
    auto git = data_.find(std::string(group));
    if (git == data_.end()) return false;
    return git->second.contains(std::string(key));
}

std::string DesktopEntry::get_string(
    std::string_view group, std::string_view key, std::string_view default_value) const {
    return get_localized_string(group, key, "", default_value);
}

std::string DesktopEntry::get_localized_string(
    std::string_view group, std::string_view key, std::string_view locale, std::string_view default_value) const {
    auto git = data_.find(std::string(group));
    if (git == data_.end()) return std::string(default_value);

    auto kit = git->second.find(std::string(key));
    if (kit == git->second.end()) return std::string(default_value);

    const auto& locale_map = kit->second;
    std::vector<std::string> candidates = generate_locale_candidates(locale);

    for (const auto& candidate : candidates) {
        auto lit = locale_map.find(candidate);
        if (lit != locale_map.end() && !lit->second.empty()) {
            return lit->second;
        }
    }

    // Default unlocalized if candidates didn't find anything
    auto unloc = locale_map.find("");
    if (unloc != locale_map.end()) {
        return unloc->second;
    }

    return std::string(default_value);
}

bool DesktopEntry::get_bool(
    std::string_view group, std::string_view key, bool default_value) const noexcept {
    std::string val = get_string(group, key);
    if (val.empty()) return default_value;
    std::string lower;
    lower.reserve(val.size());
    for (char c : val) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    if (lower == "true" || lower == "1") return true;
    if (lower == "false" || lower == "0") return false;
    return default_value;
}

std::vector<std::string> DesktopEntry::get_string_list(
    std::string_view group, std::string_view key) const {
    std::string raw = get_string(group, key);
    std::vector<std::string> list;
    if (raw.empty()) return list;

    std::string current;
    bool escape = false;

    for (size_t i = 0; i < raw.size(); ++i) {
        char c = raw[i];
        if (escape) {
            if (c == ';') {
                current.push_back(';');
            } else if (c == 's') {
                current.push_back(' ');
            } else if (c == 'n') {
                current.push_back('\n');
            } else if (c == 't') {
                current.push_back('\t');
            } else if (c == 'r') {
                current.push_back('\r');
            } else if (c == '\\') {
                current.push_back('\\');
            } else {
                current.push_back(c);
            }
            escape = false;
        } else if (c == '\\') {
            escape = true;
        } else if (c == ';') {
            std::string item = trim(current);
            if (!item.empty()) {
                list.push_back(std::move(item));
            }
            current.clear();
        } else {
            current.push_back(c);
        }
    }

    std::string remaining = trim(current);
    if (!remaining.empty()) {
        list.push_back(std::move(remaining));
    }

    return list;
}

std::vector<std::string> DesktopEntry::actions() const {
    return get_string_list("Desktop Entry", "Actions");
}

bool DesktopEntry::is_valid_application() const noexcept {
    if (!has_group("Desktop Entry")) return false;
    std::string type = get_string("Desktop Entry", "Type");
    if (type != "Application") return false;

    std::string name = get_string("Desktop Entry", "Name");
    if (name.empty()) return false;

    std::string exec = get_string("Desktop Entry", "Exec");
    if (exec.empty()) return false;

    return true;
}

} // namespace ldde::application

