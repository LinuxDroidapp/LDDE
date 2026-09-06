#pragma once

#include <vector>
#include <climits>
#include "ldde/application/application_id.hpp"
#include "ldde/window/types.hpp"

namespace ldde::switcher {

class SwitcherMru {
public:
    SwitcherMru() = default;
    ~SwitcherMru() = default;

    void record_focus(window::WindowId wid, const application::ApplicationId& app_id);
    void record_window_destroyed(window::WindowId wid);
    void record_app_removed(const application::ApplicationId& app_id);

    [[nodiscard]] const std::vector<window::WindowId>& window_order() const noexcept { return mru_windows_; }
    [[nodiscard]] const std::vector<application::ApplicationId>& app_order() const noexcept { return mru_apps_; }

    [[nodiscard]] int get_window_rank(window::WindowId wid) const noexcept;
    [[nodiscard]] int get_app_rank(const application::ApplicationId& app_id) const noexcept;

    void clear() noexcept;

private:
    std::vector<window::WindowId> mru_windows_;
    std::vector<application::ApplicationId> mru_apps_;
};

} // namespace ldde::switcher
