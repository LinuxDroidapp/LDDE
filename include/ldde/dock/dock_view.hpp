#pragma once

#include <cairo.h>
#include "ldde/shell/shm_buffer.hpp"
#include "ldde/shell/theme.hpp"
#include "ldde/shell/design_tokens.hpp"
#include "ldde/dock/dock_layout.hpp"
#include "ldde/dock/dock_model.hpp"
#include "ldde/launcher/launcher_icon_resolver.hpp"

namespace ldde::dock {

class DockView {
public:
    DockView() = default;

    void render(shell::ShmBuffer& buffer,
                const shell::ShellTheme& theme,
                const shell::DesignTokens& tokens,
                const DockLayout& layout,
                const DockModel& model,
                launcher::LauncherIconResolver& icon_resolver,
                int32_t hovered_index = -1,
                int32_t pressed_index = -1);

private:
    void render_launcher_button(cairo_t* cr,
                                const core::Rect& rect,
                                const shell::ShellTheme& theme,
                                bool is_hovered,
                                bool is_pressed);

    void render_dock_item(cairo_t* cr,
                          const DockItem& item,
                          const core::Rect& rect,
                          const shell::ShellTheme& theme,
                          const shell::DesignTokens& tokens,
                          launcher::LauncherIconResolver& icon_resolver,
                          bool is_hovered,
                          bool is_pressed);

    void render_fallback_badge(cairo_t* cr,
                               const std::string& name,
                               double x, double y, double size);
};

} // namespace ldde::dock
