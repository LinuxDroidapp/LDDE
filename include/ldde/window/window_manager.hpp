#pragma once

#include <memory>
#include <vector>
#include <optional>
#include "ldde/core/types.hpp"
#include "ldde/core/error.hpp"
#include "ldde/config/config.hpp"
#include "ldde/display/display_manager.hpp"
#include "ldde/window/types.hpp"
#include "ldde/window/window.hpp"
#include "ldde/window/window_registry.hpp"
#include "ldde/window/window_tracker.hpp"
#include "ldde/window/window_management_backend.hpp"
#include "ldde/window/window_placement.hpp"
#include "ldde/window/window_state_controller.hpp"
#include "ldde/window/window_focus.hpp"
#include "ldde/window/window_stacking.hpp"
#include "ldde/window/window_interaction.hpp"
#include "ldde/window/window_controls.hpp"

namespace ldde::window {

using core::Status;

class WindowManager {
public:
    WindowManager(WindowRegistry& registry,
                  WindowTracker& tracker,
                  display::DisplayManager& display_mgr,
                  std::unique_ptr<WindowManagementBackend> backend = nullptr);
    ~WindowManager();

    WindowManager(const WindowManager&) = delete;
    WindowManager& operator=(const WindowManager&) = delete;

    Status initialize(const config::Config& config);
    void shutdown() noexcept;

    [[nodiscard]] bool is_initialized() const noexcept { return initialized_; }

    // Core window management operations
    Status activate(WindowId id);
    Status close(WindowId id);
    Status maximize(WindowId id);
    Status restore(WindowId id);
    Status minimize(WindowId id);
    Status fullscreen(WindowId id);
    Status toggle_maximize(WindowId id);
    Status toggle_fullscreen(WindowId id);

    Status raise(WindowId id);
    Status lower(WindowId id);
    Status set_geometry(WindowId id, const core::Rect& geom);

    // Interactive operations
    bool start_move(WindowId id, const core::Point& start_pos, bool is_touch = false);
    core::Rect update_move(const core::Point& current_pos);
    core::Rect end_move();
    core::Rect cancel_move();

    bool start_resize(WindowId id, ResizeEdge edge, const core::Point& start_pos, bool is_touch = false);
    core::Rect update_resize(const core::Point& current_pos);
    core::Rect end_resize();
    core::Rect cancel_resize();

    // Input handlers
    bool handle_pointer_click(const core::Point& pos, uint32_t timestamp_ms);
    bool handle_touch_tap(const core::Point& pos, uint32_t timestamp_ms);

    // Display adaptation
    void handle_display_change(const display::DisplayPolicy& policy);
    void handle_display_change(const display::DisplayInfo& display);
    void handle_display_removed(display::DisplayId id);

    // Desktop queries
    [[nodiscard]] std::optional<WindowId> active_window_id() const noexcept { return focus_.active_window_id(); }
    [[nodiscard]] std::optional<WindowId> top_window_id() const noexcept { return stacking_.top(); }
    [[nodiscard]] const std::vector<WindowId>& stacking_order() const noexcept { return stacking_.stack(); }
    [[nodiscard]] std::vector<std::shared_ptr<Window>> visible_windows() const;
    [[nodiscard]] std::vector<std::shared_ptr<Window>> minimized_windows() const;

    // Component accessors
    [[nodiscard]] WindowRegistry& registry() noexcept { return registry_; }
    [[nodiscard]] WindowTracker& tracker() noexcept { return tracker_; }
    [[nodiscard]] WindowManagementBackend& backend() noexcept { return *backend_; }
    [[nodiscard]] WindowPlacement& placement() noexcept { return placement_; }
    [[nodiscard]] WindowStateController& state_controller() noexcept { return state_ctrl_; }
    [[nodiscard]] WindowFocus& focus() noexcept { return focus_; }
    [[nodiscard]] WindowStacking& stacking() noexcept { return stacking_; }
    [[nodiscard]] WindowInteraction& interaction() noexcept { return interaction_; }
    [[nodiscard]] WindowControls& controls() noexcept { return controls_; }

private:
    WindowRegistry& registry_;
    WindowTracker& tracker_;
    display::DisplayManager& display_mgr_;
    std::unique_ptr<WindowManagementBackend> backend_;

    WindowPlacement placement_;
    WindowStateController state_ctrl_;
    WindowFocus focus_;
    WindowStacking stacking_;
    WindowInteraction interaction_;
    WindowControls controls_;

    bool initialized_ = false;
    WindowRegistry::ListenerId registry_listener_id_ = 0;

    void on_window_event(const WindowEvent& event);
    void setup_initial_window_placement(const std::shared_ptr<Window>& window);
};

} // namespace ldde::window

