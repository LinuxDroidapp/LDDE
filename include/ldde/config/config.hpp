#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <optional>
#include <vector>
#include "ldde/core/error.hpp"

namespace ldde::config {

using core::Status;

inline constexpr std::string_view kSystemConfigPath = "/etc/linuxdroid/desktop.conf";
inline constexpr std::string_view kUserConfigRelativePath = "linuxdroid/desktop.conf";
inline constexpr int kCurrentConfigVersion = 1;

class Config {
public:
    Config();
    ~Config() = default;

    void load_defaults();

    Status load_file(const std::string& filepath);

    Status load_with_precedence(const std::optional<std::string>& custom_path = std::nullopt);

    void set(const std::string& section, const std::string& key, const std::string& value);

    [[nodiscard]] std::optional<std::string> get_string(const std::string& section,
                                                        const std::string& key) const;
    [[nodiscard]] std::string get_string_or(const std::string& section,
                                            const std::string& key,
                                            std::string_view default_val) const;

    [[nodiscard]] std::optional<int64_t> get_int(const std::string& section,
                                                 const std::string& key) const;
    [[nodiscard]] int64_t get_int_or(const std::string& section,
                                     const std::string& key,
                                     int64_t default_val) const;

    [[nodiscard]] std::optional<double> get_double(const std::string& section,
                                                   const std::string& key) const;
    [[nodiscard]] double get_double_or(const std::string& section,
                                       const std::string& key,
                                       double default_val) const;

    [[nodiscard]] std::optional<bool> get_bool(const std::string& section,
                                               const std::string& key) const;
    [[nodiscard]] bool get_bool_or(const std::string& section,
                                   const std::string& key,
                                   bool default_val) const;

    [[nodiscard]] int version() const;
    [[nodiscard]] Status validate() const;

    Status save_to_file(const std::string& filepath) const;

    [[nodiscard]] std::vector<std::string> sections() const;
    [[nodiscard]] std::vector<std::string> section_keys(const std::string& section) const;
    [[nodiscard]] bool has_section(const std::string& section) const;
    [[nodiscard]] bool has_key(const std::string& section, const std::string& key) const;

    [[nodiscard]] const std::vector<std::string>& loaded_sources() const noexcept {
        return loaded_sources_;
    }

    [[nodiscard]] static std::string default_user_config_path();

private:
    // section -> (key -> value)
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> values_;
    std::vector<std::string> loaded_sources_;

    Status parse_stream(std::istream& is, const std::string& source_name);
};

} // namespace ldde::config
