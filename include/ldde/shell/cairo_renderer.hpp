#pragma once

#include <cairo.h>
#include <string_view>
#include "ldde/shell/shm_buffer.hpp"
#include "ldde/shell/theme.hpp"
#include "ldde/shell/design_tokens.hpp"
#include "ldde/core/types.hpp"

namespace ldde::shell {

class CairoRenderer {
public:
    CairoRenderer() = default;
    ~CairoRenderer() = default;

    static void render_desktop(ShmBuffer& buffer, const ShellTheme& theme);

    static void render_status_region(ShmBuffer& buffer,
                                     const ShellTheme& theme,
                                     const DesignTokens& tokens,
                                     std::string_view clock_text = "12:00");

    static void render_dock_region(ShmBuffer& buffer,
                                   const ShellTheme& theme,
                                   const DesignTokens& tokens,
                                   int slot_count = 4);

    static void render_overlay(ShmBuffer& buffer,
                               const ShellTheme& theme,
                               const core::Rect& modal_rect);

    // Helpers
    static void draw_rounded_rect(cairo_t* cr, double x, double y, double width, double height, double radius);
    static void set_cairo_color(cairo_t* cr, const Color& color);
};

} // namespace ldde::shell
