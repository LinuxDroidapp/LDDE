#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include "ldde/core/types.hpp"
#include "ldde/application/application_id.hpp"
#include "ldde/window/types.hpp"

namespace ldde::switcher {

class SwitcherItem {
public:
    SwitcherItem() = default;
    SwitcherItem(application::ApplicationId app_id,
                 window::WindowId primary_window_id,
                 std::string display_name,
                 std::string icon_name,
                 std::string window_title,
                 bool is_minimized = false);

    [[nodiscard]] const application::ApplicationId& app_id() const noexcept { return app_id_; }
    [[nodiscard]] window::WindowId primary_window_id() const noexcept { return primary_window_id_; }
    [[nodiscard]] const std::vector<window::WindowId>& window_ids() const noexcept { return window_ids_; }
    [[nodiscard]] size_t window_count() const noexcept { return window_ids_.size(); }

    [[nodiscard]] const std::string& display_name() const noexcept { return display_name_; }
    [[nodiscard]] const std::string& icon_name() const noexcept { return icon_name_; }
    [[nodiscard]] const std::string& window_title() const noexcept { return window_title_; }

    [[nodiscard]] bool is_current() const noexcept { return is_current_; }
    [[nodiscard]] bool is_selected() const noexcept { return is_selected_; }
    [[nodiscard]] bool is_minimized() const noexcept { return is_minimized_; }
    [[nodiscard]] const core::Rect& geometry() const noexcept { return geometry_; }

    void set_primary_window_id(window::WindowId wid) noexcept { primary_window_id_ = wid; }
    void set_display_name(std::string name) { display_name_ = std::move(name); }
    void set_icon_name(std::string icon) { icon_name_ = std::move(icon); }
    void set_window_title(std::string title) { window_title_ = std::move(title); }
    void set_is_current(bool current) noexcept { is_current_ = current; }
    void set_is_selected(bool selected) noexcept { is_selected_ = selected; }
    void set_is_minimized(bool minimized) noexcept { is_minimized_ = minimized; }
    void set_geometry(const core::Rect& geom) noexcept { geometry_ = geom; }

    void add_window(window::WindowId wid);
    bool remove_window(window::WindowId wid);
    [[nodiscard]] bool has_window(window::WindowId wid) const noexcept;

private:
    application::ApplicationId app_id_;
    window::WindowId primary_window_id_ = window::kInvalidWindowId;
    std::vector<window::WindowId> window_ids_;
    std::string display_name_;
    std::string icon_name_;
    std::string window_title_;
    bool is_current_ = false;
    bool is_selected_ = false;
    bool is_minimized_ = false;
    core::Rect geometry_{0, 0, 0, 0};
};

} // namespace ldde::switcher
