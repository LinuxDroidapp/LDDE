#pragma once

#include <cairo.h>
#include <vector>
#include <string>
#include "ldde/shell/shm_buffer.hpp"
#include "ldde/shell/theme.hpp"
#include "ldde/shell/design_tokens.hpp"
#include "ldde/settings/settings_types.hpp"
#include "ldde/settings/settings_schema.hpp"
#include "ldde/settings/settings_store.hpp"
#include "ldde/settings/settings_layout.hpp"
#include "ldde/settings/settings_navigation.hpp"

namespace ldde::settings {

class SettingsView {
public:
    SettingsView() = default;
    ~SettingsView() = default;

    void render(shell::ShmBuffer& buffer,
                const shell::ShellTheme& theme,
                const shell::DesignTokens& tokens,
                const SettingsLayout& layout,
                const SettingsNavigation& navigation,
                const SettingsStore& store,
                const std::vector<const SettingDefinition*>& visible_settings,
                size_t focused_index = 0);

private:
    void render_titlebar(cairo_t* cr,
                         const SettingsLayout& layout,
                         const SettingsNavigation& navigation,
                         const shell::ShellTheme& theme);

    void render_search_bar(cairo_t* cr,
                           const SettingsLayout& layout,
                           const SettingsNavigation& navigation,
                           const shell::ShellTheme& theme);

    void render_categories(cairo_t* cr,
                           const SettingsLayout& layout,
                           const SettingsNavigation& navigation,
                           const shell::ShellTheme& theme);

    void render_setting_rows(cairo_t* cr,
                             const SettingsLayout& layout,
                             const SettingsStore& store,
                             const std::vector<const SettingDefinition*>& visible_settings,
                             const shell::ShellTheme& theme,
                             size_t focused_index);

    void render_about_page(cairo_t* cr,
                           const SettingsLayout& layout,
                           const shell::ShellTheme& theme);
};

} // namespace ldde::settings
