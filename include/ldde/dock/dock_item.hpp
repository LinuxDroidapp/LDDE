#pragma once

#include <string>
#include <string_view>
#include <vector>
#include "ldde/core/types.hpp"
#include "ldde/application/application_id.hpp"
#include "ldde/application/application_icon.hpp"
#include "ldde/window/types.hpp"

namespace ldde::dock {

class DockItem {
public:
    DockItem() = default;
    explicit DockItem(application::ApplicationId id,
                      std::string name = {},
                      application::ApplicationIconReference icon_ref = {},
                      bool is_pinned = false);

    [[nodiscard]] const application::ApplicationId& id() const noexcept { return id_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] const application::ApplicationIconReference& icon_ref() const noexcept { return icon_ref_; }
    [[nodiscard]] const std::string& executable() const noexcept { return executable_; }
    [[nodiscard]] bool is_pinned() const noexcept { return is_pinned_; }
    [[nodiscard]] bool is_running() const noexcept { return is_running_; }
    [[nodiscard]] bool is_active() const noexcept { return is_active_; }
    [[nodiscard]] bool is_minimized() const noexcept { return is_minimized_; }
    [[nodiscard]] bool is_available() const noexcept { return is_available_; }
    [[nodiscard]] size_t window_count() const noexcept { return window_ids_.size(); }
    [[nodiscard]] const std::vector<window::WindowId>& window_ids() const noexcept { return window_ids_; }
    [[nodiscard]] const core::Rect& bounds() const noexcept { return bounds_; }

    void set_name(std::string name) { name_ = std::move(name); }
    void set_icon_ref(application::ApplicationIconReference icon_ref) { icon_ref_ = std::move(icon_ref); }
    void set_executable(std::string exe) { executable_ = std::move(exe); }
    void set_pinned(bool pinned) noexcept { is_pinned_ = pinned; }
    void set_running(bool running) noexcept { is_running_ = running; }
    void set_active(bool active) noexcept { is_active_ = active; }
    void set_minimized(bool minimized) noexcept { is_minimized_ = minimized; }
    void set_available(bool available) noexcept { is_available_ = available; }
    void set_bounds(const core::Rect& rect) noexcept { bounds_ = rect; }

    void add_window(window::WindowId wid);
    void remove_window(window::WindowId wid);
    void clear_windows() noexcept;

private:
    application::ApplicationId id_;
    std::string name_;
    application::ApplicationIconReference icon_ref_;
    std::string executable_;
    bool is_pinned_ = false;
    bool is_running_ = false;
    bool is_active_ = false;
    bool is_minimized_ = false;
    bool is_available_ = true;
    std::vector<window::WindowId> window_ids_;
    core::Rect bounds_{0, 0, 0, 0};
};

} // namespace ldde::dock
