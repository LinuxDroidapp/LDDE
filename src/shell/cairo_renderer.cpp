#include "ldde/shell/cairo_renderer.hpp"
#include <cmath>

namespace ldde::shell {

void CairoRenderer::set_cairo_color(cairo_t* cr, const Color& color) {
    cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
}

void CairoRenderer::draw_rounded_rect(cairo_t* cr, double x, double y, double width, double height, double radius) {
    if (radius <= 0.0) {
        cairo_rectangle(cr, x, y, width, height);
        return;
    }

    double degrees = M_PI / 180.0;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + width - radius, y + radius, radius, -90 * degrees, 0 * degrees);
    cairo_arc(cr, x + width - radius, y + height - radius, radius, 0 * degrees, 90 * degrees);
    cairo_arc(cr, x + radius, y + height - radius, radius, 90 * degrees, 180 * degrees);
    cairo_arc(cr, x + radius, y + radius, radius, 180 * degrees, 270 * degrees);
    cairo_close_path(cr);
}

void CairoRenderer::render_desktop(ShmBuffer& buffer, const ShellTheme& theme) {
    if (!buffer.data()) return;

    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        static_cast<unsigned char*>(buffer.data()),
        CAIRO_FORMAT_ARGB32,
        buffer.width(),
        buffer.height(),
        buffer.stride());

    cairo_t* cr = cairo_create(surface);

    // Linear background gradient
    cairo_pattern_t* pat = cairo_pattern_create_linear(0, 0, 0, buffer.height());
    cairo_pattern_add_color_stop_rgba(pat, 0.0,
                                      theme.desktop_bg_top.r,
                                      theme.desktop_bg_top.g,
                                      theme.desktop_bg_top.b,
                                      theme.desktop_bg_top.a);
    cairo_pattern_add_color_stop_rgba(pat, 1.0,
                                      theme.desktop_bg_bottom.r,
                                      theme.desktop_bg_bottom.g,
                                      theme.desktop_bg_bottom.b,
                                      theme.desktop_bg_bottom.a);
    cairo_set_source(cr, pat);
    cairo_paint(cr);
    cairo_pattern_destroy(pat);

    // Subtle ambient wallpaper glow
    double cx = static_cast<double>(buffer.width()) * 0.5;
    double cy = static_cast<double>(buffer.height()) * 0.35;
    double r = static_cast<double>(std::min(buffer.width(), buffer.height())) * 0.6;
    cairo_pattern_t* glow = cairo_pattern_create_radial(cx, cy, 0, cx, cy, r);
    cairo_pattern_add_color_stop_rgba(glow, 0.0, 0.20, 0.30, 0.45, 0.22);
    cairo_pattern_add_color_stop_rgba(glow, 1.0, 0.0, 0.0, 0.0, 0.0);
    cairo_set_source(cr, glow);
    cairo_paint(cr);
    cairo_pattern_destroy(glow);

    cairo_surface_flush(surface);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

void CairoRenderer::render_status_region(ShmBuffer& buffer,
                                        const ShellTheme& theme,
                                        const DesignTokens& tokens,
                                        std::string_view clock_text) {
    if (!buffer.data()) return;

    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        static_cast<unsigned char*>(buffer.data()),
        CAIRO_FORMAT_ARGB32,
        buffer.width(),
        buffer.height(),
        buffer.stride());

    cairo_t* cr = cairo_create(surface);

    // Clear background
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    double w = buffer.width();
    double h = buffer.height();

    // Subtle background pill
    double pad = tokens.spacing_xs_px;
    draw_rounded_rect(cr, pad, pad, w - pad * 2, h - pad * 2, tokens.dock_corner_radius_px * 0.5);
    set_cairo_color(cr, theme.status_bg);
    cairo_fill(cr);

    // Render clock text on the left
    set_cairo_color(cr, theme.status_text);
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    double font_size = std::max(12.0, 14.0 * tokens.scale);
    cairo_set_font_size(cr, font_size);

    cairo_text_extents_t extents;
    cairo_text_extents(cr, clock_text.data(), &extents);

    double text_x = pad + tokens.spacing_md_px;
    double text_y = (h / 2.0) - (extents.height / 2.0 + extents.y_bearing);
    cairo_move_to(cr, text_x, text_y);
    cairo_show_text(cr, clock_text.data());

    // Render system status indicators on the right (battery / network / LDDE badge)
    double pill_h = h - tokens.spacing_sm_px * 2;
    double pill_w = 48.0 * tokens.scale;
    double pill_x = w - pad - tokens.spacing_md_px - pill_w;
    double pill_y = tokens.spacing_sm_px;

    draw_rounded_rect(cr, pill_x, pill_y, pill_w, pill_h, pill_h * 0.5);
    set_cairo_color(cr, theme.status_pill_bg);
    cairo_fill(cr);

    // LDDE system indicator text
    cairo_set_font_size(cr, std::max(10.0, 11.0 * tokens.scale));
    set_cairo_color(cr, theme.status_text);
    cairo_text_extents_t badge_ext;
    cairo_text_extents(cr, "LDDE", &badge_ext);
    cairo_move_to(cr, pill_x + (pill_w - badge_ext.width) / 2.0,
                  pill_y + (pill_h / 2.0) - (badge_ext.height / 2.0 + badge_ext.y_bearing));
    cairo_show_text(cr, "LDDE");

    cairo_surface_flush(surface);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

void CairoRenderer::render_dock_region(ShmBuffer& buffer,
                                      const ShellTheme& theme,
                                      const DesignTokens& tokens,
                                      int slot_count) {
    if (!buffer.data()) return;

    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        static_cast<unsigned char*>(buffer.data()),
        CAIRO_FORMAT_ARGB32,
        buffer.width(),
        buffer.height(),
        buffer.stride());

    cairo_t* cr = cairo_create(surface);

    // Clear buffer
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    double w = buffer.width();
    double h = buffer.height();
    double radius = tokens.dock_corner_radius_px;

    // Draw main floating dock pill
    draw_rounded_rect(cr, 1.0, 1.0, w - 2.0, h - 2.0, radius);
    set_cairo_color(cr, theme.dock_bg);
    cairo_fill_preserve(cr);

    // Subtle border highlight
    set_cairo_color(cr, theme.dock_border);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    // Draw dock slots / item placeholders
    if (slot_count > 0) {
        double item_size = std::min(h - tokens.spacing_md_px * 2, 48.0 * tokens.scale);
        double total_items_w = slot_count * item_size + (slot_count - 1) * tokens.spacing_md_px;
        double start_x = (w - total_items_w) / 2.0;
        double item_y = (h - item_size) / 2.0;

        for (int i = 0; i < slot_count; ++i) {
            double ix = start_x + i * (item_size + tokens.spacing_md_px);

            // Icon rounded background
            draw_rounded_rect(cr, ix, item_y, item_size, item_size, item_size * 0.25);
            set_cairo_color(cr, theme.dock_item_bg);
            cairo_fill_preserve(cr);

            // Icon outline
            set_cairo_color(cr, theme.dock_border);
            cairo_set_line_width(cr, 1.0);
            cairo_stroke(cr);

            // Inner icon indicator
            double inner_size = item_size * 0.4;
            double in_x = ix + (item_size - inner_size) / 2.0;
            double in_y = item_y + (item_size - inner_size) / 2.0;
            draw_rounded_rect(cr, in_x, in_y, inner_size, inner_size, inner_size * 0.2);
            set_cairo_color(cr, theme.dock_item_icon);
            cairo_fill(cr);
        }
    }

    cairo_surface_flush(surface);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

void CairoRenderer::render_overlay(ShmBuffer& buffer,
                                  const ShellTheme& theme,
                                  const core::Rect& modal_rect) {
    if (!buffer.data()) return;

    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        static_cast<unsigned char*>(buffer.data()),
        CAIRO_FORMAT_ARGB32,
        buffer.width(),
        buffer.height(),
        buffer.stride());

    cairo_t* cr = cairo_create(surface);

    // Dark backdrop scrim
    set_cairo_color(cr, theme.overlay_scrim);
    cairo_paint(cr);

    // Modal card if valid rect
    if (modal_rect.width > 0 && modal_rect.height > 0) {
        draw_rounded_rect(cr, modal_rect.x, modal_rect.y, modal_rect.width, modal_rect.height, 16.0);
        set_cairo_color(cr, theme.overlay_card_bg);
        cairo_fill_preserve(cr);

        set_cairo_color(cr, theme.overlay_border);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);
    }

    cairo_surface_flush(surface);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

} // namespace ldde::shell
