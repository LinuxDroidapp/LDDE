#pragma once

#include <cairo.h>
#include "ldde/shell/shm_buffer.hpp"
#include "ldde/shell/theme.hpp"
#include "ldde/shell/design_tokens.hpp"
#include "ldde/launcher/launcher_state.hpp"
#include "ldde/launcher/launcher_layout.hpp"
#include "ldde/launcher/launcher_model.hpp"

namespace ldde::launcher {

class LauncherView {
public:
    LauncherView() = default;
    ~LauncherView() = default;

    static void render(
        shell::ShmBuffer& buffer,
        const shell::ShellTheme& theme,
        const shell::DesignTokens& tokens,
        const LauncherLayout& layout,
        const LauncherModel& model,
        LauncherState state,
        int scroll_y = 0,
        std::string_view last_error_message = {});

private:
    static void draw_rounded_rect(cairo_t* cr, double x, double y, double w, double h, double r);
    static void set_color(cairo_t* cr, const shell::Color& c);
};

} // namespace ldde::launcher
