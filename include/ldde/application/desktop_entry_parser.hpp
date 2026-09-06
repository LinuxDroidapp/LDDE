#pragma once

#include <string>
#include <string_view>
#include <vector>
#include "ldde/core/error.hpp"
#include "ldde/application/desktop_entry.hpp"

namespace ldde::application {

struct ParsedExec {
    std::string executable;
    std::vector<std::string> arguments;
    std::vector<std::string> field_codes;
    std::string raw_exec;
};

class DesktopEntryParser {
public:
    DesktopEntryParser() = default;

    [[nodiscard]] static core::Result<DesktopEntry> parse(std::string_view content);

    [[nodiscard]] static ParsedExec parse_exec(std::string_view exec_line);

    [[nodiscard]] static std::string unescape_string(std::string_view value);
};

} // namespace ldde::application
