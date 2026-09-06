#include "ldde/settings/settings_view.hpp"
#include <cmath>
#include <iomanip>
#include <sstream>

namespace ldde::settings {

namespace {

void draw_rounded_rect(cairo_t* cr, double x, double y, double w, double h, double r) {
    if (w <= 0.0 || h <= 0.0) return;
    r = std::min(r, std::min(w / 2.0, h / 2.0));
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2.0, 0.0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0.0, M_PI / 2.0);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2.0, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3.0 * M_PI / 2.0);
    cairo_close_path(cr);
}

void draw_toggle(cairo_t* cr, const core::Rect& rect, bool is_on) {
    double w = 50.0;
    double h = 28.0;
    double x = rect.x + (rect.width - w) / 2.0;
    double y = rect.y + (rect.height - h) / 2.0;
    double r = h / 2.0;

    // Track
    draw_rounded_rect(cr, x, y, w, h, r);
    if (is_on) {
        cairo_set_source_rgba(cr, 0.29, 0.56, 0.89, 1.0); // Cyan/Blue
    } else {
        cairo_set_source_rgba(cr, 0.25, 0.30, 0.40, 1.0); // Slate
    }
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.2);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    // Knob
    double knob_r = r - 3.0;
    double knob_x = is_on ? (x + w - r) : (x + r);
    double knob_y = y + r;

    cairo_arc(cr, knob_x, knob_y, knob_r, 0.0, 2.0 * M_PI);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    cairo_fill(cr);
}

void draw_slider(cairo_t* cr, const core::Rect& rect, double fraction, const std::string& text) {
    fraction = std::clamp(fraction, 0.0, 1.0);

    double track_w = rect.width - 50.0;
    double track_h = 6.0;
    double track_x = rect.x;
    double track_y = rect.y + (rect.height - track_h) / 2.0;

    // Background track
    draw_rounded_rect(cr, track_x, track_y, track_w, track_h, 3.0);
    cairo_set_source_rgba(cr, 0.22, 0.26, 0.36, 1.0);
    cairo_fill(cr);

    // Filled track
    if (fraction > 0.0) {
        draw_rounded_rect(cr, track_x, track_y, track_w * fraction, track_h, 3.0);
        cairo_set_source_rgba(cr, 0.29, 0.56, 0.89, 1.0);
        cairo_fill(cr);
    }

    // Thumb knob
    double thumb_x = track_x + (track_w * fraction);
    double thumb_y = rect.y + rect.height / 2.0;
    cairo_arc(cr, thumb_x, thumb_y, 8.0, 0.0, 2.0 * M_PI);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0.29, 0.56, 0.89, 1.0);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);

    // Value text
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 12.0);
    cairo_set_source_rgba(cr, 0.85, 0.90, 0.98, 1.0);
    cairo_move_to(cr, rect.x + rect.width - 44.0, rect.y + rect.height / 2.0 + 4.0);
    cairo_show_text(cr, text.c_str());
}

void draw_radio_group(cairo_t* cr, const SettingRowGeometry& row,
                      const std::vector<std::string>& options,
                      const std::string& current_val) {
    for (size_t i = 0; i < row.option_rects.size() && i < options.size(); ++i) {
        const auto& r = row.option_rects[i];
        bool is_selected = (options[i] == current_val);

        draw_rounded_rect(cr, r.x + 2, r.y + 2, r.width - 4, r.height - 4, 6.0);
        if (is_selected) {
            cairo_set_source_rgba(cr, 0.29, 0.56, 0.89, 0.9);
            cairo_fill_preserve(cr);
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.4);
            cairo_set_line_width(cr, 1.5);
            cairo_stroke(cr);
        } else {
            cairo_set_source_rgba(cr, 0.18, 0.22, 0.32, 0.8);
            cairo_fill_preserve(cr);
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.1);
            cairo_set_line_width(cr, 1.0);
            cairo_stroke(cr);
        }

        // Text
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL,
                               is_selected ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 12.0);
        cairo_text_extents_t te;
        cairo_text_extents(cr, options[i].c_str(), &te);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, is_selected ? 1.0 : 0.7);
        cairo_move_to(cr, r.x + (r.width - te.width) / 2.0 - te.x_bearing,
                          r.y + (r.height - te.height) / 2.0 - te.y_bearing);
        cairo_show_text(cr, options[i].c_str());
    }
}

void draw_color_swatch(cairo_t* cr, const core::Rect& rect, const std::string& hex) {
    double swatch_size = 24.0;
    double swatch_x = rect.x + 8.0;
    double swatch_y = rect.y + (rect.height - swatch_size) / 2.0;

    // Parse hex
    auto col_opt = shell::Color::from_hex(hex);
    shell::Color col = col_opt.value_or(shell::Color::from_rgba(74, 144, 226));

    // Color box
    draw_rounded_rect(cr, swatch_x, swatch_y, swatch_size, swatch_size, 4.0);
    cairo_set_source_rgba(cr, col.r, col.g, col.b, 1.0);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.3);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    // Hex text
    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12.0);
    cairo_set_source_rgba(cr, 0.85, 0.90, 0.98, 0.9);
    cairo_move_to(cr, swatch_x + swatch_size + 8.0, rect.y + rect.height / 2.0 + 4.0);
    cairo_show_text(cr, hex.c_str());
}

} // namespace

void SettingsView::render(shell::ShmBuffer& buffer,
                          const shell::ShellTheme& theme,
                          const shell::DesignTokens& /*tokens*/,
                          const SettingsLayout& layout,
                          const SettingsNavigation& navigation,
                          const SettingsStore& store,
                          const std::vector<const SettingDefinition*>& visible_settings,
                          size_t focused_index) {
    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        reinterpret_cast<unsigned char*>(buffer.data()),
        CAIRO_FORMAT_ARGB32,
        buffer.width(),
        buffer.height(),
        buffer.stride());

    cairo_t* cr = cairo_create(surface);

    // 1. Scrim backdrop
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.55);
    cairo_paint(cr);

    // 2. Window surface
    const auto& win = layout.window_rect();
    draw_rounded_rect(cr, win.x, win.y, win.width, win.height, 14.0);
    cairo_set_source_rgba(cr, 0.09, 0.12, 0.18, 0.96); // Dark blue-gray surface
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0.22, 0.28, 0.42, 0.9);  // Clean border
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);

    // 3. Render titlebar & window controls
    render_titlebar(cr, layout, navigation, theme);

    // 4. Render search bar
    render_search_bar(cr, layout, navigation, theme);

    // 5. Render content
    if (!layout.is_portrait()) {
        // Split view: render categories on sidebar, detail on right
        render_categories(cr, layout, navigation, theme);
        if (navigation.active_category() == SettingsCategory::About && !navigation.is_searching()) {
            render_about_page(cr, layout, theme);
        } else {
            render_setting_rows(cr, layout, store, visible_settings, theme, focused_index);
        }
    } else {
        // Mobile portrait view
        if (navigation.is_searching()) {
            render_setting_rows(cr, layout, store, visible_settings, theme, focused_index);
        } else if (!navigation.is_in_category_detail()) {
            render_categories(cr, layout, navigation, theme);
        } else if (navigation.active_category() == SettingsCategory::About) {
            render_about_page(cr, layout, theme);
        } else {
            render_setting_rows(cr, layout, store, visible_settings, theme, focused_index);
        }
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

void SettingsView::render_titlebar(cairo_t* cr,
                                   const SettingsLayout& layout,
                                   const SettingsNavigation& navigation,
                                   const shell::ShellTheme& /*theme*/) {
    const auto& tb = layout.titlebar_rect();

    // Subtle titlebar divider
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.08);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, tb.x, tb.y + tb.height);
    cairo_line_to(cr, tb.x + tb.width, tb.y + tb.height);
    cairo_stroke(cr);

    // Back button in mobile detail
    const auto& bb = layout.back_button_rect();
    if (bb.width > 0) {
        draw_rounded_rect(cr, bb.x, bb.y, bb.width, bb.height, 8.0);
        cairo_set_source_rgba(cr, 0.18, 0.22, 0.32, 0.8);
        cairo_fill(cr);

        // Chevron left '<'
        cairo_set_source_rgba(cr, 0.9, 0.95, 1.0, 0.9);
        cairo_set_line_width(cr, 2.5);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, bb.x + 24, bb.y + 14);
        cairo_line_to(cr, bb.x + 16, bb.y + 20);
        cairo_line_to(cr, bb.x + 24, bb.y + 26);
        cairo_stroke(cr);
    }

    // Title text
    std::string title = "Settings";
    if (layout.is_portrait() && navigation.is_in_category_detail()) {
        title = std::string(category_name(navigation.active_category()));
    }
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 16.0);
    cairo_set_source_rgba(cr, 0.95, 0.97, 1.0, 1.0);
    double title_x = (bb.width > 0) ? (bb.x + bb.width + 12) : (tb.x + 20);
    cairo_move_to(cr, title_x, tb.y + 30);
    cairo_show_text(cr, title.c_str());

    // Window control buttons: Close (Red), Maximize (Green), Minimize (Yellow)
    const auto& cb = layout.close_button_rect();
    draw_rounded_rect(cr, cb.x, cb.y, cb.width, cb.height, 8.0);
    cairo_set_source_rgba(cr, 0.85, 0.25, 0.25, 0.85);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.9);
    cairo_set_line_width(cr, 2.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, cb.x + 14, cb.y + 14);
    cairo_line_to(cr, cb.x + 26, cb.y + 26);
    cairo_move_to(cr, cb.x + 26, cb.y + 14);
    cairo_line_to(cr, cb.x + 14, cb.y + 26);
    cairo_stroke(cr);

    const auto& mb = layout.maximize_button_rect();
    draw_rounded_rect(cr, mb.x, mb.y, mb.width, mb.height, 8.0);
    cairo_set_source_rgba(cr, 0.20, 0.65, 0.35, 0.85);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.9);
    cairo_set_line_width(cr, 1.8);
    cairo_rectangle(cr, mb.x + 14, mb.y + 14, 12, 12);
    cairo_stroke(cr);

    const auto& minb = layout.minimize_button_rect();
    draw_rounded_rect(cr, minb.x, minb.y, minb.width, minb.height, 8.0);
    cairo_set_source_rgba(cr, 0.85, 0.65, 0.15, 0.85);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.9);
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, minb.x + 14, minb.y + 20);
    cairo_line_to(cr, minb.x + 26, minb.y + 20);
    cairo_stroke(cr);
}

void SettingsView::render_search_bar(cairo_t* cr,
                                     const SettingsLayout& layout,
                                     const SettingsNavigation& navigation,
                                     const shell::ShellTheme& /*theme*/) {
    const auto& sb = layout.search_bar_rect();
    draw_rounded_rect(cr, sb.x, sb.y, sb.width, sb.height, 10.0);
    cairo_set_source_rgba(cr, 0.13, 0.17, 0.25, 0.9);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0.25, 0.32, 0.48, 0.7);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    // Magnifying glass icon
    double icon_x = sb.x + 18.0;
    double icon_y = sb.y + sb.height / 2.0 - 2.0;
    cairo_arc(cr, icon_x, icon_y, 6.0, 0.0, 2.0 * M_PI);
    cairo_set_source_rgba(cr, 0.6, 0.7, 0.85, 0.8);
    cairo_set_line_width(cr, 1.8);
    cairo_stroke(cr);
    cairo_move_to(cr, icon_x + 4.5, icon_y + 4.5);
    cairo_line_to(cr, icon_x + 9.0, icon_y + 9.0);
    cairo_stroke(cr);

    // Text query or placeholder
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 13.0);
    if (navigation.is_searching()) {
        cairo_set_source_rgba(cr, 0.95, 0.97, 1.0, 1.0);
        cairo_move_to(cr, sb.x + 36.0, sb.y + 26.0);
        cairo_show_text(cr, navigation.search_query().c_str());

        // Clear button
        const auto& cb = layout.search_clear_rect();
        cairo_arc(cr, cb.x + 14, cb.y + 14, 10.0, 0.0, 2.0 * M_PI);
        cairo_set_source_rgba(cr, 0.3, 0.35, 0.48, 0.8);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.9);
        cairo_set_line_width(cr, 1.5);
        cairo_move_to(cr, cb.x + 10, cb.y + 10);
        cairo_line_to(cr, cb.x + 18, cb.y + 18);
        cairo_move_to(cr, cb.x + 18, cb.y + 10);
        cairo_line_to(cr, cb.x + 10, cb.y + 18);
        cairo_stroke(cr);
    } else {
        cairo_set_source_rgba(cr, 0.45, 0.52, 0.65, 0.8);
        cairo_move_to(cr, sb.x + 36.0, sb.y + 26.0);
        cairo_show_text(cr, "Search preferences, display, dock, system...");
    }
}

void SettingsView::render_categories(cairo_t* cr,
                                     const SettingsLayout& layout,
                                     const SettingsNavigation& navigation,
                                     const shell::ShellTheme& /*theme*/) {
    for (const auto& [cat, rect] : layout.category_rects()) {
        bool is_active = (cat == navigation.active_category());

        draw_rounded_rect(cr, rect.x, rect.y, rect.width, rect.height, 8.0);
        if (is_active && !layout.is_portrait()) {
            cairo_set_source_rgba(cr, 0.29, 0.56, 0.89, 0.3);
            cairo_fill_preserve(cr);
            cairo_set_source_rgba(cr, 0.29, 0.56, 0.89, 0.8);
            cairo_set_line_width(cr, 1.5);
            cairo_stroke(cr);
        } else {
            cairo_set_source_rgba(cr, 0.12, 0.15, 0.23, 0.7);
            cairo_fill_preserve(cr);
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.05);
            cairo_set_line_width(cr, 1.0);
            cairo_stroke(cr);
        }

        // Category dot icon
        cairo_arc(cr, rect.x + 18, rect.y + rect.height / 2.0, 5.0, 0.0, 2.0 * M_PI);
        if (is_active) {
            cairo_set_source_rgba(cr, 0.29, 0.56, 0.89, 1.0);
        } else {
            cairo_set_source_rgba(cr, 0.55, 0.62, 0.75, 0.8);
        }
        cairo_fill(cr);

        // Label
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL,
                               is_active ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 14.0);
        cairo_set_source_rgba(cr, 0.95, 0.97, 1.0, is_active ? 1.0 : 0.85);
        cairo_move_to(cr, rect.x + 32.0, rect.y + rect.height / 2.0 + 5.0);
        cairo_show_text(cr, std::string(category_name(cat)).c_str());

        // In portrait mode, show right chevron '>'
        if (layout.is_portrait()) {
            double ch_x = rect.x + rect.width - 24;
            double ch_y = rect.y + rect.height / 2.0;
            cairo_set_source_rgba(cr, 0.45, 0.52, 0.65, 0.8);
            cairo_set_line_width(cr, 2.0);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            cairo_move_to(cr, ch_x - 4, ch_y - 6);
            cairo_line_to(cr, ch_x + 2, ch_y);
            cairo_line_to(cr, ch_x - 4, ch_y + 6);
            cairo_stroke(cr);
        }
    }
}

void SettingsView::render_setting_rows(cairo_t* cr,
                                       const SettingsLayout& layout,
                                       const SettingsStore& store,
                                       const std::vector<const SettingDefinition*>& visible_settings,
                                       const shell::ShellTheme& /*theme*/,
                                       size_t focused_index) {
    const auto& clip = layout.content_rect();
    cairo_save(cr);
    cairo_rectangle(cr, clip.x, clip.y, clip.width, clip.height);
    cairo_clip(cr);

    const auto& rows = layout.setting_rows();
    for (size_t i = 0; i < rows.size() && i < visible_settings.size(); ++i) {
        const auto& row = rows[i];
        const auto* def = visible_settings[i];
        if (!def) continue;

        // Skip rows outside vertical view
        if (row.row_rect.y + row.row_rect.height < clip.y || row.row_rect.y > clip.y + clip.height) {
            continue;
        }

        bool is_focused = (i == focused_index);

        // Row card background
        draw_rounded_rect(cr, row.row_rect.x, row.row_rect.y, row.row_rect.width, row.row_rect.height, 8.0);
        cairo_set_source_rgba(cr, 0.12, 0.15, 0.22, 0.85);
        cairo_fill_preserve(cr);
        if (is_focused) {
            cairo_set_source_rgba(cr, 0.29, 0.56, 0.89, 0.9);
            cairo_set_line_width(cr, 1.5);
        } else {
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.06);
            cairo_set_line_width(cr, 1.0);
        }
        cairo_stroke(cr);

        // Title
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 14.0);
        cairo_set_source_rgba(cr, 0.95, 0.97, 1.0, 1.0);
        cairo_move_to(cr, row.row_rect.x + 14.0, row.row_rect.y + 24.0);
        cairo_show_text(cr, def->title.c_str());

        // Description
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 11.5);
        cairo_set_source_rgba(cr, 0.65, 0.72, 0.85, 0.8);
        cairo_move_to(cr, row.row_rect.x + 14.0, row.row_rect.y + 44.0);
        cairo_show_text(cr, def->description.c_str());

        // Value widget
        auto current_val = store.get(def->key).value_or(def->default_value);

        if (def->type == SettingType::Bool) {
            draw_toggle(cr, row.control_rect, current_val.as_bool().value_or(false));
        } else if (def->type == SettingType::Int || def->type == SettingType::Double) {
            double v = current_val.as_double().value_or(0.0);
            double min_v = def->min_value.value_or(0.0);
            double max_v = def->max_value.value_or(100.0);
            double frac = (max_v > min_v) ? (v - min_v) / (max_v - min_v) : 0.0;
            draw_slider(cr, row.control_rect, frac, current_val.to_string());
        } else if (def->type == SettingType::Enum) {
            draw_radio_group(cr, row, def->enum_values, current_val.to_string());
        } else if (def->type == SettingType::String) {
            draw_color_swatch(cr, row.control_rect, current_val.to_string());
        }
    }

    cairo_restore(cr);
}

void SettingsView::render_about_page(cairo_t* cr,
                                     const SettingsLayout& layout,
                                     const shell::ShellTheme& /*theme*/) {
    const auto& clip = layout.content_rect();
    cairo_save(cr);
    cairo_rectangle(cr, clip.x, clip.y, clip.width, clip.height);
    cairo_clip(cr);

    double cx = clip.x + clip.width / 2.0;
    double y = clip.y + 36.0;

    // Monogram emblem
    cairo_arc(cr, cx, y + 32.0, 32.0, 0.0, 2.0 * M_PI);
    cairo_set_source_rgba(cr, 0.29, 0.56, 0.89, 0.2);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0.29, 0.56, 0.89, 0.8);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 20.0);
    cairo_set_source_rgba(cr, 0.95, 0.98, 1.0, 1.0);
    cairo_text_extents_t te;
    cairo_text_extents(cr, "LDDE", &te);
    cairo_move_to(cr, cx - te.width / 2.0 - te.x_bearing, y + 32.0 - te.height / 2.0 - te.y_bearing);
    cairo_show_text(cr, "LDDE");

    y += 84.0;

    // Name & Version
    cairo_set_font_size(cr, 16.0);
    cairo_text_extents(cr, "LinuxDroid Desktop Environment", &te);
    cairo_move_to(cr, cx - te.width / 2.0, y);
    cairo_show_text(cr, "LinuxDroid Desktop Environment");

    y += 24.0;
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 13.0);
    cairo_set_source_rgba(cr, 0.65, 0.72, 0.85, 0.9);
    cairo_text_extents(cr, "Version 1.0.0 (C++20)", &te);
    cairo_move_to(cr, cx - te.width / 2.0, y);
    cairo_show_text(cr, "Version 1.0.0 (C++20)");

    y += 20.0;
    cairo_text_extents(cr, "Wayland Compositor: Weston", &te);
    cairo_move_to(cr, cx - te.width / 2.0, y);
    cairo_show_text(cr, "Wayland Compositor: Weston");

    y += 36.0;

    // Reset button card
    double btn_w = 200.0;
    double btn_h = 42.0;
    double btn_x = cx - btn_w / 2.0;

    draw_rounded_rect(cr, btn_x, y, btn_w, btn_h, 8.0);
    cairo_set_source_rgba(cr, 0.75, 0.25, 0.25, 0.8);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.3);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 13.0);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    cairo_text_extents(cr, "Reset All Preferences", &te);
    cairo_move_to(cr, cx - te.width / 2.0 - te.x_bearing, y + btn_h / 2.0 - te.height / 2.0 - te.y_bearing);
    cairo_show_text(cr, "Reset All Preferences");

    cairo_restore(cr);
}

} // namespace ldde::settings
