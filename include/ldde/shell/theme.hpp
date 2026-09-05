#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <cstdint>

namespace ldde::shell {

struct Color {
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    double a = 1.0;

    constexpr Color() = default;
    constexpr Color(double red, double green, double blue, double alpha = 1.0)
        : r(red), g(green), b(blue), a(alpha) {}

    [[nodiscard]] static Color from_rgba(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255) noexcept {
        return Color(static_cast<double>(red) / 255.0,
                     static_cast<double>(green) / 255.0,
                     static_cast<double>(blue) / 255.0,
                     static_cast<double>(alpha) / 255.0);
    }

    [[nodiscard]] static std::optional<Color> from_hex(std::string_view hex) noexcept;

    [[nodiscard]] uint32_t to_argb32() const noexcept {
        uint8_t ua = static_cast<uint8_t>(a * 255.0);
        uint8_t ur = static_cast<uint8_t>(r * 255.0);
        uint8_t ug = static_cast<uint8_t>(g * 255.0);
        uint8_t ub = static_cast<uint8_t>(b * 255.0);
        return (static_cast<uint32_t>(ua) << 24) |
               (static_cast<uint32_t>(ur) << 16) |
               (static_cast<uint32_t>(ug) << 8) |
               static_cast<uint32_t>(ub);
    }
};

struct ShellTheme {
    // Desktop background gradient
    Color desktop_bg_top = Color::from_rgba(20, 24, 36, 255);
    Color desktop_bg_bottom = Color::from_rgba(11, 14, 20, 255);

    // Status region
    Color status_bg = Color::from_rgba(26, 30, 40, 215);
    Color status_text = Color::from_rgba(245, 246, 248, 255);
    Color status_pill_bg = Color::from_rgba(40, 46, 60, 200);

    // Dock region
    Color dock_bg = Color::from_rgba(30, 35, 48, 230);
    Color dock_border = Color::from_rgba(255, 255, 255, 38);
    Color dock_item_bg = Color::from_rgba(45, 52, 70, 240);
    Color dock_item_icon = Color::from_rgba(255, 255, 255, 220);

    // Overlay
    Color overlay_scrim = Color::from_rgba(0, 0, 0, 140);
    Color overlay_card_bg = Color::from_rgba(28, 32, 44, 245);
    Color overlay_border = Color::from_rgba(255, 255, 255, 30);

    static ShellTheme default_dark() noexcept {
        return ShellTheme{};
    }
};

} // namespace ldde::shell
