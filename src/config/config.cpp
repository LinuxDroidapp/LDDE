#include "ldde/config/config.hpp"
#include "ldde/core/logging.hpp"
#include "ldde/core/types.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sys/stat.h>

namespace ldde::config {

namespace {

std::string trim(std::string_view sv) {
    auto start = sv.begin();
    while (start != sv.end() && std::isspace(static_cast<unsigned char>(*start))) {
        ++start;
    }
    auto end = sv.end();
    while (end != start && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    return std::string(start, end);
}

std::string to_lower(std::string_view sv) {
    std::string result;
    result.reserve(sv.size());
    for (char c : sv) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return result;
}

bool file_exists(const std::string& path) {
    struct stat st{};
    return (stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode));
}

} // namespace

Config::Config() {
    load_defaults();
}

void Config::load_defaults() {
    values_.clear();
    loaded_sources_.clear();

    set("general", "config_version", std::to_string(kCurrentConfigVersion));
    set("general", "desktop_name", std::string(core::kDefaultDesktopName));
    set("logging", "level", "INFO");
    set("display", "scale_factor", "1.0");
    set("input", "tap_to_click", "true");

    loaded_sources_.emplace_back("<defaults>");
}

std::string Config::default_user_config_path() {
    const char* xdg_config_home = std::getenv("XDG_CONFIG_HOME");
    if (xdg_config_home && *xdg_config_home) {
        return std::string(xdg_config_home) + "/" + std::string(kUserConfigRelativePath);
    }
    const char* home = std::getenv("HOME");
    if (home && *home) {
        return std::string(home) + "/.config/" + std::string(kUserConfigRelativePath);
    }
    return "";
}

Status Config::parse_stream(std::istream& is, const std::string& source_name) {
    std::string line;
    std::string current_section = "general";
    size_t line_num = 0;

    while (std::getline(is, line)) {
        ++line_num;
        std::string trimmed = trim(line);

        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            std::string section_name = trim(trimmed.substr(1, trimmed.size() - 2));
            if (section_name.empty()) {
                return LDDE_STATUS_ERROR(
                    core::ErrorCategory::Configuration,
                    core::ErrorCode::ConfigParseError,
                    "Empty section name in " + source_name + ":" + std::to_string(line_num));
            }
            current_section = to_lower(section_name);
            continue;
        }

        auto eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos) {
            return LDDE_STATUS_ERROR(
                core::ErrorCategory::Configuration,
                core::ErrorCode::ConfigParseError,
                "Malformed config line (missing '=') in " + source_name + ":" + std::to_string(line_num));
        }

        std::string key = to_lower(trim(trimmed.substr(0, eq_pos)));
        std::string value = trim(trimmed.substr(eq_pos + 1));

        if (key.empty()) {
            return LDDE_STATUS_ERROR(
                core::ErrorCategory::Configuration,
                core::ErrorCode::ConfigParseError,
                "Empty key in " + source_name + ":" + std::to_string(line_num));
        }

        set(current_section, key, value);
    }

    return Status::ok();
}

Status Config::load_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Configuration,
                                 core::ErrorCode::ConfigNotFound,
                                 "Cannot open config file: " + filepath);
    }

    Status s = parse_stream(file, filepath);
    if (s.is_ok()) {
        loaded_sources_.push_back(filepath);
    }
    return s;
}

Status Config::load_with_precedence(const std::optional<std::string>& custom_path) {
    load_defaults();

    if (custom_path.has_value()) {
        Status s = load_file(custom_path.value());
        if (s.is_error()) {
            return s;
        }
        return validate();
    }

    // 1. System config: /etc/linuxdroid/desktop.conf
    std::string sys_path = std::string(kSystemConfigPath);
    if (file_exists(sys_path)) {
        Status s = load_file(sys_path);
        if (s.is_error()) {
            LDDE_LOG_WARN(Config, "Error parsing system config " << sys_path << ": " << s.to_string());
        } else {
            LDDE_LOG_DEBUG(Config, "Loaded system config: " << sys_path);
        }
    }

    // 2. User config: ~/.config/linuxdroid/desktop.conf
    std::string user_path = default_user_config_path();
    if (!user_path.empty() && file_exists(user_path)) {
        Status s = load_file(user_path);
        if (s.is_error()) {
            LDDE_LOG_WARN(Config, "Error parsing user config " << user_path << ": " << s.to_string());
        } else {
            LDDE_LOG_DEBUG(Config, "Loaded user config: " << user_path);
        }
    }

    return validate();
}

void Config::set(const std::string& section, const std::string& key, const std::string& value) {
    values_[to_lower(section)][to_lower(key)] = value;
}

std::optional<std::string> Config::get_string(const std::string& section,
                                              const std::string& key) const {
    auto sec_it = values_.find(to_lower(section));
    if (sec_it == values_.end()) {
        return std::nullopt;
    }
    auto key_it = sec_it->second.find(to_lower(key));
    if (key_it == sec_it->second.end()) {
        return std::nullopt;
    }
    return key_it->second;
}

std::string Config::get_string_or(const std::string& section,
                                  const std::string& key,
                                  std::string_view default_val) const {
    auto opt = get_string(section, key);
    return opt.value_or(std::string(default_val));
}

std::optional<int64_t> Config::get_int(const std::string& section,
                                       const std::string& key) const {
    auto opt = get_string(section, key);
    if (!opt.has_value()) {
        return std::nullopt;
    }
    try {
        size_t idx = 0;
        int64_t val = std::stoll(opt.value(), &idx);
        if (idx == opt.value().size()) {
            return val;
        }
    } catch (...) {}
    return std::nullopt;
}

int64_t Config::get_int_or(const std::string& section,
                           const std::string& key,
                           int64_t default_val) const {
    auto opt = get_int(section, key);
    return opt.value_or(default_val);
}

std::optional<double> Config::get_double(const std::string& section,
                                         const std::string& key) const {
    auto opt = get_string(section, key);
    if (!opt.has_value()) {
        return std::nullopt;
    }
    try {
        size_t idx = 0;
        double val = std::stod(opt.value(), &idx);
        if (idx == opt.value().size()) {
            return val;
        }
    } catch (...) {}
    return std::nullopt;
}

double Config::get_double_or(const std::string& section,
                             const std::string& key,
                             double default_val) const {
    auto opt = get_double(section, key);
    return opt.value_or(default_val);
}

std::optional<bool> Config::get_bool(const std::string& section,
                                     const std::string& key) const {
    auto opt = get_string(section, key);
    if (!opt.has_value()) {
        return std::nullopt;
    }
    std::string lower = to_lower(opt.value());
    if (lower == "true" || lower == "1" || lower == "yes" || lower == "on") {
        return true;
    }
    if (lower == "false" || lower == "0" || lower == "no" || lower == "off") {
        return false;
    }
    return std::nullopt;
}

bool Config::get_bool_or(const std::string& section,
                         const std::string& key,
                         bool default_val) const {
    auto opt = get_bool(section, key);
    return opt.value_or(default_val);
}

int Config::version() const {
    return static_cast<int>(get_int_or("general", "config_version", 0));
}

Status Config::validate() const {
    int v = version();
    if (v <= 0) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Configuration,
                                 core::ErrorCode::ConfigValidationFailed,
                                 "Missing or invalid 'config_version' in configuration");
    }

    if (v > kCurrentConfigVersion) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Configuration,
                                 core::ErrorCode::ConfigVersionMismatch,
                                 "Unsupported config_version " + std::to_string(v) +
                                 " (max supported is " + std::to_string(kCurrentConfigVersion) + ")");
    }

    double scale = get_double_or("display", "scale_factor", 1.0);
    if (scale <= 0.0 || scale > 10.0) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Configuration,
                                 core::ErrorCode::ConfigValidationFailed,
                                 "Invalid display.scale_factor: " + std::to_string(scale));
    }

    return Status::ok();
}

} // namespace ldde::config

