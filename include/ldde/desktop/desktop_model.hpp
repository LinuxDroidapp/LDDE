#pragma once

#include "ldde/window/window_registry.hpp"
#include <functional>
#include <vector>
#include <cstdint>

namespace ldde::desktop {

class DesktopModel {
public:
    using ModelChangedCallback = std::function<void()>;

    explicit DesktopModel(window::WindowRegistry& registry);
    ~DesktopModel();

    DesktopModel(const DesktopModel&) = delete;
    DesktopModel& operator=(const DesktopModel&) = delete;

    [[nodiscard]] size_t active_window_count() const noexcept { return active_window_count_; }
    [[nodiscard]] bool is_empty() const noexcept { return active_window_count_ == 0; }
    [[nodiscard]] bool is_desktop_focused() const noexcept { return is_desktop_focused_; }

    void refresh();
    void on_model_changed(ModelChangedCallback callback);

private:
    window::WindowRegistry& registry_;
    window::WindowRegistry::ListenerId registry_listener_id_ = 0;

    size_t active_window_count_ = 0;
    bool is_desktop_focused_ = true;

    std::vector<ModelChangedCallback> callbacks_;

    void notify_changed();
    void evaluate_state();
};

} // namespace ldde::desktop
