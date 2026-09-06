#include "ldde/switcher/switcher_view.hpp"
#include <cmath>
#include <algorithm>
#include <unistd.h>

namespace ldde::switcher {

void SwitcherView::set_color(cairo_t* cr, const shell::Color& c) {
    cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a);
}

void SwitcherView::draw_rounded_rect(cairo_t* cr, double x, double y, double w, double h, double r) {
    if (r <= 0.0) {
        cairo_rectangle(cr, x, y, w, h);
        return;
    }
    double deg = M_PI / 180.0;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -90.0 * deg, 0.0 * deg);
    cairo_arc(cr, x + w - r, y + h - r, r, 0.0 * deg, 90.0 * deg);
    cairo_arc(cr, x + r, y + h - r, r, 90.0 * deg, 180.0 * deg);
    cairo_arc(cr, x + r, y + r, r, 180.0 * deg, 270.0 * deg);
    cairo_close_path(cr);
}

void SwitcherView::draw_icon(cairo_t* cr,
                             launcher::LauncherIconResolver& icon_resolver,
                             const SwitcherItem& item,
                             double x, double y, double size) {
    bool drawn = false;
    if (!item.icon_name().empty()) {
        auto opt_path = icon_resolver.resolve_icon_name(item.icon_name(), static_cast<int>(size));
        if (opt_path.has_value() && !opt_path->empty()) {
            const std::string& icon_path = *opt_path;
            if (icon_path.ends_with(".png") && access(icon_path.c_str(), R_OK) == 0) {
                cairo_surface_t* img = cairo_image_surface_create_from_png(icon_path.c_str());
                if (img && cairo_surface_status(img) == CAIRO_STATUS_SUCCESS) {
                    int img_w = cairo_image_surface_get_width(img);
                    int img_h = cairo_image_surface_get_height(img);
                    if (img_w > 0 && img_h > 0) {
                        cairo_save(cr);
                        cairo_translate(cr, x, y);
                        cairo_scale(cr, size / static_cast<double>(img_w), size / static_cast<double>(img_h));
                        cairo_set_source_surface(cr, img, 0, 0);
                        cairo_paint(cr);
                        cairo_restore(cr);
                        drawn = true;
                    }
                    cairo_surface_destroy(img);
                }
            }
        }
    }

    if (!drawn) {
        // High quality procedural vector icon with app initial
        draw_rounded_rect(cr, x, y, size, size, size * 0.25);
        size_t h = std::hash<std::string>{}(item.display_name());
        double r = 0.25 + 0.45 * (static_cast<double>(h & 0xFF) / 255.0);
        double g = 0.25 + 0.45 * (static_cast<double>((h >> 8) & 0xFF) / 255.0);
        double b = 0.35 + 0.45 * (static_cast<double>((h >> 16) & 0xFF) / 255.0);
        cairo_set_source_rgb(cr, r, g, b);
        cairo_fill(cr);

        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, size * 0.55);
        char initial[2] = { item.display_name().empty() ? 'A' : static_cast<char>(std::toupper(static_cast<unsigned char>(item.display_name()[0]))), '\0' };
        cairo_text_extents_t ext;
        cairo_text_extents(cr, initial, &ext);
        cairo_move_to(cr, x + (size - ext.width) / 2.0 - ext.x_bearing,
                          y + (size - ext.height) / 2.0 - ext.y_bearing);
        cairo_show_text(cr, initial);
    }
}

void SwitcherView::render(
    shell::ShmBuffer& buffer,
    const shell::ShellTheme& theme,
    const shell::DesignTokens& tokens,
    const SwitcherLayout& layout,
    const SwitcherModel& model,
    SwitcherState /*state*/,
    size_t selected_index,
    launcher::LauncherIconResolver& icon_resolver) {

    if (!buffer.data() || buffer.width() <= 0 || buffer.height() <= 0) {
        return;
    }

    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        static_cast<unsigned char*>(buffer.data()),
        CAIRO_FORMAT_ARGB32,
        buffer.width(),
        buffer.height(),
        buffer.stride());

    if (!surface || cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        if (surface) cairo_surface_destroy(surface);
        return;
    }

    cairo_t* cr = cairo_create(surface);

    // Primary & secondary text colors
    shell::Color text_primary{0.95, 0.96, 0.98, 1.0};
    shell::Color text_secondary{0.70, 0.74, 0.82, 0.90};
    shell::Color active_accent{0.30, 0.60, 0.95, 1.0};

    // 1. Dimmed backdrop scrim
    set_color(cr, theme.overlay_scrim);
    cairo_paint(cr);

    // 2. Header
    const auto& hr = layout.header_rect();
    if (hr.width > 0 && hr.height > 0) {
        cairo_save(cr);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, std::max(16.0, hr.height * 0.40));
        set_color(cr, text_primary);
        cairo_move_to(cr, hr.x + 20, hr.y + hr.height * 0.55);
        cairo_show_text(cr, "Switch Applications");

        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, std::max(11.0, hr.height * 0.22));
        set_color(cr, text_secondary);
        cairo_move_to(cr, hr.x + 20, hr.y + hr.height * 0.85);
        cairo_show_text(cr, "Tap or Tab to switch \xe2\x80\xa2 Esc to cancel");
        cairo_restore(cr);
    }

    // 3. Empty state check
    if (model.empty()) {
        const auto& crt = layout.content_rect();
        cairo_save(cr);
        draw_rounded_rect(cr, crt.x + (crt.width - 260) / 2.0, crt.y + (crt.height - 100) / 2.0, 260, 100, 16);
        set_color(cr, shell::Color{0.14, 0.16, 0.20, 0.90});
        cairo_fill_preserve(cr);
        set_color(cr, theme.overlay_border);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 16.0);
        set_color(cr, text_primary);
        cairo_move_to(cr, crt.x + (crt.width - 260) / 2.0 + 35, crt.y + (crt.height - 100) / 2.0 + 55);
        cairo_show_text(cr, "No running applications");
        cairo_restore(cr);

        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return;
    }

    // 4. Cards
    const auto& content = layout.content_rect();
    cairo_save(cr);
    cairo_rectangle(cr, content.x, content.y, content.width, content.height);
    cairo_clip(cr);

    int32_t scroll = layout.scroll_offset();
    bool horizontal = layout.is_horizontal();

    for (size_t i = 0; i < model.item_count(); ++i) {
        const auto* item = model.item_at(i);
        const auto* orig_rect = layout.rect_at(i);
        if (!item || !orig_rect) continue;

        core::Rect r = *orig_rect;
        if (!horizontal) {
            r.y -= scroll;
        } else {
            r.x -= scroll;
        }

        // Culling
        if (r.x + r.width < content.x || r.x > content.x + content.width ||
            r.y + r.height < content.y || r.y > content.y + content.height) {
            continue;
        }

        bool is_selected = (i == selected_index);
        bool is_current = item->is_current();
        bool is_minimized = item->is_minimized();

        cairo_save(cr);
        if (is_minimized && !is_selected) {
            cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
        }

        draw_rounded_rect(cr, r.x, r.y, r.width, r.height, tokens.dock_corner_radius_px > 0 ? tokens.dock_corner_radius_px : 12.0);

        // Background
        if (is_selected) {
            set_color(cr, shell::Color{0.25, 0.35, 0.52, 0.95});
            cairo_fill_preserve(cr);
            set_color(cr, active_accent);
            cairo_set_line_width(cr, 2.5);
            cairo_stroke(cr);
        } else if (is_current) {
            set_color(cr, shell::Color{0.18, 0.22, 0.28, 0.90});
            cairo_fill_preserve(cr);
            set_color(cr, shell::Color{0.30, 0.60, 0.90, 0.70});
            cairo_set_line_width(cr, 1.5);
            cairo_stroke(cr);
        } else {
            set_color(cr, shell::Color{0.13, 0.15, 0.18, 0.85});
            cairo_fill_preserve(cr);
            set_color(cr, theme.overlay_border);
            cairo_set_line_width(cr, 1.0);
            cairo_stroke(cr);
        }

        if (!horizontal) {
            // Portrait card layout: Icon on left, Title & subtitle in center, badge on right
            double icon_size = std::min(48.0, static_cast<double>(r.height) - 20.0);
            double icon_x = r.x + 16.0;
            double icon_y = r.y + (r.height - icon_size) / 2.0;
            draw_icon(cr, icon_resolver, *item, icon_x, icon_y, icon_size);

            double text_x = icon_x + icon_size + 16.0;
            double max_text_w = r.width - (text_x - r.x) - 90.0;

            cairo_save(cr);
            cairo_rectangle(cr, text_x, r.y, std::max(20.0, max_text_w), r.height);
            cairo_clip(cr);

            cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 15.0);
            set_color(cr, text_primary);
            cairo_move_to(cr, text_x, r.y + r.height * 0.45);
            cairo_show_text(cr, item->display_name().c_str());

            cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, 12.0);
            set_color(cr, text_secondary);
            cairo_move_to(cr, text_x, r.y + r.height * 0.75);
            std::string sub = item->window_title().empty() ? item->display_name() : item->window_title();
            cairo_show_text(cr, sub.c_str());
            cairo_restore(cr);

            // Badges on right
            double badge_right = r.x + r.width - 12.0;
            if (is_current) {
                draw_rounded_rect(cr, badge_right - 54, r.y + (r.height - 22) / 2.0, 54, 22, 11);
                set_color(cr, shell::Color{0.15, 0.45, 0.75, 0.85});
                cairo_fill(cr);
                cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
                cairo_set_font_size(cr, 10.0);
                set_color(cr, shell::Color{1, 1, 1, 1});
                cairo_move_to(cr, badge_right - 46, r.y + (r.height - 22) / 2.0 + 15);
                cairo_show_text(cr, "Active");
            } else if (is_minimized) {
                draw_rounded_rect(cr, badge_right - 68, r.y + (r.height - 22) / 2.0, 68, 22, 11);
                set_color(cr, shell::Color{0.30, 0.30, 0.35, 0.85});
                cairo_fill(cr);
                cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
                cairo_set_font_size(cr, 10.0);
                set_color(cr, shell::Color{0.9, 0.9, 0.9, 1});
                cairo_move_to(cr, badge_right - 63, r.y + (r.height - 22) / 2.0 + 15);
                cairo_show_text(cr, "Minimized");
            } else if (item->window_count() > 1) {
                std::string cnt_str = std::to_string(item->window_count()) + " wins";
                draw_rounded_rect(cr, badge_right - 58, r.y + (r.height - 22) / 2.0, 58, 22, 11);
                set_color(cr, shell::Color{0.25, 0.28, 0.35, 0.85});
                cairo_fill(cr);
                cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
                cairo_set_font_size(cr, 10.0);
                set_color(cr, shell::Color{0.9, 0.9, 0.9, 1});
                cairo_move_to(cr, badge_right - 52, r.y + (r.height - 22) / 2.0 + 15);
                cairo_show_text(cr, cnt_str.c_str());
            }

        } else {
            // Landscape card layout: Icon on top, App title and window title below
            double icon_size = std::min(56.0, r.height * 0.42);
            double icon_x = r.x + (r.width - icon_size) / 2.0;
            double icon_y = r.y + 16.0;
            draw_icon(cr, icon_resolver, *item, icon_x, icon_y, icon_size);

            cairo_save(cr);
            cairo_rectangle(cr, r.x + 8.0, icon_y + icon_size + 8.0, r.width - 16.0, r.height - (icon_size + 30.0));
            cairo_clip(cr);

            cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 14.0);
            set_color(cr, text_primary);
            cairo_text_extents_t ext;
            cairo_text_extents(cr, item->display_name().c_str(), &ext);
            cairo_move_to(cr, r.x + (r.width - ext.width) / 2.0, icon_y + icon_size + 24.0);
            cairo_show_text(cr, item->display_name().c_str());

            std::string sub = item->window_title().empty() ? item->display_name() : item->window_title();
            cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, 11.0);
            set_color(cr, text_secondary);
            cairo_text_extents(cr, sub.c_str(), &ext);
            cairo_move_to(cr, r.x + (r.width - ext.width) / 2.0, icon_y + icon_size + 42.0);
            cairo_show_text(cr, sub.c_str());
            cairo_restore(cr);

            if (is_current) {
                draw_rounded_rect(cr, r.x + (r.width - 50) / 2.0, r.y + r.height - 20, 50, 16, 8);
                set_color(cr, shell::Color{0.15, 0.45, 0.75, 0.85});
                cairo_fill(cr);
                cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
                cairo_set_font_size(cr, 9.0);
                set_color(cr, shell::Color{1, 1, 1, 1});
                cairo_move_to(cr, r.x + (r.width - 50) / 2.0 + 9, r.y + r.height - 8);
                cairo_show_text(cr, "Active");
            }
        }

        cairo_restore(cr);
    }

    cairo_restore(cr); // Content clip

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

} // namespace ldde::switcher
