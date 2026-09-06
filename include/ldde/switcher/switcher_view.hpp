#pragma once

#include <string_view>
#include <cairo.h>
#include "ldde/shell/shm_buffer.hpp"
#include "ldde/shell/theme.hpp"
#include "ldde/shell/design_tokens.hpp"
#include "ldde/switcher/switcher_layout.hpp"
#include "ldde/switcher/switcher_model.hpp"
#include "ldde/switcher/switcher_state.hpp"
#include "ldde/launcher/launcher_icon_resolver.hpp"

namespace ldde::switcher {

class SwitcherView {
public:
    SwitcherView() = default;
    ~SwitcherView() = default;

    void render(shell::ShmBuffer& buffer,
                const shell::ShellTheme& theme,
                const shell::DesignTokens& tokens,
                const SwitcherLayout& layout,
                const SwitcherModel& model,
                SwitcherState state,
                size_t selected_index,
                launcher::LauncherIconResolver& icon_resolver);

private:
    static void set_color(cairo_t* cr, const shell::Color& c);
    static void draw_rounded_rect(cairo_t* cr, double x, double y, double w, double h, double r);
    static void draw_icon(cairo_t* cr,
                          launcher::LauncherIconResolver& icon_resolver,
                          const SwitcherItem& item,
                          double x, double y, double size);
};

} // namespace ldde::switcher
