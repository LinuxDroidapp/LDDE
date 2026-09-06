#include "ldde/settings/settings_types.hpp"
#include <algorithm>
#include <cctype>

namespace ldde::settings {

std::string_view category_name(SettingsCategory category) noexcept {
    switch (category) {
        case SettingsCategory::Appearance:    return "Appearance";
        case SettingsCategory::Display:       return "Display";
        case SettingsCategory::Windows:       return "Windows";
        case SettingsCategory::Desktop:       return "Desktop";
        case SettingsCategory::Dock:          return "Dock";
        case SettingsCategory::Launcher:      return "Launcher";
        case SettingsCategory::Input:         return "Input";
        case SettingsCategory::Notifications: return "Notifications";
        case SettingsCategory::SystemUI:      return "System UI";
        case SettingsCategory::About:         return "About";
    }
    return "Unknown";
}

std::optional<SettingsCategory> parse_category(std::string_view name) noexcept {
    std::string s;
    s.reserve(name.size());
    for (char c : name) {
        s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    if (s == "appearance")    return SettingsCategory::Appearance;
    if (s == "display")       return SettingsCategory::Display;
    if (s == "windows" || s == "window") return SettingsCategory::Windows;
    if (s == "desktop")       return SettingsCategory::Desktop;
    if (s == "dock")          return SettingsCategory::Dock;
    if (s == "launcher")      return SettingsCategory::Launcher;
    if (s == "input")         return SettingsCategory::Input;
    if (s == "notifications" || s == "notification") return SettingsCategory::Notifications;
    if (s == "system ui" || s == "system" || s == "systemui" || s == "system_ui") return SettingsCategory::SystemUI;
    if (s == "about")         return SettingsCategory::About;

    return std::nullopt;
}

std::string_view setting_type_name(SettingType type) noexcept {
    switch (type) {
        case SettingType::Bool:   return "Bool";
        case SettingType::Int:    return "Int";
        case SettingType::Double: return "Double";
        case SettingType::String: return "String";
        case SettingType::Enum:   return "Enum";
    }
    return "Unknown";
}

std::string_view apply_mode_name(ApplyMode mode) noexcept {
    switch (mode) {
        case ApplyMode::Immediate:       return "Immediate";
        case ApplyMode::SessionLevel:    return "Session-Level";
        case ApplyMode::RestartRequired: return "Restart Required";
    }
    return "Unknown";
}

std::string_view window_mode_name(SettingsWindowMode mode) noexcept {
    switch (mode) {
        case SettingsWindowMode::Closed:    return "Closed";
        case SettingsWindowMode::Normal:    return "Normal";
        case SettingsWindowMode::Maximized: return "Maximized";
        case SettingsWindowMode::Minimized: return "Minimized";
    }
    return "Unknown";
}

} // namespace ldde::settings
