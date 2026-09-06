#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <optional>

namespace ldde::application {

struct DesktopKey {
    std::string name;
    std::string locale; // empty if unlocalized
};

class DesktopEntry {
public:
    DesktopEntry() = default;

    void set_value(std::string group, std::string key, std::string locale, std::string value);

    [[nodiscard]] bool has_group(std::string_view group) const noexcept;
    [[nodiscard]] std::vector<std::string> groups() const;

    [[nodiscard]] bool has_key(std::string_view group, std::string_view key) const noexcept;
    
    [[nodiscard]] std::string get_string(
        std::string_view group, std::string_view key, std::string_view default_value = {}) const;

    [[nodiscard]] std::string get_localized_string(
        std::string_view group, std::string_view key, std::string_view locale, std::string_view default_value = {}) const;

    [[nodiscard]] bool get_bool(
        std::string_view group, std::string_view key, bool default_value = false) const noexcept;

    [[nodiscard]] std::vector<std::string> get_string_list(
        std::string_view group, std::string_view key) const;

    [[nodiscard]] std::vector<std::string> actions() const;

    [[nodiscard]] bool is_valid_application() const noexcept;

private:
    // group_name -> (key_name -> (locale -> value))
    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, std::string>>> data_;
};

} // namespace ldde::application

