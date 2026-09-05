#pragma once

#include <memory>
#include <optional>
#include "ldde/core/error.hpp"
#include "ldde/window/types.hpp"
#include "ldde/window/window.hpp"
#include "ldde/window/window_management_backend.hpp"
#include "ldde/window/window_placement.hpp"
#include "ldde/display/display_info.hpp"

namespace ldde::window {

using core::Status;

class WindowStateController {
public:
    WindowStateController(WindowManagementBackend& backend, const WindowPlacement& placement);

    Status maximize(const std::shared_ptr<Window>& window, const display::DisplayInfo& display);
    Status restore(const std::shared_ptr<Window>& window, const display::DisplayInfo& display);
    Status minimize(const std::shared_ptr<Window>& window);
    Status restore_minimized(const std::shared_ptr<Window>& window);
    Status fullscreen(const std::shared_ptr<Window>& window, const display::DisplayInfo& display);
    Status restore_fullscreen(const std::shared_ptr<Window>& window, const display::DisplayInfo& display);

    Status toggle_maximize(const std::shared_ptr<Window>& window, const display::DisplayInfo& display);
    Status toggle_fullscreen(const std::shared_ptr<Window>& window, const display::DisplayInfo& display);

    void adapt_to_display_change(const std::shared_ptr<Window>& window, const display::DisplayInfo& display);

private:
    WindowManagementBackend& backend_;
    const WindowPlacement& placement_;
};

} // namespace ldde::window
