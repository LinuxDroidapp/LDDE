#include "ldde/desktop/desktop_view.hpp"
#include "ldde/core/logging.hpp"
#include <cairo/cairo.h>

namespace ldde::desktop {

DesktopView::DesktopView() = default;

void DesktopView::load_config(const config::Config& config) {
    show_empty_hint_ = config.get_bool_or("desktop", "show_empty_hint", true);
}

void DesktopView::render(shell::ShmBuffer& buffer,
                         DesktopBackground& background,
                         const DesktopLayout& layout,
                         const DesktopModel& model) {
    if (!buffer.data() || buffer.width() <= 0 || buffer.height() <= 0) return;

    // 1. Render primary background
    background.render(buffer, layout.scale());

    // 2. Render subtle empty-state branding when no windows are active
    if (model.is_empty() && show_empty_hint_) {
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

        const auto& ws = layout.workspace_bounds();
        double center_x = ws.x + ws.width * 0.5;
        double center_y = ws.y + ws.height * 0.48;

        // Subtle desktop title
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        double font_size = 28.0 * layout.scale();
        cairo_set_font_size(cr, font_size);

        cairo_text_extents_t extents;
        const char* title = "LinuxDroid";
        cairo_text_extents(cr, title, &extents);

        cairo_move_to(cr, center_x - (extents.width * 0.5 + extents.x_bearing),
                          center_y - (extents.height * 0.5 + extents.y_bearing));
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.14);
        cairo_show_text(cr, title);

        // Subtle desktop subtitle
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        double sub_size = 13.0 * layout.scale();
        cairo_set_font_size(cr, sub_size);

        const char* subtitle = "Desktop Environment";
        cairo_text_extents(cr, subtitle, &extents);
        cairo_move_to(cr, center_x - (extents.width * 0.5 + extents.x_bearing),
                          center_y + font_size * 0.8);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.10);
        cairo_show_text(cr, subtitle);

        cairo_destroy(cr);
        cairo_surface_destroy(surface);
    }
}

} // namespace ldde::desktop
