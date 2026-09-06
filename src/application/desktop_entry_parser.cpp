#include "ldde/application/desktop_entry_parser.hpp"
#include "ldde/core/logging.hpp"
#include <sstream>
#include <cctype>

namespace ldde::application {

namespace {

std::string trim_spaces(std::string_view sv) {
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

} // namespace

std::string DesktopEntryParser::unescape_string(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    bool escape = false;

    for (size_t i = 0; i < value.size(); ++i) {
        char c = value[i];
        if (escape) {
            switch (c) {
                case 's': result.push_back(' '); break;
                case 'n': result.push_back('\n'); break;
                case 't': result.push_back('\t'); break;
                case 'r': result.push_back('\r'); break;
                case '\\': result.push_back('\\'); break;
                default:
                    // Literal character
                    result.push_back(c);
                    break;
            }
            escape = false;
        } else if (c == '\\') {
            escape = true;
        } else {
            result.push_back(c);
        }
    }
    if (escape) {
        result.push_back('\\');
    }
    return result;
}

ParsedExec DesktopEntryParser::parse_exec(std::string_view exec_line) {
    ParsedExec res;
    res.raw_exec = std::string(exec_line);
    if (exec_line.empty()) return res;

    std::vector<std::string> tokens;
    std::string current;
    bool in_quotes = false;
    bool escape = false;

    for (size_t i = 0; i < exec_line.size(); ++i) {
        char c = exec_line[i];
        if (escape) {
            current.push_back(c);
            escape = false;
        } else if (c == '\\') {
            escape = true;
        } else if (c == '"') {
            in_quotes = !in_quotes;
        } else if (std::isspace(static_cast<unsigned char>(c)) && !in_quotes) {
            if (!current.empty()) {
                tokens.push_back(std::move(current));
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        tokens.push_back(std::move(current));
    }

    if (tokens.empty()) return res;

    res.executable = tokens[0];

    for (size_t i = 1; i < tokens.size(); ++i) {
        const auto& tok = tokens[i];
        if (tok.size() >= 2 && tok[0] == '%') {
            res.field_codes.push_back(tok);
        }
        res.arguments.push_back(tok);
    }

    return res;
}

core::Result<DesktopEntry> DesktopEntryParser::parse(std::string_view content) {
    DesktopEntry entry;
    std::string current_group;
    std::istringstream stream((std::string(content)));
    std::string line;
    size_t line_number = 0;

    while (std::getline(stream, line)) {
        line_number++;
        // Remove trailing \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::string trimmed = trim_spaces(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            current_group = trimmed.substr(1, trimmed.size() - 2);
            current_group = trim_spaces(current_group);
            continue;
        }

        auto eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos) {
            // Malformed line outside group or without '='
            LDDE_LOG_DEBUG(Application, "Skipping malformed line " << line_number << " in desktop entry");
            continue;
        }

        if (current_group.empty()) {
            LDDE_LOG_DEBUG(Application, "Key before group header on line " << line_number << "; ignoring");
            continue;
        }

        std::string raw_key = trim_spaces(trimmed.substr(0, eq_pos));
        std::string raw_val = trim_spaces(trimmed.substr(eq_pos + 1));

        std::string key_name;
        std::string locale;

        auto bracket_open = raw_key.find('[');
        if (bracket_open != std::string::npos && raw_key.back() == ']') {
            key_name = trim_spaces(raw_key.substr(0, bracket_open));
            locale = raw_key.substr(bracket_open + 1, raw_key.size() - bracket_open - 2);
            locale = trim_spaces(locale);
        } else {
            key_name = raw_key;
        }

        std::string unescaped_val = unescape_string(raw_val);
        entry.set_value(current_group, std::move(key_name), std::move(locale), std::move(unescaped_val));
    }

    if (!entry.has_group("Desktop Entry")) {
        return core::Error(core::ErrorCategory::Application, core::ErrorCode::DesktopEntryInvalid,
                           "Missing [Desktop Entry] group", __FILE__, __LINE__);
    }

    return entry;
}

} // namespace ldde::application

