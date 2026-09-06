#include "ldde/notification/notification_view.hpp"
#include <cairo/cairo.h>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ldde::notification {

namespace {

void draw_rounded_rect(cairo_t* cr, double x, double y, double w, double h, double r) {
    if (r <= 0.0) {
        cairo_rectangle(cr, x, y, w, h);
        return;
    }
    r = std::min(r, std::min(w / 2.0, h / 2.0));
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2.0, 0.0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0.0, M_PI / 2.0);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2.0, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3.0 * M_PI / 2.0);
    cairo_close_path(cr);
}

} // namespace

void NotificationView::render_popups(
    shell::ShmBuffer& buffer,
    const shell::ShellTheme& /*theme*/,
    const shell::DesignTokens& tokens,
    const NotificationLayout& /*layout*/,
    const std::vector<const Notification*>& visible_popups) {
    if (visible_popups.empty() || buffer.data() == nullptr) return;

    cairo_surface_t* surf = cairo_image_surface_create_for_data(
        static_cast<unsigned char*>(buffer.data()),
        CAIRO_FORMAT_ARGB32,
        buffer.width(),
        buffer.height(),
        buffer.stride());
    if (!surf) return;

    cairo_t* cr = cairo_create(surf);
    if (!cr) {
        cairo_surface_destroy(surf);
        return;
    }

    for (const auto* notif : visible_popups) {
        if (!notif) continue;
        draw_notification_card(cr, *notif, notif->popup_geometry(), tokens, tokens.scale, true);
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surf);
}

void NotificationView::render_notification_center(
    shell::ShmBuffer& buffer,
    const shell::ShellTheme& /*theme*/,
    const shell::DesignTokens& tokens,
    const NotificationLayout& layout,
    const std::vector<const Notification*>& items) {
    if (buffer.data() == nullptr) return;

    cairo_surface_t* surf = cairo_image_surface_create_for_data(
        static_cast<unsigned char*>(buffer.data()),
        CAIRO_FORMAT_ARGB32,
        buffer.width(),
        buffer.height(),
        buffer.stride());
    if (!surf) return;

    cairo_t* cr = cairo_create(surf);
    if (!cr) {
        cairo_surface_destroy(surf);
        return;
    }

    const auto& panel_rect = layout.center_panel_geometry();

    // 1. Panel background scrim & card
    draw_rounded_rect(cr, panel_rect.x, panel_rect.y, panel_rect.width, panel_rect.height, 14.0 * tokens.scale);
    cairo_set_source_rgba(cr, 0.08, 0.10, 0.14, 0.96);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0.25, 0.30, 0.40, 0.6);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);

    // 2. Header
    const auto& header_rect = layout.center_header_geometry();
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 15.0 * tokens.scale);
    cairo_set_source_rgba(cr, 0.95, 0.95, 0.95, 1.0);
    cairo_move_to(cr, header_rect.x + tokens.spacing_md_px, header_rect.y + header_rect.height * 0.65);
    cairo_show_text(cr, "Notifications");

    // Notification count badge
    std::string count_str = "(" + std::to_string(items.size()) + ")";
    cairo_set_font_size(cr, 12.0 * tokens.scale);
    cairo_set_source_rgba(cr, 0.6, 0.65, 0.75, 1.0);
    cairo_show_text(cr, (" " + count_str).c_str());

    // Clear All button
    if (!items.empty()) {
        const auto& clear_rect = layout.center_clear_all_geometry();
        draw_rounded_rect(cr, clear_rect.x, clear_rect.y, clear_rect.width, clear_rect.height, 8.0 * tokens.scale);
        cairo_set_source_rgba(cr, 0.2, 0.24, 0.32, 0.8);
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, 0.4, 0.45, 0.55, 0.6);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 11.0 * tokens.scale);
        cairo_set_source_rgba(cr, 0.9, 0.9, 0.9, 1.0);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, "Clear All", &ext);
        cairo_move_to(cr, clear_rect.x + (clear_rect.width - ext.width) / 2.0,
                      clear_rect.y + (clear_rect.height + ext.height) / 2.0);
        cairo_show_text(cr, "Clear All");
    }

    // Header divider line
    cairo_set_source_rgba(cr, 0.2, 0.24, 0.32, 0.6);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, panel_rect.x, header_rect.y + header_rect.height);
    cairo_line_to(cr, panel_rect.x + panel_rect.width, header_rect.y + header_rect.height);
    cairo_stroke(cr);

    // 3. Items list or empty state
    if (items.empty()) {
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 13.0 * tokens.scale);
        cairo_set_source_rgba(cr, 0.5, 0.55, 0.65, 0.8);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, "No notifications", &ext);
        cairo_move_to(cr, panel_rect.x + (panel_rect.width - ext.width) / 2.0,
                      panel_rect.y + panel_rect.height / 2.0);
        cairo_show_text(cr, "No notifications");
    } else {
        const auto& item_geoms = layout.center_item_geometries();
        for (size_t i = 0; i < items.size() && i < item_geoms.size(); ++i) {
            if (items[i]) {
                draw_notification_card(cr, *items[i], item_geoms[i], tokens, tokens.scale, false);
            }
        }
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surf);
}

void NotificationView::draw_notification_card(
    cairo_t* cr,
    const Notification& notif,
    const core::Rect& geom,
    const shell::DesignTokens& tokens,
    double scale,
    bool is_popup) {
    if (geom.width <= 0 || geom.height <= 0) return;

    // Card background
    draw_rounded_rect(cr, geom.x, geom.y, geom.width, geom.height, 10.0 * scale);
    if (is_popup) {
        cairo_set_source_rgba(cr, 0.12, 0.14, 0.18, 0.95);
    } else {
        cairo_set_source_rgba(cr, 0.14, 0.16, 0.22, 0.90);
    }
    cairo_fill_preserve(cr);

    // Card border
    if (notif.is_critical()) {
        cairo_set_source_rgba(cr, 0.95, 0.3, 0.2, 0.85);
        cairo_set_line_width(cr, 2.0);
    } else {
        cairo_set_source_rgba(cr, 0.28, 0.32, 0.42, 0.6);
        cairo_set_line_width(cr, 1.0);
    }
    cairo_stroke(cr);

    // App badge / icon (small circle with first letter)
    double badge_size = 20.0 * scale;
    double badge_x = geom.x + tokens.spacing_sm_px;
    double badge_y = geom.y + tokens.spacing_sm_px;
    cairo_arc(cr, badge_x + badge_size / 2.0, badge_y + badge_size / 2.0, badge_size / 2.0, 0, 2 * M_PI);
    if (notif.is_critical()) {
        cairo_set_source_rgba(cr, 0.8, 0.2, 0.2, 0.9);
    } else {
        cairo_set_source_rgba(cr, 0.2, 0.45, 0.8, 0.9);
    }
    cairo_fill(cr);

    // App letter in badge
    std::string app_initial = notif.app_name().empty() ? "N" : notif.app_name().substr(0, 1);
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0 * scale);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    cairo_text_extents_t ext;
    cairo_text_extents(cr, app_initial.c_str(), &ext);
    cairo_move_to(cr, badge_x + (badge_size - ext.width) / 2.0,
                  badge_y + (badge_size + ext.height) / 2.0);
    cairo_show_text(cr, app_initial.c_str());

    // App name
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10.0 * scale);
    cairo_set_source_rgba(cr, 0.7, 0.75, 0.85, 1.0);
    cairo_move_to(cr, badge_x + badge_size + tokens.spacing_xs_px, badge_y + badge_size * 0.75);
    cairo_show_text(cr, notif.app_name().empty() ? "Application" : notif.app_name().c_str());

    // Summary line
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 12.0 * scale);
    cairo_set_source_rgba(cr, 0.98, 0.98, 0.98, 1.0);
    cairo_move_to(cr, badge_x, badge_y + badge_size + 14.0 * scale);
    cairo_show_text(cr, notif.summary().c_str());

    // Body line
    if (!notif.body().empty()) {
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 11.0 * scale);
        cairo_set_source_rgba(cr, 0.75, 0.8, 0.85, 0.9);
        cairo_move_to(cr, badge_x, badge_y + badge_size + 28.0 * scale);
        // Truncate for single line preview if needed
        std::string preview_body = notif.body().substr(0, 60);
        cairo_show_text(cr, preview_body.c_str());
    }

    // Dismiss 'X' button on top right
    if (is_popup) {
        const auto& dismiss_rect = notif.dismiss_button_geometry();
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 13.0 * scale);
        cairo_set_source_rgba(cr, 0.6, 0.65, 0.75, 0.8);
        cairo_text_extents(cr, "×", &ext);
        cairo_move_to(cr, dismiss_rect.x + (dismiss_rect.width - ext.width) / 2.0,
                      dismiss_rect.y + (dismiss_rect.height + ext.height) / 2.0);
        cairo_show_text(cr, "×");
    }

    // Action buttons
    for (const auto& act : notif.actions()) {
        if (act.geometry.width <= 0 || act.geometry.height <= 0) continue;

        draw_rounded_rect(cr, act.geometry.x, act.geometry.y, act.geometry.width, act.geometry.height, 6.0 * scale);
        cairo_set_source_rgba(cr, 0.22, 0.26, 0.35, 0.8);
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, 0.35, 0.42, 0.55, 0.6);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 10.5 * scale);
        cairo_set_source_rgba(cr, 0.92, 0.94, 0.98, 1.0);
        cairo_text_extents(cr, act.label.c_str(), &ext);
        cairo_move_to(cr, act.geometry.x + (act.geometry.width - ext.width) / 2.0,
                      act.geometry.y + (act.geometry.height + ext.height) / 2.0);
        cairo_show_text(cr, act.label.c_str());
    }
}

} // namespace ldde::notification

