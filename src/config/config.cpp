#include "ldde/config/config.hpp"
#include "ldde/core/logging.hpp"
#include "ldde/core/types.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <unordered_set>
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
    set("input", "touch_enabled", "true");
    set("input", "touch_move_threshold", "10");
    set("input", "touch_double_tap_timeout", "350");
    set("input", "touch_double_tap_distance", "16");
    set("input", "touch_resize_target", "28");
    set("application", "desktop_identity", "LinuxDroid");
    set("application", "watch_filesystem", "true");
    set("application", "system_paths", "/usr/local/share/applications:/usr/share/applications");
    set("application", "user_path", "~/.local/share/applications");
    set("launcher", "default_view", "grid");
    set("launcher", "grid_min_item_width", "80");
    set("launcher", "show_categories", "true");
    set("launcher", "search_enabled", "true");
    set("launcher", "terminal_emulator", "x-terminal-emulator");
    set("dock", "enabled", "true");
    set("dock", "position", "bottom");
    set("dock", "visibility", "visible");
    set("dock", "pinned", "");
    set("dock", "item_size", "48");
    set("dock", "spacing", "8");
    set("switcher", "enabled", "true");
    set("switcher", "presentation", "application");
    set("switcher", "mru", "true");
    set("switcher", "show_window_count", "true");
    set("switcher", "show_window_titles", "true");
    set("desktop", "background", "default");
    set("desktop", "background_mode", "gradient");
    set("desktop", "background_color", "#121826");
    set("desktop", "background_color_bottom", "#0a0d14");
    set("desktop", "ambient_glow", "true");
    set("desktop", "show_empty_hint", "true");
    set("system", "clock_format", "24h");
    set("system", "show_seconds", "false");
    set("system", "show_network", "true");
    set("system", "show_audio", "true");
    set("system", "show_battery", "true");
    set("system", "status_bar_enabled", "true");
    set("system", "quick_controls_enabled", "true");
    set("notifications", "enabled", "true");
    set("notifications", "popup_duration_ms", "5000");
    set("notifications", "max_visible_popups", "3");
    set("notifications", "max_history_entries", "50");
    set("notifications", "grouping_enabled", "true");
    set("notifications", "critical_persistent", "true");

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

Status Config::save_to_file(const std::string& filepath) const {
    if (filepath.empty()) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Configuration,
                                 core::ErrorCode::InvalidArgument,
                                 "Config file path is empty");
    }

    // 1. Create directory if not exists
    auto slash_pos = filepath.rfind('/');
    if (slash_pos != std::string::npos) {
        std::string dir = filepath.substr(0, slash_pos);
        if (!dir.empty()) {
            std::string current_path;
            std::stringstream ss(dir);
            std::string part;
            if (dir[0] == '/') {
                current_path = "/";
            }
            while (std::getline(ss, part, '/')) {
                if (part.empty()) continue;
                if (!current_path.empty() && current_path.back() != '/') {
                    current_path += '/';
                }
                current_path += part;
                struct stat st{};
                if (stat(current_path.c_str(), &st) != 0) {
                    if (mkdir(current_path.c_str(), 0755) != 0 && errno != EEXIST) {
                        return LDDE_STATUS_ERROR(core::ErrorCategory::Configuration,
                                                 core::ErrorCode::IoError,
                                                 "Failed to create directory " + current_path + ": " + std::strerror(errno));
                    }
                }
            }
        }
    }

    // 2. Open temporary file in the same directory for atomic rename
    std::string temp_template = filepath + ".tmp.XXXXXX";
    std::vector<char> temp_path_buf(temp_template.begin(), temp_template.end());
    temp_path_buf.push_back('\0');

    int fd = mkstemp(temp_path_buf.data());
    if (fd < 0) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Configuration,
                                 core::ErrorCode::IoError,
                                 "Failed to create temporary config file: " + std::string(std::strerror(errno)));
    }
    std::string temp_filepath(temp_path_buf.data());

    // 3. Write INI format
    std::stringstream ss;
    ss << "# LinuxDroid Desktop Environment (LDDE) Configuration File\n\n";

    // Standard section order if available
    std::vector<std::string> standard_sections = {
        "general", "logging", "display", "input", "application",
        "launcher", "dock", "switcher", "desktop", "system",
        "notifications", "window"
    };

    std::unordered_set<std::string> written_sections;
    for (const auto& sec : standard_sections) {
        auto it = values_.find(sec);
        if (it != values_.end()) {
            ss << "[" << sec << "]\n";
            std::vector<std::pair<std::string, std::string>> pairs(it->second.begin(), it->second.end());
            std::sort(pairs.begin(), pairs.end());
            for (const auto& [k, v] : pairs) {
                ss << k << " = " << v << "\n";
            }
            ss << "\n";
            written_sections.insert(sec);
        }
    }

    // Any remaining sections
    std::vector<std::string> other_sections;
    for (const auto& [sec, _] : values_) {
        if (written_sections.find(sec) == written_sections.end()) {
            other_sections.push_back(sec);
        }
    }
    std::sort(other_sections.begin(), other_sections.end());
    for (const auto& sec : other_sections) {
        ss << "[" << sec << "]\n";
        auto it = values_.find(sec);
        std::vector<std::pair<std::string, std::string>> pairs(it->second.begin(), it->second.end());
        std::sort(pairs.begin(), pairs.end());
        for (const auto& [k, v] : pairs) {
            ss << k << " = " << v << "\n";
        }
        ss << "\n";
    }

    std::string content = ss.str();
    ssize_t written = write(fd, content.data(), content.size());
    if (written < 0 || static_cast<size_t>(written) != content.size()) {
        close(fd);
        unlink(temp_filepath.c_str());
        return LDDE_STATUS_ERROR(core::ErrorCategory::Configuration,
                                 core::ErrorCode::IoError,
                                 "Failed to write full config to temp file: " + std::string(std::strerror(errno)));
    }

    // fsync to ensure physical write to storage
    if (fsync(fd) != 0) {
        close(fd);
        unlink(temp_filepath.c_str());
        return LDDE_STATUS_ERROR(core::ErrorCategory::Configuration,
                                 core::ErrorCode::IoError,
                                 "Failed to fsync config file: " + std::string(std::strerror(errno)));
    }

    close(fd);

    // 4. Atomic rename
    if (rename(temp_filepath.c_str(), filepath.c_str()) != 0) {
        unlink(temp_filepath.c_str());
        return LDDE_STATUS_ERROR(core::ErrorCategory::Configuration,
                                 core::ErrorCode::IoError,
                                 "Failed to rename temp config file to " + filepath + ": " + std::string(std::strerror(errno)));
    }

    return Status::ok();
}

std::vector<std::string> Config::sections() const {
    std::vector<std::string> result;
    result.reserve(values_.size());
    for (const auto& [sec, _] : values_) {
        result.push_back(sec);
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::string> Config::section_keys(const std::string& section) const {
    std::vector<std::string> result;
    auto it = values_.find(to_lower(section));
    if (it != values_.end()) {
        result.reserve(it->second.size());
        for (const auto& [key, _] : it->second) {
            result.push_back(key);
        }
        std::sort(result.begin(), result.end());
    }
    return result;
}

bool Config::has_section(const std::string& section) const {
    return values_.find(to_lower(section)) != values_.end();
}

bool Config::has_key(const std::string& section, const std::string& key) const {
    auto it = values_.find(to_lower(section));
    if (it == values_.end()) return false;
    return it->second.find(to_lower(key)) != it->second.end();
}

} // namespace ldde::config

