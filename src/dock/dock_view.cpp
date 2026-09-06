#include "ldde/dock/dock_view.hpp"
#include "ldde/core/logging.hpp"
#include <cairo/cairo.h>
#include <cmath>

namespace ldde::dock {

namespace {

void set_cairo_color(cairo_t* cr, const shell::Color& c, double alpha_mult = 1.0) {
    cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a * alpha_mult);
}

void draw_rounded_rect(cairo_t* cr, double x, double y, double w, double h, double r) {
    r = std::min(r, std::min(w, h) / 2.0);
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI_2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI_2);
    cairo_arc(cr, x + r, y + h - r, r, M_PI_2, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3.0 * M_PI_2);
    cairo_close_path(cr);
}

} // namespace

void DockView::render(shell::ShmBuffer& buffer,
                      const shell::ShellTheme& theme,
                      const shell::DesignTokens& tokens,
                      const DockLayout& layout,
                      const DockModel& model,
                      launcher::LauncherIconResolver& icon_resolver,
                      int32_t hovered_index,
                      int32_t pressed_index) {
    if (!buffer.data() || buffer.width() <= 0 || buffer.height() <= 0) return;

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
    double radius = tokens.dock_corner_radius_px > 0 ? tokens.dock_corner_radius_px : 24.0;

    // 1. Draw floating dock pill background
    draw_rounded_rect(cr, 1.0, 1.0, w - 2.0, h - 2.0, radius);
    set_cairo_color(cr, theme.dock_bg);
    cairo_fill_preserve(cr);

    // Subtle border
    set_cairo_color(cr, theme.dock_border);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    // 2. Render launcher button
    core::Rect launcher_rect = layout.launcher_button_rect();
    launcher_rect.x -= layout.scroll_offset_x();
    render_launcher_button(cr, launcher_rect, theme,
                           hovered_index == -2, pressed_index == -2);

    // 3. Subtle separator between launcher button and items
    double sep_x = launcher_rect.x + launcher_rect.width + 4.0;
    cairo_set_line_width(cr, 1.0);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.15);
    cairo_move_to(cr, sep_x, launcher_rect.y + 6.0);
    cairo_line_to(cr, sep_x, launcher_rect.y + launcher_rect.height - 6.0);
    cairo_stroke(cr);

    // 4. Render items
    for (size_t i = 0; i < model.item_count(); ++i) {
        const auto* item = model.item_at(i);
        if (!item) continue;

        core::Rect item_r = layout.visible_item_rect(i);
        // Skip if outside horizontal bounds
        if (item_r.x + item_r.width < 0 || item_r.x > w) {
            continue;
        }

        render_dock_item(cr, *item, item_r, theme, tokens, icon_resolver,
                         hovered_index == static_cast<int32_t>(i),
                         pressed_index == static_cast<int32_t>(i));
    }

    cairo_surface_flush(surface);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

void DockView::render_launcher_button(cairo_t* cr,
                                     const core::Rect& rect,
                                     const shell::ShellTheme& theme,
                                     bool is_hovered,
                                     bool is_pressed) {
    double x = rect.x;
    double y = rect.y;
    double size = rect.width;
    double r = size * 0.25;

    // Background pill/card
    draw_rounded_rect(cr, x, y, size, size, r);
    if (is_pressed) {
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.25);
    } else if (is_hovered) {
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.15);
    } else {
        set_cairo_color(cr, theme.dock_item_bg);
    }
    cairo_fill_preserve(cr);

    set_cairo_color(cr, theme.dock_border);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    // App grid icon (3x3 dots)
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.90);
    double dot_r = size * 0.055;
    double center_x = x + size / 2.0;
    double center_y = y + size / 2.0;
    double step = size * 0.22;

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            cairo_arc(cr, center_x + dx * step, center_y + dy * step, dot_r, 0, 2.0 * M_PI);
            cairo_fill(cr);
        }
    }
}

void DockView::render_dock_item(cairo_t* cr,
                               const DockItem& item,
                               const core::Rect& rect,
                               const shell::ShellTheme& theme,
                               const shell::DesignTokens& tokens,
                               launcher::LauncherIconResolver& icon_resolver,
                               bool is_hovered,
                               bool is_pressed) {
    (void)theme;
    (void)tokens;
    double x = rect.x;
    double y = rect.y;
    double size = rect.width;
    double r = size * 0.25;
    double alpha = item.is_minimized() ? 0.60 : 1.0;
    if (!item.is_available()) alpha *= 0.45;

    // Background highlight
    if (item.is_active()) {
        draw_rounded_rect(cr, x, y, size, size, r);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.22 * alpha);
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.40 * alpha);
        cairo_set_line_width(cr, 1.2);
        cairo_stroke(cr);
    } else if (is_pressed) {
        draw_rounded_rect(cr, x, y, size, size, r);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.18 * alpha);
        cairo_fill(cr);
    } else if (is_hovered) {
        draw_rounded_rect(cr, x, y, size, size, r);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.10 * alpha);
        cairo_fill(cr);
    }

    // Icon rendering
    double icon_size = size * 0.72;
    double icon_x = x + (size - icon_size) / 2.0;
    double icon_y = y + (size - icon_size) / 2.0 - (item.is_running() ? 2.0 : 0.0);

    bool icon_rendered = false;
    auto icon_path = icon_resolver.resolve(item.icon_ref(), static_cast<int>(icon_size));
    if (icon_path && !icon_path->empty()) {
        cairo_surface_t* img_surf = cairo_image_surface_create_from_png(icon_path->c_str());
        if (cairo_surface_status(img_surf) == CAIRO_STATUS_SUCCESS) {
            int img_w = cairo_image_surface_get_width(img_surf);
            int img_h = cairo_image_surface_get_height(img_surf);
            if (img_w > 0 && img_h > 0) {
                cairo_save(cr);
                cairo_translate(cr, icon_x, icon_y);
                cairo_scale(cr, icon_size / img_w, icon_size / img_h);
                cairo_set_source_surface(cr, img_surf, 0, 0);
                if (alpha < 0.99) {
                    cairo_paint_with_alpha(cr, alpha);
                } else {
                    cairo_paint(cr);
                }
                cairo_restore(cr);
                icon_rendered = true;
            }
        }
        cairo_surface_destroy(img_surf);
    }

    if (!icon_rendered) {
        // Fallback vector badge
        cairo_save(cr);
        if (alpha < 0.99) {
            cairo_push_group(cr);
        }
        render_fallback_badge(cr, item.name(), icon_x, icon_y, icon_size);
        if (alpha < 0.99) {
            cairo_pop_group_to_source(cr);
            cairo_paint_with_alpha(cr, alpha);
        }
        cairo_restore(cr);
    }

    // Running indicator
    if (item.is_running()) {
        double ind_y = y + size - 4.0;
        double center_x = x + size / 2.0;

        if (item.is_active()) {
            // Active application: glowing pill indicator
            double pill_w = size * 0.32;
            double pill_h = 3.0;
            draw_rounded_rect(cr, center_x - pill_w / 2.0, ind_y - 1.0, pill_w, pill_h, 1.5);
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.95);
            cairo_fill(cr);
        } else {
            // Running but not active: dot indicator
            double dot_r = 2.4;
            if (item.window_count() > 1) {
                // Multiple windows: two subtle dots
                cairo_arc(cr, center_x - 4.0, ind_y, dot_r, 0, 2.0 * M_PI);
                cairo_arc(cr, center_x + 4.0, ind_y, dot_r, 0, 2.0 * M_PI);
            } else {
                cairo_arc(cr, center_x, ind_y, dot_r, 0, 2.0 * M_PI);
            }
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.75);
            cairo_fill(cr);
        }
    }
}

void DockView::render_fallback_badge(cairo_t* cr,
                                     const std::string& name,
                                     double x, double y, double size) {
    // Generate deterministic hue from name
    uint32_t hash = 5381;
    for (char c : name) {
        hash = ((hash << 5) + hash) + static_cast<unsigned char>(c);
    }
    double hue = static_cast<double>(hash % 360);

    // Simple HSV to RGB conversion
    double s = 0.55;
    double v = 0.70;
    double c = v * s;
    double hp = hue / 60.0;
    double x_c = c * (1.0 - std::fabs(std::fmod(hp, 2.0) - 1.0));
    double r1 = 0, g1 = 0, b1 = 0;
    if (hp >= 0 && hp < 1) { r1 = c; g1 = x_c; }
    else if (hp < 2) { r1 = x_c; g1 = c; }
    else if (hp < 3) { g1 = c; b1 = x_c; }
    else if (hp < 4) { g1 = x_c; b1 = c; }
    else if (hp < 5) { r1 = x_c; b1 = c; }
    else { r1 = c; b1 = x_c; }
    double m = v - c;
    double r_val = r1 + m;
    double g_val = g1 + m;
    double b_val = b1 + m;

    draw_rounded_rect(cr, x, y, size, size, size * 0.22);
    cairo_set_source_rgb(cr, r_val, g_val, b_val);
    cairo_fill(cr);

    // Draw initial letter
    char initial = 'A';
    if (!name.empty()) {
        initial = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
    }
    char letter_str[2] = {initial, '\0'};

    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, size * 0.50);
    cairo_text_extents_t extents;
    cairo_text_extents(cr, letter_str, &extents);

    double tx = x + (size - extents.width) / 2.0 - extents.x_bearing;
    double ty = y + (size - extents.height) / 2.0 - extents.y_bearing;

    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_move_to(cr, tx, ty);
    cairo_show_text(cr, letter_str);
}

} // namespace ldde::dock
