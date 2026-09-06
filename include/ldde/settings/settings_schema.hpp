#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <optional>
#include "ldde/core/error.hpp"
#include "ldde/settings/settings_types.hpp"
#include "ldde/settings/settings_value.hpp"

namespace ldde::settings {

struct SettingDefinition {
    std::string key; // "section.key" e.g. "dock.position"
    SettingsCategory category = SettingsCategory::Appearance;
    SettingType type = SettingType::String;
    std::string title;
    std::string description;
    SettingsValue default_value;
    ApplyMode apply_mode = ApplyMode::Immediate;
    std::string owning_subsystem;
    std::vector<std::string> enum_values;
    std::optional<double> min_value;
    std::optional<double> max_value;
    double step_value = 1.0;
    std::vector<std::string> keywords;

    [[nodiscard]] std::string section() const;
    [[nodiscard]] std::string property() const;
    [[nodiscard]] core::Status validate(const SettingsValue& value) const;
};

class SettingsSchema {
public:
    SettingsSchema();
    ~SettingsSchema() = default;

    core::Status register_setting(SettingDefinition def);

    [[nodiscard]] const SettingDefinition* find(std::string_view key) const noexcept;
    [[nodiscard]] bool contains(std::string_view key) const noexcept;

    [[nodiscard]] std::vector<const SettingDefinition*> settings_in_category(SettingsCategory category) const;
    [[nodiscard]] const std::vector<SettingDefinition>& all_settings() const noexcept { return settings_; }
    [[nodiscard]] size_t count() const noexcept { return settings_.size(); }

    static SettingsSchema create_default_schema();

private:
    std::vector<SettingDefinition> settings_;
    std::unordered_map<std::string, size_t> key_to_index_;
};

} // namespace ldde::settings
