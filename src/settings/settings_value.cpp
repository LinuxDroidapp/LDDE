#include "ldde/settings/settings_value.hpp"
#include <sstream>
#include <iomanip>
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

} // namespace

SettingType SettingsValue::type() const noexcept {
    if (std::holds_alternative<bool>(value_)) return SettingType::Bool;
    if (std::holds_alternative<int64_t>(value_)) return SettingType::Int;
    if (std::holds_alternative<double>(value_)) return SettingType::Double;
    return SettingType::String;
}

std::optional<bool> SettingsValue::as_bool() const noexcept {
    if (auto* b = std::get_if<bool>(&value_)) {
        return *b;
    }
    if (auto* s = std::get_if<std::string>(&value_)) {
        std::string lower = to_lower(*s);
        if (lower == "true" || lower == "1" || lower == "yes" || lower == "on") return true;
        if (lower == "false" || lower == "0" || lower == "no" || lower == "off") return false;
    }
    if (auto* i = std::get_if<int64_t>(&value_)) {
        return *i != 0;
    }
    return std::nullopt;
}

std::optional<int64_t> SettingsValue::as_int() const noexcept {
    if (auto* i = std::get_if<int64_t>(&value_)) {
        return *i;
    }
    if (auto* d = std::get_if<double>(&value_)) {
        return static_cast<int64_t>(*d);
    }
    if (auto* b = std::get_if<bool>(&value_)) {
        return *b ? 1 : 0;
    }
    if (auto* s = std::get_if<std::string>(&value_)) {
        try {
            size_t idx = 0;
            int64_t v = std::stoll(*s, &idx);
            if (idx == s->size()) return v;
        } catch (...) {}
    }
    return std::nullopt;
}

std::optional<double> SettingsValue::as_double() const noexcept {
    if (auto* d = std::get_if<double>(&value_)) {
        return *d;
    }
    if (auto* i = std::get_if<int64_t>(&value_)) {
        return static_cast<double>(*i);
    }
    if (auto* s = std::get_if<std::string>(&value_)) {
        try {
            size_t idx = 0;
            double v = std::stod(*s, &idx);
            if (idx == s->size()) return v;
        } catch (...) {}
    }
    return std::nullopt;
}

std::optional<std::string> SettingsValue::as_string() const noexcept {
    if (auto* s = std::get_if<std::string>(&value_)) {
        return *s;
    }
    return to_string();
}

std::string SettingsValue::to_string() const {
    if (auto* b = std::get_if<bool>(&value_)) {
        return *b ? "true" : "false";
    }
    if (auto* i = std::get_if<int64_t>(&value_)) {
        return std::to_string(*i);
    }
    if (auto* d = std::get_if<double>(&value_)) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << *d;
        std::string s = ss.str();
        // Trim trailing zeros after decimal point
        if (s.find('.') != std::string::npos) {
            while (!s.empty() && s.back() == '0') s.pop_back();
            if (!s.empty() && s.back() == '.') s.pop_back();
        }
        return s;
    }
    if (auto* s = std::get_if<std::string>(&value_)) {
        return *s;
    }
    return "";
}

std::optional<SettingsValue> SettingsValue::from_string(SettingType type, std::string_view str) {
    switch (type) {
        case SettingType::Bool: {
            std::string lower = to_lower(str);
            if (lower == "true" || lower == "1" || lower == "yes" || lower == "on") {
                return SettingsValue(true);
            }
            if (lower == "false" || lower == "0" || lower == "no" || lower == "off") {
                return SettingsValue(false);
            }
            return std::nullopt;
        }
        case SettingType::Int: {
            try {
                size_t idx = 0;
                std::string s(str);
                int64_t v = std::stoll(s, &idx);
                if (idx == s.size()) return SettingsValue(v);
            } catch (...) {}
            return std::nullopt;
        }
        case SettingType::Double: {
            try {
                size_t idx = 0;
                std::string s(str);
                double v = std::stod(s, &idx);
                if (idx == s.size()) return SettingsValue(v);
            } catch (...) {}
            return std::nullopt;
        }
        case SettingType::String:
        case SettingType::Enum:
            return SettingsValue(std::string(str));
    }
    return std::nullopt;
}

bool SettingsValue::operator==(const SettingsValue& other) const noexcept {
    return value_ == other.value_;
}

} // namespace ldde::settings
