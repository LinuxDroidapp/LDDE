#include "ldde/application/application_discovery_policy.hpp"
#include <cstdlib>
#include <sstream>

namespace ldde::application {

namespace {

std::vector<std::string> split_colon(std::string_view sv) {
    std::vector<std::string> items;
    std::string current;
    for (char c : sv) {
        if (c == ':') {
            if (!current.empty()) {
                items.push_back(std::move(current));
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        items.push_back(std::move(current));
    }
    return items;
}

std::string detect_system_locale() {
    const char* lc_all = std::getenv("LC_ALL");
    if (lc_all && *lc_all) return lc_all;

    const char* lc_messages = std::getenv("LC_MESSAGES");
    if (lc_messages && *lc_messages) return lc_messages;

    const char* lang = std::getenv("LANG");
    if (lang && *lang) return lang;

    return "";
}

} // namespace

ApplicationDiscoveryPolicy ApplicationDiscoveryPolicy::from_config_and_env(const config::Config& config) {
    ApplicationDiscoveryPolicy policy;
    policy.desktop_identity_ = config.get_string_or("application", "desktop_identity", "LinuxDroid");
    policy.locale_ = detect_system_locale();

    int next_priority = 0;

    // 1. User Application Directory ($XDG_DATA_HOME/applications or ~/.local/share/applications)
    const char* xdg_data_home = std::getenv("XDG_DATA_HOME");
    std::filesystem::path user_apps;
    if (xdg_data_home && *xdg_data_home) {
        user_apps = std::filesystem::path(xdg_data_home) / "applications";
    } else {
        const char* home = std::getenv("HOME");
        if (home && *home) {
            user_apps = std::filesystem::path(home) / ".local" / "share" / "applications";
        }
    }

    if (!user_apps.empty()) {
        policy.add_search_directory(user_apps, DesktopEntrySourceType::User, next_priority++);
    }

    // 2. System Directories ($XDG_DATA_DIRS/applications or /usr/local/share, /usr/share)
    const char* xdg_data_dirs = std::getenv("XDG_DATA_DIRS");
    std::vector<std::string> sys_data_paths;
    if (xdg_data_dirs && *xdg_data_dirs) {
        sys_data_paths = split_colon(xdg_data_dirs);
    } else {
        sys_data_paths = {"/usr/local/share", "/usr/share"};
    }

    for (const auto& dir_str : sys_data_paths) {
        std::filesystem::path p = std::filesystem::path(dir_str) / "applications";
        DesktopEntrySourceType st = (dir_str.find("/usr/local") != std::string::npos)
                                        ? DesktopEntrySourceType::Local
                                        : DesktopEntrySourceType::System;
        policy.add_search_directory(p, st, next_priority++);
    }

    return policy;
}

void ApplicationDiscoveryPolicy::add_search_directory(
    std::filesystem::path path, DesktopEntrySourceType type, int priority) {
    directories_.push_back(SearchDirectory{std::move(path), type, priority});
}

void ApplicationDiscoveryPolicy::clear_search_directories() {
    directories_.clear();
}

} // namespace ldde::application

