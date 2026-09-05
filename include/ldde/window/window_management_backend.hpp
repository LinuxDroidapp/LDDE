#pragma once

#include <cstdint>
#include <vector>
#include "ldde/core/types.hpp"
#include "ldde/core/error.hpp"
#include "ldde/window/types.hpp"

namespace ldde::window {

using core::Status;

class WindowManagementBackend {
public:
    virtual ~WindowManagementBackend() = default;

    virtual Status activate(WindowId id) = 0;
    virtual Status deactivate(WindowId id) = 0;
    virtual Status close(WindowId id) = 0;
    virtual Status set_geometry(WindowId id, const core::Rect& geom) = 0;
    virtual Status set_maximized(WindowId id, bool maximized, const core::Size& target_size) = 0;
    virtual Status set_fullscreen(WindowId id, bool fullscreen, const core::Size& target_size) = 0;
    virtual Status set_minimized(WindowId id, bool minimized) = 0;
    virtual Status start_move(WindowId id, uint32_t serial) = 0;
    virtual Status start_resize(WindowId id, ResizeEdge edge, uint32_t serial) = 0;
};

class WindowTracker;
class WindowRegistry;

class DefaultWindowManagementBackend : public WindowManagementBackend {
public:
    DefaultWindowManagementBackend(WindowTracker& tracker, WindowRegistry& registry);
    ~DefaultWindowManagementBackend() override = default;

    Status activate(WindowId id) override;
    Status deactivate(WindowId id) override;
    Status close(WindowId id) override;
    Status set_geometry(WindowId id, const core::Rect& geom) override;
    Status set_maximized(WindowId id, bool maximized, const core::Size& target_size) override;
    Status set_fullscreen(WindowId id, bool fullscreen, const core::Size& target_size) override;
    Status set_minimized(WindowId id, bool minimized) override;
    Status start_move(WindowId id, uint32_t serial) override;
    Status start_resize(WindowId id, ResizeEdge edge, uint32_t serial) override;

private:
    WindowTracker& tracker_;
    WindowRegistry& registry_;
};

} // namespace ldde::window
