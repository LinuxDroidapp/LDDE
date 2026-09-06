#include "ldde/launcher/launcher_category.hpp"
#include <algorithm>
#include <cctype>

namespace ldde::launcher {

const std::string LauncherCategory::kAllCategory = "All";

static std::string to_lower(std::string_view sv) {
    std::string s;
    s.reserve(sv.size());
    for (char c : sv) {
        s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return s;
}

std::vector<std::string> LauncherCategory::standard_category_ids() {
    return {
        "All",
        "Development",
        "Education",
        "Game",
        "Graphics",
        "Network",
        "AudioVideo",
        "Office",
        "Science",
        "Settings",
        "System",
        "Utility",
        "Other"
    };
}

std::string LauncherCategory::display_name_for_category(std::string_view category_id) {
    if (category_id == "All") return "All";
    if (category_id == "Development") return "Development";
    if (category_id == "Education") return "Education";
    if (category_id == "Game") return "Games";
    if (category_id == "Graphics") return "Graphics";
    if (category_id == "Network") return "Internet";
    if (category_id == "AudioVideo") return "Multimedia";
    if (category_id == "Office") return "Office";
    if (category_id == "Science") return "Science";
    if (category_id == "Settings") return "Settings";
    if (category_id == "System") return "System";
    if (category_id == "Utility") return "Utilities";
    if (category_id == "Other") return "Other";
    return std::string(category_id);
}

std::string LauncherCategory::canonicalize_category(std::string_view raw_category) {
    std::string lower = to_lower(raw_category);

    if (lower == "audio" || lower == "video" || lower == "audiovideo" || lower == "player" || lower == "recorder" || lower == "music") {
        return "AudioVideo";
    }
    if (lower == "network" || lower == "webbrowser" || lower == "email" || lower == "chat" || lower == "feed" || lower == "filetransfer") {
        return "Network";
    }
    if (lower == "game" || lower == "games" || lower == "arcadegame" || lower == "actiongame" || lower == "boardgame") {
        return "Game";
    }
    if (lower == "development" || lower == "ide" || lower == "debugger" || lower == "building" || lower == "translation") {
        return "Development";
    }
    if (lower == "office" || lower == "wordprocessor" || lower == "spreadsheet" || lower == "presentation") {
        return "Office";
    }
    if (lower == "graphics" || lower == "rastergraphics" || lower == "vectorgraphics" || lower == "photography" || lower == "2dgraphics" || lower == "3dgraphics") {
        return "Graphics";
    }
    if (lower == "system" || lower == "terminalemulator" || lower == "filemanager" || lower == "monitor" || lower == "security") {
        return "System";
    }
    if (lower == "utility" || lower == "utilities" || lower == "texteditor" || lower == "calculator" || lower == "archiving" || lower == "compression") {
        return "Utility";
    }
    if (lower == "settings" || lower == "desktopsettings" || lower == "hardwaresettings") {
        return "Settings";
    }
    if (lower == "science" || lower == "math" || lower == "astronomy" || lower == "chemistry" || lower == "physics") {
        return "Science";
    }
    if (lower == "education" || lower == "teaching" || lower == "languages") {
        return "Education";
    }

    return std::string(raw_category);
}

bool LauncherCategory::matches_category(
    const application::ApplicationMetadata& meta,
    std::string_view category_id) {
    if (category_id == "All" || category_id.empty()) {
        return true;
    }

    if (category_id == "Other") {
        if (meta.categories().empty()) {
            return true;
        }
        for (const auto& cat : meta.categories()) {
            std::string canon = canonicalize_category(cat);
            for (const auto& std_id : standard_category_ids()) {
                if (std_id != "All" && std_id != "Other" && canon == std_id) {
                    return false;
                }
            }
        }
        return true;
    }

    for (const auto& cat : meta.categories()) {
        if (cat == category_id || canonicalize_category(cat) == category_id) {
            return true;
        }
    }
    return false;
}

std::vector<LauncherCategoryInfo> LauncherCategory::compute_categories(
    const std::vector<application::ApplicationMetadata>& apps,
    bool include_empty) {
    std::vector<LauncherCategoryInfo> result;
    auto std_ids = standard_category_ids();
    result.reserve(std_ids.size());

    for (const auto& cid : std_ids) {
        size_t count = 0;
        if (cid == "All") {
            count = apps.size();
        } else {
            for (const auto& app : apps) {
                if (matches_category(app, cid)) {
                    ++count;
                }
            }
        }

        if (count > 0 || include_empty || cid == "All") {
            LauncherCategoryInfo info;
            info.id = cid;
            info.display_name = display_name_for_category(cid);
            info.count = count;
            result.push_back(std::move(info));
        }
    }

    return result;
}

} // namespace ldde::launcher

