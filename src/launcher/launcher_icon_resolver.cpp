#include "ldde/launcher/launcher_icon_resolver.hpp"
#include "ldde/core/logging.hpp"

#include <unistd.h>
#include <cstdlib>
#include <filesystem>
#include <algorithm>
#include <sstream>

namespace ldde::launcher {

namespace fs = std::filesystem;

LauncherIconResolver::LauncherIconResolver(std::string theme_name)
    : theme_name_(std::move(theme_name)) {
    initialize_default_paths();
}

void LauncherIconResolver::initialize_default_paths() {
    search_paths_.clear();

    const char* xdg_data_home = std::getenv("XDG_DATA_HOME");
    if (xdg_data_home && *xdg_data_home) {
        search_paths_.push_back(std::string(xdg_data_home) + "/icons");
    } else {
        const char* home = std::getenv("HOME");
        if (home && *home) {
            search_paths_.push_back(std::string(home) + "/.local/share/icons");
            search_paths_.push_back(std::string(home) + "/.icons");
        }
    }

    const char* xdg_data_dirs = std::getenv("XDG_DATA_DIRS");
    if (xdg_data_dirs && *xdg_data_dirs) {
        std::stringstream ss(xdg_data_dirs);
        std::string dir;
        while (std::getline(ss, dir, ':')) {
            if (!dir.empty()) {
                search_paths_.push_back(dir + "/icons");
                search_paths_.push_back(dir + "/pixmaps");
            }
        }
    } else {
        search_paths_.push_back("/usr/local/share/icons");
        search_paths_.push_back("/usr/share/icons");
        search_paths_.push_back("/usr/share/pixmaps");
    }
}

void LauncherIconResolver::add_search_path(std::string path) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    search_paths_.insert(search_paths_.begin(), std::move(path));
    cache_.clear();
}

void LauncherIconResolver::clear_cache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cache_.clear();
}

size_t LauncherIconResolver::cached_count() const noexcept {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    return cache_.size();
}

std::optional<std::string> LauncherIconResolver::resolve(
    const application::ApplicationIconReference& icon_ref,
    int preferred_size) {
    if (!icon_ref.has_icon()) {
        return std::nullopt;
    }

    if (icon_ref.is_file_path()) {
        std::string path = icon_ref.raw();
        if (access(path.c_str(), R_OK) == 0) {
            return path;
        }
    }

    return resolve_icon_name(icon_ref.raw(), preferred_size);
}

std::optional<std::string> LauncherIconResolver::resolve_icon_name(
    std::string_view icon_name,
    int preferred_size) {
    if (icon_name.empty()) return std::nullopt;

    std::string name_key(icon_name);
    name_key += "@" + std::to_string(preferred_size);

    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = cache_.find(name_key);
        if (it != cache_.end()) {
            return it->second;
        }
    }

    auto res = search_icon_name_unlocked(icon_name, preferred_size);

    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cache_[name_key] = res;
    }

    return res;
}

std::optional<std::string> LauncherIconResolver::search_icon_name_unlocked(
    std::string_view icon_name,
    int preferred_size) {
    // 1. If absolute or relative path with extension
    if (icon_name.find('/') != std::string_view::npos) {
        std::string path(icon_name);
        if (access(path.c_str(), R_OK) == 0) {
            return path;
        }
    }

    // Strip extension if supplied in theme icon name (some .desktop have Name.png)
    std::string clean_name(icon_name);
    if (clean_name.ends_with(".png") || clean_name.ends_with(".svg") || clean_name.ends_with(".xpm")) {
        auto dot = clean_name.rfind('.');
        if (dot != std::string::npos) {
            clean_name = clean_name.substr(0, dot);
        }
    }

    std::vector<std::string> themes;
    if (!theme_name_.empty()) {
        themes.push_back(theme_name_);
    }
    if (theme_name_ != "hicolor") {
        themes.push_back("hicolor");
    }

    std::vector<std::string> sizes;
    sizes.push_back(std::to_string(preferred_size) + "x" + std::to_string(preferred_size));
    sizes.push_back("scalable");
    sizes.push_back("64x64");
    sizes.push_back("48x48");
    sizes.push_back("128x128");
    sizes.push_back("256x256");
    sizes.push_back("32x32");
    sizes.push_back("24x24");

    const std::vector<std::string> subcategories = {
        "apps", "categories", "status", "devices", "places", "actions", "mimetypes"
    };

    const std::vector<std::string> extensions = {".png", ".svg", ".xpm"};

    for (const auto& root : search_paths_) {
        // Direct match in directory (e.g. /usr/share/pixmaps)
        for (const auto& ext : extensions) {
            std::string direct_file = root + "/" + clean_name + ext;
            if (access(direct_file.c_str(), R_OK) == 0) {
                return direct_file;
            }
        }

        // Standard theme directory lookups
        for (const auto& theme : themes) {
            std::string theme_dir = root + "/" + theme;
            for (const auto& sz : sizes) {
                for (const auto& sub : subcategories) {
                    for (const auto& ext : extensions) {
                        std::string candidate = theme_dir + "/" + sz + "/" + sub + "/" + clean_name + ext;
                        if (access(candidate.c_str(), R_OK) == 0) {
                            return candidate;
                        }
                    }
                }
                for (const auto& ext : extensions) {
                    std::string candidate = theme_dir + "/" + sz + "/" + clean_name + ext;
                    if (access(candidate.c_str(), R_OK) == 0) {
                        return candidate;
                    }
                }
            }
        }
    }

    return std::nullopt;
}

} // namespace ldde::launcher

