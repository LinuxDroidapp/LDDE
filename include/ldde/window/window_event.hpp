#pragma once

#include <memory>
#include <string>
#include "ldde/window/types.hpp"

namespace ldde::window {

class Window;

struct WindowEvent {
    WindowEventType type = WindowEventType::Created;
    WindowId window_id = kInvalidWindowId;
    std::shared_ptr<Window> window;
    std::string property_name;
};

} // namespace ldde::window
