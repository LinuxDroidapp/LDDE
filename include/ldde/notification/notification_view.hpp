#pragma once

#include "ldde/shell/shm_buffer.hpp"
#include "ldde/shell/theme.hpp"
#include "ldde/shell/design_tokens.hpp"
#include "ldde/notification/notification.hpp"
#include "ldde/notification/notification_layout.hpp"
#include <cairo/cairo.h>
#include <vector>

namespace ldde::notification {

class NotificationView {
public:
    static void render_popups(
        shell::ShmBuffer& buffer,
        const shell::ShellTheme& theme,
        const shell::DesignTokens& tokens,
        const NotificationLayout& layout,
        const std::vector<const Notification*>& visible_popups);

    static void render_notification_center(
        shell::ShmBuffer& buffer,
        const shell::ShellTheme& theme,
        const shell::DesignTokens& tokens,
        const NotificationLayout& layout,
        const std::vector<const Notification*>& items);

private:
    static void draw_notification_card(
        cairo_t* cr,
        const Notification& notif,
        const core::Rect& geom,
        const shell::DesignTokens& tokens,
        double scale,
        bool is_popup);
};

} // namespace ldde::notification

