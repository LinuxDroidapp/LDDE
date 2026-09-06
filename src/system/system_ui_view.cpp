#include "ldde/system/system_ui_view.hpp"
#include <cmath>
#include <algorithm>

namespace ldde::system {

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

void SystemUIView::render_status_bar(
    shell::ShmBuffer& buffer,
    const shell::ShellTheme& theme,
    const shell::DesignTokens& tokens,
    const SystemUILayout& layout,
    const SystemDataProvider& data) {
    if (!buffer.data() || buffer.width() <= 0 || buffer.height() <= 0) return;

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
    draw_rounded_rect(cr, pad, pad, w - pad * 2, h - pad * 2, tokens.dock_corner_radius_px * 0.4);
    set_cairo_color(cr, theme.status_bg);
    cairo_fill(cr);

    // 1. Clock on the left
    const auto& clock_info = data.clock().info();
    set_cairo_color(cr, theme.status_text);
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    double font_size = std::max(12.0, 14.0 * tokens.scale);
    cairo_set_font_size(cr, font_size);

    cairo_text_extents_t extents;
    cairo_text_extents(cr, clock_info.formatted_time.c_str(), &extents);

    double text_x = pad + tokens.spacing_md_px;
    double text_y = (h / 2.0) - (extents.height / 2.0 + extents.y_bearing);
    cairo_move_to(cr, text_x, text_y);
    cairo_show_text(cr, clock_info.formatted_time.c_str());

    // 2. Right Indicators: Session, Battery, Audio, Network
    // Session indicator (status dot)
    const auto& session_geom = layout.session_icon_geometry();
    double sess_cx = session_geom.x + session_geom.width / 2.0;
    double sess_cy = session_geom.y + session_geom.height / 2.0;
    double dot_r = 3.5 * tokens.scale;
    cairo_arc(cr, sess_cx, sess_cy, dot_r, 0, 2.0 * M_PI);
    if (data.session().info().state == SessionState::Active) {
        cairo_set_source_rgba(cr, 0.2, 0.8, 0.4, 0.9);
    } else {
        cairo_set_source_rgba(cr, 0.6, 0.6, 0.6, 0.7);
    }
    cairo_fill(cr);

    // Battery indicator (pill outline with fill)
    const auto& batt_geom = layout.battery_icon_geometry();
    const auto& batt_info = data.battery().info();
    if (batt_info.is_available) {
        double bw = 20.0 * tokens.scale;
        double bh = 10.0 * tokens.scale;
        double bx = batt_geom.x + (batt_geom.width - bw) / 2.0;
        double by = batt_geom.y + (batt_geom.height - bh) / 2.0;

        // Outline
        draw_rounded_rect(cr, bx, by, bw, bh, 2.0 * tokens.scale);
        cairo_set_source_rgba(cr, 0.8, 0.8, 0.8, 0.6);
        cairo_set_line_width(cr, 1.2 * tokens.scale);
        cairo_stroke(cr);

        // Terminal nipple
        cairo_rectangle(cr, bx + bw, by + bh * 0.3, 2.0 * tokens.scale, bh * 0.4);
        cairo_set_source_rgba(cr, 0.8, 0.8, 0.8, 0.6);
        cairo_fill(cr);

        // Fill based on percentage
        double fill_w = std::max(2.0, (bw - 3.0) * (batt_info.percentage / 100.0));
        draw_rounded_rect(cr, bx + 1.5, by + 1.5, fill_w, bh - 3.0, 1.0 * tokens.scale);
        if (batt_info.is_charging) {
            cairo_set_source_rgba(cr, 0.3, 0.8, 1.0, 0.9); // cyan charging
        } else if (batt_info.percentage <= 15) {
            cairo_set_source_rgba(cr, 0.95, 0.3, 0.3, 0.9); // red low
        } else {
            cairo_set_source_rgba(cr, 0.3, 0.85, 0.45, 0.9); // green ok
        }
        cairo_fill(cr);
    }

    // Audio indicator (Speaker icon)
    const auto& audio_geom = layout.audio_icon_geometry();
    const auto& audio_info = data.audio().info();
    if (audio_info.is_available) {
        double ax = audio_geom.x + audio_geom.width / 2.0;
        double ay = audio_geom.y + audio_geom.height / 2.0;
        double sz = 6.0 * tokens.scale;

        cairo_new_sub_path(cr);
        cairo_move_to(cr, ax - sz * 0.6, ay - sz * 0.4);
        cairo_line_to(cr, ax - sz * 0.2, ay - sz * 0.4);
        cairo_line_to(cr, ax + sz * 0.4, ay - sz * 0.8);
        cairo_line_to(cr, ax + sz * 0.4, ay + sz * 0.8);
        cairo_line_to(cr, ax - sz * 0.2, ay + sz * 0.4);
        cairo_line_to(cr, ax - sz * 0.6, ay + sz * 0.4);
        cairo_close_path(cr);

        if (audio_info.is_muted) {
            cairo_set_source_rgba(cr, 0.8, 0.4, 0.4, 0.8);
            cairo_fill(cr);
            // Mute slash
            cairo_move_to(cr, ax - sz * 0.8, ay - sz * 0.8);
            cairo_line_to(cr, ax + sz * 0.8, ay + sz * 0.8);
            cairo_set_source_rgba(cr, 0.9, 0.3, 0.3, 0.9);
            cairo_set_line_width(cr, 1.5 * tokens.scale);
            cairo_stroke(cr);
        } else {
            cairo_set_source_rgba(cr, 0.85, 0.85, 0.85, 0.9);
            cairo_fill(cr);
            // Sound wave arc
            cairo_arc(cr, ax + sz * 0.2, ay, sz * 0.8, -M_PI_4, M_PI_4);
            cairo_set_source_rgba(cr, 0.85, 0.85, 0.85, 0.7);
            cairo_set_line_width(cr, 1.2 * tokens.scale);
            cairo_stroke(cr);
        }
    }

    // Network indicator
    const auto& net_geom = layout.network_icon_geometry();
    const auto& net_info = data.network().info();
    double nx = net_geom.x + net_geom.width / 2.0;
    double ny = net_geom.y + net_geom.height / 2.0;
    double nsz = 6.0 * tokens.scale;

    if (net_info.state == NetworkState::Connected) {
        cairo_arc(cr, nx, ny + nsz * 0.5, 2.0 * tokens.scale, 0, 2.0 * M_PI);
        cairo_set_source_rgba(cr, 0.9, 0.9, 0.9, 0.9);
        cairo_fill(cr);

        cairo_arc(cr, nx, ny + nsz * 0.5, nsz * 0.7, -M_PI * 0.75, -M_PI * 0.25);
        cairo_set_source_rgba(cr, 0.9, 0.9, 0.9, 0.9);
        cairo_set_line_width(cr, 1.5 * tokens.scale);
        cairo_stroke(cr);

        cairo_arc(cr, nx, ny + nsz * 0.5, nsz * 1.2, -M_PI * 0.75, -M_PI * 0.25);
        cairo_set_source_rgba(cr, 0.9, 0.9, 0.9, 0.9);
        cairo_set_line_width(cr, 1.5 * tokens.scale);
        cairo_stroke(cr);
    } else {
        // Disconnected or unavailable
        cairo_arc(cr, nx, ny + nsz * 0.5, 2.0 * tokens.scale, 0, 2.0 * M_PI);
        cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.5);
        cairo_fill(cr);
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

void SystemUIView::render_system_panel(
    shell::ShmBuffer& buffer,
    const shell::ShellTheme& /*theme*/,
    const shell::DesignTokens& tokens,
    const SystemUILayout& layout,
    const SystemDataProvider& data,
    const QuickControlsManager& controls_mgr) {
    if (!buffer.data() || buffer.width() <= 0 || buffer.height() <= 0) return;

    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        static_cast<unsigned char*>(buffer.data()),
        CAIRO_FORMAT_ARGB32,
        buffer.width(),
        buffer.height(),
        buffer.stride());

    cairo_t* cr = cairo_create(surface);

    // 1. Scrim backdrop over entire overlay
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.45);
    cairo_paint(cr);

    // 2. System panel floating card
    const auto& panel_rect = layout.panel_geometry();
    double px = panel_rect.x;
    double py = panel_rect.y;
    double pw = panel_rect.width;
    double ph = panel_rect.height;
    double radius = tokens.dock_corner_radius_px > 0 ? tokens.dock_corner_radius_px : 20.0;

    draw_rounded_rect(cr, px, py, pw, ph, radius);
    // Frosted dark background
    cairo_set_source_rgba(cr, 0.08, 0.11, 0.17, 0.96);
    cairo_fill_preserve(cr);

    // Card border
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.12);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    double pad = tokens.spacing_md_px;
    double cur_y = py + pad;

    // 3. Header: Time, Date & Title
    const auto& clock_info = data.clock().info();
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 18.0 * tokens.scale);
    cairo_set_source_rgba(cr, 0.95, 0.95, 0.98, 1.0);
    cairo_move_to(cr, px + pad, cur_y + 18.0 * tokens.scale);
    cairo_show_text(cr, clock_info.formatted_time.c_str());

    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 13.0 * tokens.scale);
    cairo_set_source_rgba(cr, 0.65, 0.70, 0.78, 1.0);
    cairo_move_to(cr, px + pad, cur_y + 36.0 * tokens.scale);
    cairo_show_text(cr, clock_info.formatted_date.c_str());

    cur_y += 48.0 * tokens.scale;

    // 4. Status Summary Card
    double sum_w = pw - pad * 2;
    double sum_h = 76.0 * tokens.scale;
    draw_rounded_rect(cr, px + pad, cur_y, sum_w, sum_h, 12.0 * tokens.scale);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.05);
    cairo_fill(cr);

    // Network line
    const auto& net_info = data.network().info();
    cairo_set_font_size(cr, 12.0 * tokens.scale);
    cairo_set_source_rgba(cr, 0.85, 0.88, 0.92, 1.0);
    cairo_move_to(cr, px + pad + 12.0, cur_y + 22.0 * tokens.scale);
    std::string net_str = "Network: " + net_info.status_label;
    cairo_show_text(cr, net_str.c_str());

    // Audio & Battery line
    const auto& batt_info = data.battery().info();
    const auto& audio_info = data.audio().info();
    std::string batt_str = batt_info.is_available ?
        ("Battery: " + std::to_string(batt_info.percentage) + "%" + (batt_info.is_charging ? " (Charging)" : "")) :
        "Battery: Unavailable";
    cairo_move_to(cr, px + pad + 12.0, cur_y + 44.0 * tokens.scale);
    cairo_show_text(cr, batt_str.c_str());

    std::string audio_str = audio_info.is_available ?
        ("Audio: " + (audio_info.is_muted ? "Muted" : std::to_string(audio_info.volume_percent) + "%")) :
        "Audio: Unavailable";
    cairo_move_to(cr, px + pad + 12.0, cur_y + 64.0 * tokens.scale);
    cairo_show_text(cr, audio_str.c_str());

    cur_y += sum_h + tokens.spacing_sm_px;

    // 5. Quick Controls Section
    for (size_t i = 0; i < controls_mgr.control_count(); ++i) {
        const auto* ctrl = controls_mgr.control_at(i);
        const auto* geom = layout.control_tile_geometry(i);
        if (!ctrl || !geom) continue;

        double tx = geom->x;
        double ty = geom->y;
        double tw = geom->width;
        double th = geom->height;

        draw_rounded_rect(cr, tx, ty, tw, th, 10.0 * tokens.scale);

        if (ctrl->is_active) {
            cairo_set_source_rgba(cr, 0.18, 0.42, 0.85, 0.50);
        } else {
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.08);
        }
        cairo_fill_preserve(cr);

        // Border: highlight if selected
        if (static_cast<int32_t>(i) == controls_mgr.selected_index()) {
            cairo_set_source_rgba(cr, 0.4, 0.7, 1.0, 0.9);
            cairo_set_line_width(cr, 2.0 * tokens.scale);
        } else {
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.10);
            cairo_set_line_width(cr, 1.0 * tokens.scale);
        }
        cairo_stroke(cr);

        // Control Label & Status text
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 13.0 * tokens.scale);
        cairo_set_source_rgba(cr, 0.95, 0.95, 0.98, 1.0);
        cairo_move_to(cr, tx + 12.0 * tokens.scale, ty + 22.0 * tokens.scale);
        cairo_show_text(cr, ctrl->label.c_str());

        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 11.0 * tokens.scale);
        cairo_set_source_rgba(cr, 0.65, 0.70, 0.78, 1.0);
        cairo_move_to(cr, tx + 12.0 * tokens.scale, ty + 40.0 * tokens.scale);
        cairo_show_text(cr, ctrl->status_text.c_str());
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

} // namespace ldde::system
