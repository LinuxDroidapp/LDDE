#pragma once

#include <string>
#include <string_view>
#include <variant>
#include <optional>
#include <cstdint>
#include "ldde/settings/settings_types.hpp"

namespace ldde::settings {

class SettingsValue {
public:
    using VariantType = std::variant<bool, int64_t, double, std::string>;

    SettingsValue() : value_(false) {}
    SettingsValue(bool v) : value_(v) {}
    SettingsValue(int v) : value_(static_cast<int64_t>(v)) {}
    SettingsValue(int64_t v) : value_(v) {}
    SettingsValue(double v) : value_(v) {}
    SettingsValue(std::string v) : value_(std::move(v)) {}
    SettingsValue(std::string_view v) : value_(std::string(v)) {}
    SettingsValue(const char* v) : value_(std::string(v ? v : "")) {}

    [[nodiscard]] SettingType type() const noexcept;

    [[nodiscard]] bool is_bool() const noexcept { return std::holds_alternative<bool>(value_); }
    [[nodiscard]] bool is_int() const noexcept { return std::holds_alternative<int64_t>(value_); }
    [[nodiscard]] bool is_double() const noexcept { return std::holds_alternative<double>(value_); }
    [[nodiscard]] bool is_string() const noexcept { return std::holds_alternative<std::string>(value_); }

    [[nodiscard]] std::optional<bool> as_bool() const noexcept;
    [[nodiscard]] std::optional<int64_t> as_int() const noexcept;
    [[nodiscard]] std::optional<double> as_double() const noexcept;
    [[nodiscard]] std::optional<std::string> as_string() const noexcept;

    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] static std::optional<SettingsValue> from_string(SettingType type, std::string_view str);

    [[nodiscard]] bool operator==(const SettingsValue& other) const noexcept;
    [[nodiscard]] bool operator!=(const SettingsValue& other) const noexcept { return !(*this == other); }

    [[nodiscard]] const VariantType& raw_variant() const noexcept { return value_; }

private:
    VariantType value_;
};

} // namespace ldde::settings
