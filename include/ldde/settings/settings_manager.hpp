#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include "ldde/core/error.hpp"
#include "ldde/config/config.hpp"
#include "ldde/shell/shell.hpp"
#include "ldde/display/display_policy.hpp"
#include "ldde/window/window_registry.hpp"
#include "ldde/window/window_manager.hpp"
#include "ldde/application/application_catalog.hpp"
#include "ldde/settings/settings_types.hpp"
#include "ldde/settings/settings_schema.hpp"
#include "ldde/settings/settings_store.hpp"
#include "ldde/settings/settings_navigation.hpp"
#include "ldde/settings/settings_layout.hpp"
#include "ldde/settings/settings_search.hpp"
#include "ldde/settings/settings_view.hpp"
#include "ldde/settings/settings_controller.hpp"

namespace ldde::settings {

class SettingsManager {
public:
    using RenderRequestCallback = std::function<void()>;
    using StateChangedCallback = std::function<void(SettingsWindowMode mode)>;

    SettingsManager();
    ~SettingsManager();

    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;

    core::Status initialize(shell::Shell& shell,
                            window::WindowRegistry& window_registry,
                            window::WindowManager& window_manager,
                            application::ApplicationCatalog& catalog,
                            const display::DisplayPolicy& policy,
                            config::Config& config);
    void shutdown() noexcept;

    [[nodiscard]] bool is_initialized() const noexcept { return initialized_; }

    // Window lifecycle & Presentation
    void open();
    void close();
    void toggle();
    void minimize();
    void maximize();
    void restore();

    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] bool is_minimized() const noexcept { return mode_ == SettingsWindowMode::Minimized; }
    [[nodiscard]] bool is_maximized() const noexcept { return mode_ == SettingsWindowMode::Maximized; }
    [[nodiscard]] SettingsWindowMode window_mode() const noexcept { return mode_; }

    [[nodiscard]] std::optional<window::WindowId> window_id() const noexcept { return window_id_; }

    // Display policy adaptation
    void update_display_policy(const display::DisplayPolicy& policy);

    // Cairo Rendering
    void render(shell::ShmBuffer& buffer,
                const shell::ShellTheme& theme,
                const shell::DesignTokens& tokens);

    // Input handlers
    bool handle_touch_down(int32_t x, int32_t y);
    bool handle_touch_motion(int32_t x, int32_t y);
    bool handle_touch_up(int32_t x, int32_t y);
    void handle_touch_cancel();

    bool handle_pointer_motion(int32_t x, int32_t y);
    bool handle_pointer_button(uint32_t button, uint32_t state, int32_t x, int32_t y);
    bool handle_pointer_axis(double delta_x, double delta_y);

    bool handle_key(uint32_t key_symbol, uint32_t state = 1, uint32_t modifiers = 0);

    // Callbacks
    void on_request_render(RenderRequestCallback cb) { on_request_render_ = std::move(cb); }
    void on_state_changed(StateChangedCallback cb) { on_state_changed_ = std::move(cb); }

    // Accessors
    [[nodiscard]] SettingsStore& store() noexcept { return *store_; }
    [[nodiscard]] const SettingsStore& store() const noexcept { return *store_; }
    [[nodiscard]] SettingsNavigation& navigation() noexcept { return navigation_; }
    [[nodiscard]] const SettingsNavigation& navigation() const noexcept { return navigation_; }
    [[nodiscard]] SettingsLayout& layout() noexcept { return layout_; }
    [[nodiscard]] const SettingsLayout& layout() const noexcept { return layout_; }
    [[nodiscard]] SettingsController& controller() noexcept { return *controller_; }

private:
    bool initialized_ = false;

    shell::Shell* shell_ = nullptr;
    window::WindowRegistry* window_registry_ = nullptr;
    window::WindowManager* window_manager_ = nullptr;
    application::ApplicationCatalog* catalog_ = nullptr;

    display::DisplayPolicy display_policy_;
    SettingsSchema schema_;
    std::unique_ptr<SettingsStore> store_;
    SettingsNavigation navigation_;
    SettingsLayout layout_;
    std::unique_ptr<SettingsSearch> search_;
    SettingsView view_;
    std::unique_ptr<SettingsController> controller_;

    SettingsWindowMode mode_ = SettingsWindowMode::Closed;
    std::shared_ptr<window::Window> window_;
    std::optional<window::WindowId> window_id_;

    RenderRequestCallback on_request_render_;
    StateChangedCallback on_state_changed_;

    void set_mode(SettingsWindowMode new_mode);
    void ensure_window_created();
    void register_desktop_entry();
};

} // namespace ldde::settings
