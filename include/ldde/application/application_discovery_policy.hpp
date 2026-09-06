#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include "ldde/config/config.hpp"
#include "ldde/application/desktop_entry_source.hpp"

namespace ldde::application {

struct SearchDirectory {
    std::filesystem::path path;
    DesktopEntrySourceType type;
    int priority; // Lower number means higher precedence
};

class ApplicationDiscoveryPolicy {
public:
    ApplicationDiscoveryPolicy() = default;

    [[nodiscard]] static ApplicationDiscoveryPolicy from_config_and_env(const config::Config& config);

    void add_search_directory(std::filesystem::path path, DesktopEntrySourceType type, int priority);
    void clear_search_directories();

    [[nodiscard]] const std::vector<SearchDirectory>& search_directories() const noexcept {
        return directories_;
    }

    [[nodiscard]] const std::string& desktop_identity() const noexcept { return desktop_identity_; }
    void set_desktop_identity(std::string id) { desktop_identity_ = std::move(id); }

    [[nodiscard]] const std::string& locale() const noexcept { return locale_; }
    void set_locale(std::string loc) { locale_ = std::move(loc); }

private:
    std::vector<SearchDirectory> directories_;
    std::string desktop_identity_ = "LinuxDroid";
    std::string locale_;
};

} // namespace ldde::application

