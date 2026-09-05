#pragma once

#include <optional>
#include <memory>
#include "ldde/core/error.hpp"
#include "ldde/window/types.hpp"
#include "ldde/window/window_registry.hpp"
#include "ldde/window/window_stacking.hpp"
#include "ldde/window/window_management_backend.hpp"

namespace ldde::window {

using core::Status;

class WindowFocus {
public:
    WindowFocus(WindowRegistry& registry, WindowStacking& stacking, WindowManagementBackend& backend);

    Status activate(WindowId id);
    Status deactivate();

    [[nodiscard]] std::optional<WindowId> active_window_id() const noexcept { return active_id_; }
    [[nodiscard]] bool is_active(WindowId id) const noexcept { return active_id_.has_value() && active_id_.value() == id; }

    void handle_window_removed_or_hidden(WindowId id);

private:
    WindowRegistry& registry_;
    WindowStacking& stacking_;
    WindowManagementBackend& backend_;
    std::optional<WindowId> active_id_;
};

} // namespace ldde::window
