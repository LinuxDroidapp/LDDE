#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <cstdint>

namespace ldde::settings {

enum class SettingsCategory {
    Appearance,
    Display,
    Windows,
    Desktop,
    Dock,
    Launcher,
    Input,
    Notifications,
    SystemUI,
    About
};

[[nodiscard]] std::string_view category_name(SettingsCategory category) noexcept;
[[nodiscard]] std::optional<SettingsCategory> parse_category(std::string_view name) noexcept;

enum class SettingType {
    Bool,
    Int,
    Double,
    String,
    Enum
};

[[nodiscard]] std::string_view setting_type_name(SettingType type) noexcept;

enum class ApplyMode {
    Immediate,
    SessionLevel,
    RestartRequired
};

[[nodiscard]] std::string_view apply_mode_name(ApplyMode mode) noexcept;

enum class SettingsWindowMode {
    Closed,
    Normal,
    Maximized,
    Minimized
};

[[nodiscard]] std::string_view window_mode_name(SettingsWindowMode mode) noexcept;

} // namespace ldde::settings
