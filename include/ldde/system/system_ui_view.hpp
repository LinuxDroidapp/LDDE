#pragma once

#include <cairo.h>
#include "ldde/shell/shm_buffer.hpp"
#include "ldde/shell/theme.hpp"
#include "ldde/shell/design_tokens.hpp"
#include "ldde/system/system_ui_layout.hpp"
#include "ldde/system/system_data_provider.hpp"
#include "ldde/system/quick_controls.hpp"

namespace ldde::system {

class SystemUIView {
public:
    static void render_status_bar(
        shell::ShmBuffer& buffer,
        const shell::ShellTheme& theme,
        const shell::DesignTokens& tokens,
        const SystemUILayout& layout,
        const SystemDataProvider& data);

    static void render_system_panel(
        shell::ShmBuffer& buffer,
        const shell::ShellTheme& theme,
        const shell::DesignTokens& tokens,
        const SystemUILayout& layout,
        const SystemDataProvider& data,
        const QuickControlsManager& controls_mgr);
};

} // namespace ldde::system

