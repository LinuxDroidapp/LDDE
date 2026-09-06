#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <optional>
#include <mutex>
#include "ldde/application/application_icon.hpp"

namespace ldde::launcher {

class LauncherIconResolver {
public:
    explicit LauncherIconResolver(std::string theme_name = "hicolor");
    ~LauncherIconResolver() = default;

    void set_theme(std::string theme_name) {
        theme_name_ = std::move(theme_name);
        clear_cache();
    }

    [[nodiscard]] const std::string& theme() const noexcept { return theme_name_; }

    void add_search_path(std::string path);
    void clear_cache();

    [[nodiscard]] std::optional<std::string> resolve(
        const application::ApplicationIconReference& icon_ref,
        int preferred_size = 48);

    [[nodiscard]] std::optional<std::string> resolve_icon_name(
        std::string_view icon_name,
        int preferred_size = 48);

    [[nodiscard]] size_t cached_count() const noexcept;

private:
    std::string theme_name_;
    std::vector<std::string> search_paths_;
    std::unordered_map<std::string, std::optional<std::string>> cache_;
    mutable std::mutex cache_mutex_;

    void initialize_default_paths();
    std::optional<std::string> search_icon_name_unlocked(std::string_view name, int preferred_size);
};

} // namespace ldde::launcher

