#include "ldde/desktop/desktop_background.hpp"
#include "ldde/core/logging.hpp"
#include <cairo/cairo.h>
#include <cmath>
#include <algorithm>

namespace ldde::desktop {

DesktopBackground::DesktopBackground() = default;

void DesktopBackground::load_config(const config::Config& config) {
    std::string mode_str = config.get_string_or("desktop", "background_mode", "gradient");
    if (mode_str == "solid") {
        mode_ = DesktopBackgroundMode::Solid;
    } else {
        mode_ = DesktopBackgroundMode::Gradient;
    }

    std::string color_top_hex = config.get_string_or("desktop", "background_color", "#121826");
    auto c_top = shell::Color::from_hex(color_top_hex);
    if (c_top.has_value()) {
        color_top_ = *c_top;
    }

    std::string color_bot_hex = config.get_string_or("desktop", "background_color_bottom", "#0a0d14");
    auto c_bot = shell::Color::from_hex(color_bot_hex);
    if (c_bot.has_value()) {
        color_bottom_ = *c_bot;
    }

    ambient_glow_ = config.get_bool_or("desktop", "ambient_glow", true);
    LDDE_LOG_DEBUG(Desktop, "DesktopBackground loaded (mode=" << mode_str
                           << ", color=" << color_top_hex << ", glow="
                           << (ambient_glow_ ? "yes" : "no") << ")");
}

void DesktopBackground::render(shell::ShmBuffer& buffer, double /*scale*/) {
    if (!buffer.data() || buffer.width() <= 0 || buffer.height() <= 0) return;

    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        static_cast<unsigned char*>(buffer.data()),
        CAIRO_FORMAT_ARGB32,
        buffer.width(),
        buffer.height(),
        buffer.stride());

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return;
    }

    cairo_t* cr = cairo_create(surface);

    if (mode_ == DesktopBackgroundMode::Solid) {
        cairo_set_source_rgba(cr, color_top_.r, color_top_.g, color_top_.b, color_top_.a);
        cairo_paint(cr);
    } else {
        // Gradient mode
        cairo_pattern_t* pat = cairo_pattern_create_linear(0, 0, 0, buffer.height());
        cairo_pattern_add_color_stop_rgba(pat, 0.0,
                                          color_top_.r, color_top_.g, color_top_.b, color_top_.a);
        cairo_pattern_add_color_stop_rgba(pat, 1.0,
                                          color_bottom_.r, color_bottom_.g, color_bottom_.b, color_bottom_.a);
        cairo_set_source(cr, pat);
        cairo_paint(cr);
        cairo_pattern_destroy(pat);
    }

    if (ambient_glow_) {
        double cx = static_cast<double>(buffer.width()) * 0.5;
        double cy = static_cast<double>(buffer.height()) * 0.38;
        double r = static_cast<double>(std::min(buffer.width(), buffer.height())) * 0.65;

        cairo_pattern_t* glow = cairo_pattern_create_radial(cx, cy, 0.0, cx, cy, r);
        cairo_pattern_add_color_stop_rgba(glow, 0.0, 0.20, 0.35, 0.60, 0.22);
        cairo_pattern_add_color_stop_rgba(glow, 1.0, 0.0, 0.0, 0.0, 0.0);
        cairo_set_source(cr, glow);
        cairo_paint(cr);
        cairo_pattern_destroy(glow);
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

} // namespace ldde::desktop
