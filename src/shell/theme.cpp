#include "ldde/shell/theme.hpp"
#include <charconv>

namespace ldde::shell {

std::optional<Color> Color::from_hex(std::string_view hex) noexcept {
    if (hex.empty()) {
        return std::nullopt;
    }

    if (hex.front() == '#') {
        hex.remove_prefix(1);
    }

    if (hex.length() != 6 && hex.length() != 8) {
        return std::nullopt;
    }

    uint32_t val = 0;
    auto [ptr, ec] = std::from_chars(hex.data(), hex.data() + hex.length(), val, 16);
    if (ec != std::errc{} || ptr != hex.data() + hex.length()) {
        return std::nullopt;
    }

    if (hex.length() == 6) {
        uint8_t r = static_cast<uint8_t>((val >> 16) & 0xFF);
        uint8_t g = static_cast<uint8_t>((val >> 8) & 0xFF);
        uint8_t b = static_cast<uint8_t>(val & 0xFF);
        return Color::from_rgba(r, g, b, 255);
    } else {
        uint8_t r = static_cast<uint8_t>((val >> 24) & 0xFF);
        uint8_t g = static_cast<uint8_t>((val >> 16) & 0xFF);
        uint8_t b = static_cast<uint8_t>((val >> 8) & 0xFF);
        uint8_t a = static_cast<uint8_t>(val & 0xFF);
        return Color::from_rgba(r, g, b, a);
    }
}

} // namespace ldde::shell
