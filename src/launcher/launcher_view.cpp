#include "ldde/launcher/launcher_view.hpp"
#include <cairo.h>
#include <cmath>
#include <algorithm>
#include <unistd.h>

namespace ldde::launcher {

void LauncherView::set_color(cairo_t* cr, const shell::Color& c) {
    cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a);
}

void LauncherView::draw_rounded_rect(cairo_t* cr, double x, double y, double w, double h, double r) {
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

void LauncherView::render(
    shell::ShmBuffer& buffer,
    const shell::ShellTheme& theme,
    const shell::DesignTokens& tokens,
    const LauncherLayout& layout,
    const LauncherModel& model,
    LauncherState state,
    int scroll_y,
    std::string_view last_error_message) {
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

    // 1. Scrim backdrop
    set_color(cr, theme.overlay_scrim);
    cairo_paint(cr);

    // 2. Container card
    const auto& container = layout.container_rect();
    if (container.width > 0 && container.height > 0) {
        draw_rounded_rect(cr, container.x, container.y, container.width, container.height, tokens.dock_corner_radius_px);
        set_color(cr, theme.overlay_card_bg);
        cairo_fill_preserve(cr);

        set_color(cr, theme.overlay_border);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);
    }

    // 3. Search bar
    const auto& sb = layout.search_bar_rect();
    if (sb.width > 0 && sb.height > 0) {
        draw_rounded_rect(cr, sb.x, sb.y, sb.width, sb.height, sb.height * 0.3);
        set_color(cr, shell::Color{0.18, 0.20, 0.24, 0.90});
        cairo_fill_preserve(cr);

        set_color(cr, theme.overlay_border);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        // Search text
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 15.0);

        std::string query = model.filter().search_query;
        if (query.empty()) {
            set_color(cr, shell::Color{0.60, 0.62, 0.68, 0.70});
            cairo_move_to(cr, sb.x + 16, sb.y + sb.height * 0.62);
            cairo_show_text(cr, "Search applications...");
        } else {
            set_color(cr, shell::Color{1.0, 1.0, 1.0, 1.0});
            cairo_move_to(cr, sb.x + 16, sb.y + sb.height * 0.62);
            cairo_show_text(cr, query.c_str());

            // Clear button "✕"
            const auto& cb = layout.clear_button_rect();
            draw_rounded_rect(cr, cb.x, cb.y, cb.width, cb.height, cb.height * 0.5);
            set_color(cr, shell::Color{0.30, 0.32, 0.38, 0.80});
            cairo_fill(cr);

            set_color(cr, shell::Color{0.9, 0.9, 0.9, 1.0});
            cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 13.0);
            cairo_move_to(cr, cb.x + cb.width * 0.32, cb.y + cb.height * 0.68);
            cairo_show_text(cr, "x");
        }
    }

    // 4. Category chips
    const auto& cb = layout.category_bar_rect();
    if (cb.width > 0 && cb.height > 0) {
        cairo_save(cr);
        cairo_rectangle(cr, cb.x, cb.y, cb.width, cb.height);
        cairo_clip(cr);

        const auto& cats = model.categories();
        for (size_t i = 0; i < cats.size(); ++i) {
            core::Rect chip_r = layout.category_chip_rect(i);
            if (chip_r.width <= 0) continue;

            bool is_active = (cats[i].id == model.filter().category);
            draw_rounded_rect(cr, chip_r.x, chip_r.y, chip_r.width, chip_r.height, chip_r.height * 0.5);
            if (is_active) {
                set_color(cr, shell::Color{0.25, 0.45, 0.85, 0.95});
                cairo_fill(cr);
                set_color(cr, shell::Color{1.0, 1.0, 1.0, 1.0});
            } else {
                set_color(cr, shell::Color{0.20, 0.22, 0.28, 0.70});
                cairo_fill(cr);
                set_color(cr, shell::Color{0.80, 0.82, 0.88, 0.90});
            }

            cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL,
                                  is_active ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, 12.0);
            cairo_text_extents_t ext;
            cairo_text_extents(cr, cats[i].display_name.c_str(), &ext);
            double tx = chip_r.x + (chip_r.width - ext.width) / 2.0;
            double ty = chip_r.y + (chip_r.height + ext.height) / 2.0;
            cairo_move_to(cr, tx, ty);
            cairo_show_text(cr, cats[i].display_name.c_str());
        }
        cairo_restore(cr);
    }

    // 5. Grid items
    const auto& grid = layout.grid_rect();
    if (grid.width > 0 && grid.height > 0) {
        cairo_save(cr);
        cairo_rectangle(cr, grid.x, grid.y, grid.width, grid.height);
        cairo_clip(cr);

        const auto& items = model.items();
        if (items.empty()) {
            set_color(cr, shell::Color{0.70, 0.72, 0.78, 0.80});
            cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, 14.0);

            std::string empty_text;
            if (!model.filter().search_query.empty()) {
                empty_text = "No applications match \"" + model.filter().search_query + "\"";
            } else {
                empty_text = "No applications found";
            }

            cairo_text_extents_t ext;
            cairo_text_extents(cr, empty_text.c_str(), &ext);
            double ex = grid.x + (grid.width - ext.width) / 2.0;
            double ey = grid.y + grid.height * 0.4;
            cairo_move_to(cr, ex, ey);
            cairo_show_text(cr, empty_text.c_str());
        } else {
            for (size_t i = 0; i < items.size(); ++i) {
                core::Rect ir = layout.item_rect(i, scroll_y);
                // Check if outside visible viewport
                if (ir.y + ir.height < grid.y || ir.y > grid.y + grid.height) {
                    continue;
                }

                const auto& item = items[i];

                // Item background
                if (item.is_selected() || item.is_pressed()) {
                    draw_rounded_rect(cr, ir.x, ir.y, ir.width, ir.height, 12.0);
                    set_color(cr, item.is_pressed() ? shell::Color{0.35, 0.40, 0.55, 0.50}
                                                    : shell::Color{0.25, 0.30, 0.45, 0.40});
                    cairo_fill(cr);
                }

                // App Icon
                double icon_size = std::min(48.0, ir.width * 0.6);
                double icon_x = ir.x + (ir.width - icon_size) / 2.0;
                double icon_y = ir.y + 8.0;

                bool drawn_image = false;
                if (item.resolved_icon_path() && !item.resolved_icon_path()->empty()) {
                    const std::string& icon_path = *item.resolved_icon_path();
                    if (icon_path.ends_with(".png") && access(icon_path.c_str(), R_OK) == 0) {
                        cairo_surface_t* img = cairo_image_surface_create_from_png(icon_path.c_str());
                        if (img && cairo_surface_status(img) == CAIRO_STATUS_SUCCESS) {
                            int iw = cairo_image_surface_get_width(img);
                            int ih = cairo_image_surface_get_height(img);
                            if (iw > 0 && ih > 0) {
                                cairo_save(cr);
                                cairo_translate(cr, icon_x, icon_y);
                                cairo_scale(cr, icon_size / iw, icon_size / ih);
                                cairo_set_source_surface(cr, img, 0, 0);
                                cairo_paint(cr);
                                cairo_restore(cr);
                                drawn_image = true;
                            }
                            cairo_surface_destroy(img);
                        }
                    }
                }

                if (!drawn_image) {
                    // Fallback stylized squircle tile
                    draw_rounded_rect(cr, icon_x, icon_y, icon_size, icon_size, icon_size * 0.25);
                    set_color(cr, theme.dock_item_icon);
                    cairo_fill(cr);

                    // App initial letter badge
                    if (!item.name().empty()) {
                        std::string initial(1, static_cast<char>(std::toupper(static_cast<unsigned char>(item.name().front()))));
                        set_color(cr, shell::Color{1.0, 1.0, 1.0, 0.95});
                        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
                        cairo_set_font_size(cr, icon_size * 0.5);
                        cairo_text_extents_t text_ext;
                        cairo_text_extents(cr, initial.c_str(), &text_ext);
                        cairo_move_to(cr, icon_x + (icon_size - text_ext.width) / 2.0,
                                         icon_y + (icon_size + text_ext.height) / 2.0);
                        cairo_show_text(cr, initial.c_str());
                    }
                }

                // App label text
                set_color(cr, shell::Color{0.92, 0.94, 0.98, 1.0});
                cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
                cairo_set_font_size(cr, 11.0);

                std::string label = item.name();
                cairo_text_extents_t ext;
                cairo_text_extents(cr, label.c_str(), &ext);
                if (ext.width > ir.width - 8) {
                    while (label.size() > 3 && ext.width > ir.width - 16) {
                        label.pop_back();
                        std::string test_label = label + "...";
                        cairo_text_extents(cr, test_label.c_str(), &ext);
                    }
                    label += "...";
                }

                double tx = ir.x + (ir.width - ext.width) / 2.0;
                double ty = icon_y + icon_size + 16.0;
                cairo_move_to(cr, tx, ty);
                cairo_show_text(cr, label.c_str());
            }

            // Scrollbar indicator
            if (layout.max_scroll_y() > 0) {
                double sb_w = 4.0;
                double sb_x = grid.x + grid.width - sb_w - 2.0;
                double ratio = static_cast<double>(grid.height) / static_cast<double>(layout.total_content_height());
                double thumb_h = std::max(24.0, grid.height * ratio);
                double max_thumb_travel = grid.height - thumb_h;
                double thumb_y = grid.y + (static_cast<double>(scroll_y) / layout.max_scroll_y()) * max_thumb_travel;

                draw_rounded_rect(cr, sb_x, thumb_y, sb_w, thumb_h, sb_w * 0.5);
                set_color(cr, shell::Color{0.6, 0.6, 0.7, 0.5});
                cairo_fill(cr);
            }
        }

        cairo_restore(cr);
    }

    // 6. Launch failure alert banner
    if (state == LauncherState::LaunchFailed && !last_error_message.empty()) {
        double banner_h = 36.0;
        double banner_y = container.y + container.height - banner_h - 12.0;
        double banner_x = container.x + 16.0;
        double banner_w = container.width - 32.0;

        draw_rounded_rect(cr, banner_x, banner_y, banner_w, banner_h, 8.0);
        set_color(cr, shell::Color{0.8, 0.2, 0.2, 0.85});
        cairo_fill(cr);

        set_color(cr, shell::Color{1.0, 1.0, 1.0, 1.0});
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 12.0);

        std::string err_text = "Launch failed: " + std::string(last_error_message);
        cairo_move_to(cr, banner_x + 12.0, banner_y + 22.0);
        cairo_show_text(cr, err_text.c_str());
    }

    cairo_surface_flush(surface);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

} // namespace ldde::launcher
