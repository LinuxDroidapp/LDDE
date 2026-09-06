#pragma once

#include <string>
#include <string_view>
#include <filesystem>
#include <chrono>

namespace ldde::application {

enum class DesktopEntrySourceType {
    User = 0,    // ~/.local/share/applications/ (Highest priority)
    Local = 1,   // /usr/local/share/applications/
    System = 2,  // /usr/share/applications/
    Custom = 3   // Arbitrary configured directory
};

[[nodiscard]] std::string_view source_type_name(DesktopEntrySourceType type) noexcept;

class DesktopEntrySource {
public:
    DesktopEntrySource() = default;
    DesktopEntrySource(std::filesystem::path path, DesktopEntrySourceType type, int priority = 0);

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }
    [[nodiscard]] DesktopEntrySourceType type() const noexcept { return type_; }
    [[nodiscard]] int priority() const noexcept { return priority_; }
    [[nodiscard]] std::filesystem::file_time_type modified_time() const noexcept { return modified_time_; }

    [[nodiscard]] bool is_user_level() const noexcept { return type_ == DesktopEntrySourceType::User; }
    [[nodiscard]] bool is_system_level() const noexcept { return type_ == DesktopEntrySourceType::System || type_ == DesktopEntrySourceType::Local; }

    void update_modified_time();

    [[nodiscard]] bool operator==(const DesktopEntrySource& other) const noexcept {
        return path_ == other.path_;
    }
    [[nodiscard]] bool operator!=(const DesktopEntrySource& other) const noexcept {
        return path_ != other.path_;
    }

private:
    std::filesystem::path path_;
    DesktopEntrySourceType type_ = DesktopEntrySourceType::System;
    int priority_ = 0; // Lower number means higher precedence
    std::filesystem::file_time_type modified_time_{};
};

} // namespace ldde::application

