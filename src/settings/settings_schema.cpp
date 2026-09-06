#include "ldde/settings/settings_schema.hpp"
#include <algorithm>
#include <cctype>

namespace ldde::settings {

namespace {

std::string to_lower(std::string_view sv) {
    std::string res;
    res.reserve(sv.size());
    for (char c : sv) {
        res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return res;
}

bool is_valid_hex_color(std::string_view hex) {
    if (hex.empty() || hex[0] != '#') return false;
    if (hex.size() != 7 && hex.size() != 9) return false;
    for (size_t i = 1; i < hex.size(); ++i) {
        if (!std::isxdigit(static_cast<unsigned char>(hex[i]))) return false;
    }
    return true;
}

} // namespace

std::string SettingDefinition::section() const {
    auto dot = key.find('.');
    if (dot != std::string::npos) {
        return key.substr(0, dot);
    }
    return "general";
}

std::string SettingDefinition::property() const {
    auto dot = key.find('.');
    if (dot != std::string::npos) {
        return key.substr(dot + 1);
    }
    return key;
}

core::Status SettingDefinition::validate(const SettingsValue& value) const {
    switch (type) {
        case SettingType::Bool: {
            if (!value.as_bool().has_value()) {
                return LDDE_STATUS_ERROR(core::ErrorCategory::Settings,
                                         core::ErrorCode::SettingTypeMismatch,
                                         "Expected boolean value for " + key);
            }
            break;
        }
        case SettingType::Int: {
            auto val = value.as_int();
            if (!val.has_value()) {
                return LDDE_STATUS_ERROR(core::ErrorCategory::Settings,
                                         core::ErrorCode::SettingTypeMismatch,
                                         "Expected integer value for " + key);
            }
            if (min_value.has_value() && static_cast<double>(*val) < *min_value) {
                return LDDE_STATUS_ERROR(core::ErrorCategory::Settings,
                                         core::ErrorCode::SettingValidationFailed,
                                         "Value below minimum (" + std::to_string(*min_value) + ") for " + key);
            }
            if (max_value.has_value() && static_cast<double>(*val) > *max_value) {
                return LDDE_STATUS_ERROR(core::ErrorCategory::Settings,
                                         core::ErrorCode::SettingValidationFailed,
                                         "Value exceeds maximum (" + std::to_string(*max_value) + ") for " + key);
            }
            break;
        }
        case SettingType::Double: {
            auto val = value.as_double();
            if (!val.has_value()) {
                return LDDE_STATUS_ERROR(core::ErrorCategory::Settings,
                                         core::ErrorCode::SettingTypeMismatch,
                                         "Expected double value for " + key);
            }
            if (min_value.has_value() && *val < *min_value) {
                return LDDE_STATUS_ERROR(core::ErrorCategory::Settings,
                                         core::ErrorCode::SettingValidationFailed,
                                         "Value below minimum (" + std::to_string(*min_value) + ") for " + key);
            }
            if (max_value.has_value() && *val > *max_value) {
                return LDDE_STATUS_ERROR(core::ErrorCategory::Settings,
                                         core::ErrorCode::SettingValidationFailed,
                                         "Value exceeds maximum (" + std::to_string(*max_value) + ") for " + key);
            }
            break;
        }
        case SettingType::String: {
            auto val = value.as_string();
            if (!val.has_value()) {
                return LDDE_STATUS_ERROR(core::ErrorCategory::Settings,
                                         core::ErrorCode::SettingTypeMismatch,
                                         "Expected string value for " + key);
            }
            // Check for hex colors if applicable
            if (key.find("color") != std::string::npos && !is_valid_hex_color(*val)) {
                return LDDE_STATUS_ERROR(core::ErrorCategory::Settings,
                                         core::ErrorCode::SettingValidationFailed,
                                         "Invalid hex color format for " + key + ": " + *val);
            }
            break;
        }
        case SettingType::Enum: {
            auto val = value.as_string();
            if (!val.has_value()) {
                return LDDE_STATUS_ERROR(core::ErrorCategory::Settings,
                                         core::ErrorCode::SettingTypeMismatch,
                                         "Expected string enum value for " + key);
            }
            std::string lower = to_lower(*val);
            bool found = false;
            for (const auto& ev : enum_values) {
                if (to_lower(ev) == lower) {
                    found = true;
                    break;
                }
            }
            if (!found && !enum_values.empty()) {
                return LDDE_STATUS_ERROR(core::ErrorCategory::Settings,
                                         core::ErrorCode::SettingValidationFailed,
                                         "Value '" + *val + "' not permitted for enum " + key);
            }
            break;
        }
    }
    return core::Status::ok();
}

SettingsSchema::SettingsSchema() = default;

core::Status SettingsSchema::register_setting(SettingDefinition def) {
    if (def.key.empty()) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Settings,
                                 core::ErrorCode::InvalidArgument,
                                 "Setting key cannot be empty");
    }

    std::string lower_key = to_lower(def.key);
    auto it = key_to_index_.find(lower_key);
    if (it != key_to_index_.end()) {
        settings_[it->second] = std::move(def);
        return core::Status::ok();
    }

    size_t index = settings_.size();
    key_to_index_[lower_key] = index;
    settings_.push_back(std::move(def));
    return core::Status::ok();
}

const SettingDefinition* SettingsSchema::find(std::string_view key) const noexcept {
    std::string lower_key = to_lower(key);
    auto it = key_to_index_.find(lower_key);
    if (it != key_to_index_.end() && it->second < settings_.size()) {
        return &settings_[it->second];
    }
    return nullptr;
}

bool SettingsSchema::contains(std::string_view key) const noexcept {
    return find(key) != nullptr;
}

std::vector<const SettingDefinition*> SettingsSchema::settings_in_category(SettingsCategory category) const {
    std::vector<const SettingDefinition*> result;
    for (const auto& def : settings_) {
        if (def.category == category) {
            result.push_back(&def);
        }
    }
    return result;
}

SettingsSchema SettingsSchema::create_default_schema() {
    SettingsSchema s;

    // 1. Appearance
    s.register_setting({
        "appearance.theme_mode",
        SettingsCategory::Appearance,
        SettingType::Enum,
        "Color Scheme",
        "Select light, dark, or system matching color theme",
        SettingsValue("dark"),
        ApplyMode::Immediate,
        "Shell",
        {"dark", "light", "system"},
        std::nullopt, std::nullopt, 1.0,
        {"theme", "dark", "light", "color", "appearance", "style"}
    });

    s.register_setting({
        "desktop.background_color",
        SettingsCategory::Appearance,
        SettingType::String,
        "Desktop Color (Top)",
        "Top gradient wallpaper hex color",
        SettingsValue("#121826"),
        ApplyMode::Immediate,
        "Desktop",
        {},
        std::nullopt, std::nullopt, 1.0,
        {"wallpaper", "background", "color", "hex"}
    });

    s.register_setting({
        "desktop.background_color_bottom",
        SettingsCategory::Appearance,
        SettingType::String,
        "Desktop Color (Bottom)",
        "Bottom gradient wallpaper hex color",
        SettingsValue("#0a0d14"),
        ApplyMode::Immediate,
        "Desktop",
        {},
        std::nullopt, std::nullopt, 1.0,
        {"wallpaper", "background", "color", "hex", "bottom"}
    });

    s.register_setting({
        "appearance.ui_density",
        SettingsCategory::Appearance,
        SettingType::Enum,
        "Interface Density",
        "Spacing density for controls and lists",
        SettingsValue("normal"),
        ApplyMode::Immediate,
        "Shell",
        {"compact", "normal", "comfortable"},
        std::nullopt, std::nullopt, 1.0,
        {"density", "padding", "compact", "spacing"}
    });

    s.register_setting({
        "appearance.accent_color",
        SettingsCategory::Appearance,
        SettingType::String,
        "Accent Highlight",
        "Primary brand accent color for toggles and highlights",
        SettingsValue("#4a90e2"),
        ApplyMode::Immediate,
        "Shell",
        {},
        std::nullopt, std::nullopt, 1.0,
        {"accent", "color", "highlight", "brand", "blue"}
    });

    // 2. Display
    s.register_setting({
        "display.scale_factor",
        SettingsCategory::Display,
        SettingType::Double,
        "Display Scale",
        "UI scale factor multiplier (1.0 = standard 100%)",
        SettingsValue(1.0),
        ApplyMode::Immediate,
        "Display",
        {},
        0.5, 3.0, 0.25,
        {"scale", "zoom", "dpi", "size", "resolution", "display"}
    });

    s.register_setting({
        "display.orientation_policy",
        SettingsCategory::Display,
        SettingType::Enum,
        "Orientation Policy",
        "Screen orientation adaptation rule",
        SettingsValue("auto"),
        ApplyMode::Immediate,
        "Display",
        {"auto", "portrait", "landscape"},
        std::nullopt, std::nullopt, 1.0,
        {"orientation", "rotate", "rotation", "portrait", "landscape"}
    });

    // 3. Windows
    s.register_setting({
        "window.default_mode",
        SettingsCategory::Windows,
        SettingType::Enum,
        "Default Window Mode",
        "Initial state for newly opened application windows",
        SettingsValue("normal"),
        ApplyMode::Immediate,
        "WindowManager",
        {"normal", "maximized", "fullscreen"},
        std::nullopt, std::nullopt, 1.0,
        {"window", "maximize", "fullscreen", "size", "launch"}
    });

    s.register_setting({
        "window.cascade_step",
        SettingsCategory::Windows,
        SettingType::Int,
        "Cascade Offset",
        "Pixel offset when cascading multiple windows",
        SettingsValue(32),
        ApplyMode::Immediate,
        "WindowManager",
        {},
        0.0, 64.0, 8.0,
        {"cascade", "placement", "offset", "step"}
    });

    s.register_setting({
        "window.margin_top",
        SettingsCategory::Windows,
        SettingType::Int,
        "Top Margin",
        "Work area top boundary padding in pixels",
        SettingsValue(8),
        ApplyMode::Immediate,
        "WindowManager",
        {},
        0.0, 32.0, 4.0,
        {"margin", "padding", "top", "boundary"}
    });

    s.register_setting({
        "window.margin_horizontal",
        SettingsCategory::Windows,
        SettingType::Int,
        "Horizontal Margin",
        "Work area left and right boundary padding",
        SettingsValue(8),
        ApplyMode::Immediate,
        "WindowManager",
        {},
        0.0, 32.0, 4.0,
        {"margin", "padding", "horizontal", "sides"}
    });

    // 4. Desktop
    s.register_setting({
        "desktop.background_mode",
        SettingsCategory::Desktop,
        SettingType::Enum,
        "Background Style",
        "Desktop background rendering mode",
        SettingsValue("gradient"),
        ApplyMode::Immediate,
        "Desktop",
        {"gradient", "solid"},
        std::nullopt, std::nullopt, 1.0,
        {"desktop", "background", "wallpaper", "gradient", "solid"}
    });

    s.register_setting({
        "desktop.ambient_glow",
        SettingsCategory::Desktop,
        SettingType::Bool,
        "Ambient Wallpaper Glow",
        "Radial ambient light glow behind desktop surface",
        SettingsValue(true),
        ApplyMode::Immediate,
        "Desktop",
        {},
        std::nullopt, std::nullopt, 1.0,
        {"glow", "ambient", "light", "wallpaper"}
    });

    s.register_setting({
        "desktop.show_empty_hint",
        SettingsCategory::Desktop,
        SettingType::Bool,
        "Empty Desktop Watermark",
        "Display subtle desktop branding watermark when idle",
        SettingsValue(true),
        ApplyMode::Immediate,
        "Desktop",
        {},
        std::nullopt, std::nullopt, 1.0,
        {"hint", "watermark", "branding", "logo"}
    });

    // 5. Dock
    s.register_setting({
        "dock.enabled",
        SettingsCategory::Dock,
        SettingType::Bool,
        "Enable Dock",
        "Show floating application dock at screen edge",
        SettingsValue(true),
        ApplyMode::Immediate,
        "Dock",
        {},
        std::nullopt, std::nullopt, 1.0,
        {"dock", "bar", "taskbar", "apps", "enable"}
    });

    s.register_setting({
        "dock.position",
        SettingsCategory::Dock,
        SettingType::Enum,
        "Dock Position",
        "Screen edge anchoring position for dock",
        SettingsValue("bottom"),
        ApplyMode::Immediate,
        "Dock",
        {"bottom", "top", "left", "right"},
        std::nullopt, std::nullopt, 1.0,
        {"dock", "position", "bottom", "top", "edge"}
    });

    s.register_setting({
        "dock.visibility",
        SettingsCategory::Dock,
        SettingType::Enum,
        "Dock Visibility",
        "Persistent display or automatic hiding behavior",
        SettingsValue("visible"),
        ApplyMode::Immediate,
        "Dock",
        {"visible", "autohide"},
        std::nullopt, std::nullopt, 1.0,
        {"dock", "autohide", "hide", "visibility"}
    });

    s.register_setting({
        "dock.item_size",
        SettingsCategory::Dock,
        SettingType::Int,
        "Dock Icon Size",
        "Size of application icons in the dock (dp)",
        SettingsValue(48),
        ApplyMode::Immediate,
        "Dock",
        {},
        32.0, 96.0, 4.0,
        {"dock", "icon", "size", "pixels"}
    });

    // 6. Launcher
    s.register_setting({
        "launcher.default_view",
        SettingsCategory::Launcher,
        SettingType::Enum,
        "Launcher Layout",
        "Application presentation style in launcher",
        SettingsValue("grid"),
        ApplyMode::Immediate,
        "Launcher",
        {"grid", "list"},
        std::nullopt, std::nullopt, 1.0,
        {"launcher", "apps", "grid", "list", "view"}
    });

    s.register_setting({
        "launcher.show_categories",
        SettingsCategory::Launcher,
        SettingType::Bool,
        "Show Category Tabs",
        "Display category navigation bar in launcher",
        SettingsValue(true),
        ApplyMode::Immediate,
        "Launcher",
        {},
        std::nullopt, std::nullopt, 1.0,
        {"launcher", "categories", "tabs", "filter"}
    });

    s.register_setting({
        "launcher.search_enabled",
        SettingsCategory::Launcher,
        SettingType::Bool,
        "Enable App Search",
        "Show search input box at top of launcher",
        SettingsValue(true),
        ApplyMode::Immediate,
        "Launcher",
        {},
        std::nullopt, std::nullopt, 1.0,
        {"launcher", "search", "filter", "query"}
    });

    // 7. Input
    s.register_setting({
        "input.tap_to_click",
        SettingsCategory::Input,
        SettingType::Bool,
        "Tap to Click",
        "Enable touch contact tap for mouse button clicks",
        SettingsValue(true),
        ApplyMode::Immediate,
        "Input",
        {},
        std::nullopt, std::nullopt, 1.0,
        {"touch", "tap", "click", "input", "mouse"}
    });

    s.register_setting({
        "input.touch_enabled",
        SettingsCategory::Input,
        SettingType::Bool,
        "Touch Gestures",
        "Enable multi-touch window manipulation gestures",
        SettingsValue(true),
        ApplyMode::Immediate,
        "Input",
        {},
        std::nullopt, std::nullopt, 1.0,
        {"touch", "gesture", "drag", "resize"}
    });

    s.register_setting({
        "input.touch_move_threshold",
        SettingsCategory::Input,
        SettingType::Int,
        "Drag Sensitivity",
        "Minimum pixels before a drag gesture engages",
        SettingsValue(10),
        ApplyMode::Immediate,
        "Input",
        {},
        2.0, 50.0, 2.0,
        {"sensitivity", "threshold", "drag", "touch"}
    });

    s.register_setting({
        "input.touch_double_tap_timeout",
        SettingsCategory::Input,
        SettingType::Int,
        "Double-Tap Timeout (ms)",
        "Maximum milliseconds between taps for double-tap",
        SettingsValue(350),
        ApplyMode::Immediate,
        "Input",
        {},
        150.0, 800.0, 50.0,
        {"double-tap", "timeout", "speed", "touch"}
    });

    // 8. Notifications
    s.register_setting({
        "notifications.enabled",
        SettingsCategory::Notifications,
        SettingType::Bool,
        "Enable Notifications",
        "Allow desktop applications to show notification banners",
        SettingsValue(true),
        ApplyMode::Immediate,
        "NotificationManager",
        {},
        std::nullopt, std::nullopt, 1.0,
        {"notification", "toast", "banner", "alerts"}
    });

    s.register_setting({
        "notifications.popup_duration_ms",
        SettingsCategory::Notifications,
        SettingType::Int,
        "Popup Duration (ms)",
        "Toast banner display time before auto-dismissal",
        SettingsValue(5000),
        ApplyMode::Immediate,
        "NotificationManager",
        {},
        1000.0, 30000.0, 1000.0,
        {"duration", "timeout", "toast", "dismiss", "seconds"}
    });

    s.register_setting({
        "notifications.max_visible_popups",
        SettingsCategory::Notifications,
        SettingType::Int,
        "Max Concurrent Popups",
        "Maximum visible toast banners stacked at one time",
        SettingsValue(3),
        ApplyMode::Immediate,
        "NotificationManager",
        {},
        1.0, 5.0, 1.0,
        {"stack", "count", "popups", "limit"}
    });

    s.register_setting({
        "notifications.grouping_enabled",
        SettingsCategory::Notifications,
        SettingType::Bool,
        "Group by Application",
        "Group notification items by source application",
        SettingsValue(true),
        ApplyMode::Immediate,
        "NotificationManager",
        {},
        std::nullopt, std::nullopt, 1.0,
        {"group", "stack", "bundle", "notifications"}
    });

    // 9. System UI
    s.register_setting({
        "system.clock_format",
        SettingsCategory::SystemUI,
        SettingType::Enum,
        "Clock Time Format",
        "Format for the status bar clock display",
        SettingsValue("24h"),
        ApplyMode::Immediate,
        "SystemUI",
        {"12h", "24h"},
        std::nullopt, std::nullopt, 1.0,
        {"clock", "time", "12h", "24h", "hour"}
    });

    s.register_setting({
        "system.show_seconds",
        SettingsCategory::SystemUI,
        SettingType::Bool,
        "Show Seconds",
        "Include seconds readout on status bar clock",
        SettingsValue(false),
        ApplyMode::Immediate,
        "SystemUI",
        {},
        std::nullopt, std::nullopt, 1.0,
        {"seconds", "time", "clock"}
    });

    s.register_setting({
        "system.show_network",
        SettingsCategory::SystemUI,
        SettingType::Bool,
        "Network Status Icon",
        "Show WiFi and Ethernet connectivity indicator",
        SettingsValue(true),
        ApplyMode::Immediate,
        "SystemUI",
        {},
        std::nullopt, std::nullopt, 1.0,
        {"network", "wifi", "ethernet", "status", "internet"}
    });

    s.register_setting({
        "system.show_audio",
        SettingsCategory::SystemUI,
        SettingType::Bool,
        "Audio Status Icon",
        "Show volume level and mute indicator in status bar",
        SettingsValue(true),
        ApplyMode::Immediate,
        "SystemUI",
        {},
        std::nullopt, std::nullopt, 1.0,
        {"audio", "sound", "volume", "mute"}
    });

    s.register_setting({
        "system.show_battery",
        SettingsCategory::SystemUI,
        SettingType::Bool,
        "Battery Status Icon",
        "Show battery level percentage and charging state",
        SettingsValue(true),
        ApplyMode::Immediate,
        "SystemUI",
        {},
        std::nullopt, std::nullopt, 1.0,
        {"battery", "power", "charging", "percentage"}
    });

    s.register_setting({
        "system.quick_controls_enabled",
        SettingsCategory::SystemUI,
        SettingType::Bool,
        "Quick Controls Panel",
        "Enable drop-down quick controls drawer from status bar",
        SettingsValue(true),
        ApplyMode::Immediate,
        "SystemUI",
        {},
        std::nullopt, std::nullopt, 1.0,
        {"quick", "controls", "panel", "drawer", "toggles"}
    });

    // 10. About
    s.register_setting({
        "about.system_name",
        SettingsCategory::About,
        SettingType::String,
        "Environment",
        "LinuxDroid Desktop Environment",
        SettingsValue("LDDE"),
        ApplyMode::Immediate,
        "Core",
        {},
        std::nullopt, std::nullopt, 1.0,
        {"about", "system", "version", "ldde", "linuxdroid"}
    });

    s.register_setting({
        "about.version",
        SettingsCategory::About,
        SettingType::String,
        "Version",
        "Installed release version",
        SettingsValue("1.0.0"),
        ApplyMode::Immediate,
        "Core",
        {},
        std::nullopt, std::nullopt, 1.0,
        {"version", "release", "build"}
    });

    return s;
}

} // namespace ldde::settings
