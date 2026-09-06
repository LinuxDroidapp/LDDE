#include "ldde/application/desktop_entry_reader.hpp"
#include "ldde/core/logging.hpp"
#include <fstream>
#include <sstream>
#include <system_error>

namespace ldde::application {

bool DesktopEntryReader::is_valid_utf8(std::string_view text) noexcept {
    const auto* bytes = reinterpret_cast<const uint8_t*>(text.data());
    size_t len = text.size();
    size_t i = 0;

    while (i < len) {
        uint8_t byte = bytes[i];
        if (byte <= 0x7F) {
            // ASCII
            i += 1;
        } else if ((byte & 0xE0) == 0xC0) {
            // 2-byte sequence (C2-DF)
            if (byte < 0xC2) return false;
            if (i + 1 >= len) return false;
            if ((bytes[i + 1] & 0xC0) != 0x80) return false;
            i += 2;
        } else if ((byte & 0xF0) == 0xE0) {
            // 3-byte sequence (E0-EF)
            if (i + 2 >= len) return false;
            uint8_t b1 = bytes[i + 1];
            uint8_t b2 = bytes[i + 2];
            if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80) return false;
            if (byte == 0xE0 && b1 < 0xA0) return false; // Overlong
            if (byte == 0xED && b1 >= 0xA0) return false; // UTF-16 surrogate
            i += 3;
        } else if ((byte & 0xF8) == 0xF0) {
            // 4-byte sequence (F0-F4)
            if (i + 3 >= len) return false;
            uint8_t b1 = bytes[i + 1];
            uint8_t b2 = bytes[i + 2];
            uint8_t b3 = bytes[i + 3];
            if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80) return false;
            if (byte == 0xF0 && b1 < 0x90) return false; // Overlong
            if (byte == 0xF4 && b1 > 0x8F) return false; // Above U+10FFFF
            if (byte > 0xF4) return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

core::Result<std::string> DesktopEntryReader::read_file(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return core::Error(core::ErrorCategory::Application, core::ErrorCode::FilesystemError,
                           "Desktop file does not exist: " + path.string(), __FILE__, __LINE__);
    }

    if (!std::filesystem::is_regular_file(path, ec)) {
        return core::Error(core::ErrorCategory::Application, core::ErrorCode::FilesystemError,
                           "Path is not a regular file: " + path.string(), __FILE__, __LINE__);
    }

    auto file_size = std::filesystem::file_size(path, ec);
    if (ec) {
        return core::Error(core::ErrorCategory::Application, core::ErrorCode::FilesystemError,
                           "Failed to get file size for: " + path.string() + " (" + ec.message() + ")", __FILE__, __LINE__);
    }

    constexpr uintmax_t kMaxDesktopFileSize = 1024 * 1024; // 1 MB limit
    if (file_size > kMaxDesktopFileSize) {
        return core::Error(core::ErrorCategory::Application, core::ErrorCode::FilesystemError,
                           "Desktop file exceeds maximum size limit: " + path.string(), __FILE__, __LINE__);
    }

    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    if (!ifs.is_open()) {
        return core::Error(core::ErrorCategory::Application, core::ErrorCode::FilesystemError,
                           "Permission denied or unable to open desktop file: " + path.string(), __FILE__, __LINE__);
    }

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (!is_valid_utf8(content)) {
        return core::Error(core::ErrorCategory::Application, core::ErrorCode::DesktopEntryParseError,
                           "Desktop file is not valid UTF-8: " + path.string(), __FILE__, __LINE__);
    }

    return content;
}

} // namespace ldde::application

