#pragma once

#include <string>
#include <filesystem>
#include "ldde/core/error.hpp"

namespace ldde::application {

class DesktopEntryReader {
public:
    DesktopEntryReader() = default;

    [[nodiscard]] static core::Result<std::string> read_file(const std::filesystem::path& path);
    [[nodiscard]] static bool is_valid_utf8(std::string_view text) noexcept;
};

} // namespace ldde::application
