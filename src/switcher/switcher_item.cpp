#include "ldde/switcher/switcher_item.hpp"

namespace ldde::switcher {

SwitcherItem::SwitcherItem(application::ApplicationId app_id,
                           window::WindowId primary_window_id,
                           std::string display_name,
                           std::string icon_name,
                           std::string window_title,
                           bool is_minimized)
    : app_id_(std::move(app_id)),
      primary_window_id_(primary_window_id),
      display_name_(std::move(display_name)),
      icon_name_(std::move(icon_name)),
      window_title_(std::move(window_title)),
      is_minimized_(is_minimized) {
    if (primary_window_id_ != window::kInvalidWindowId) {
        window_ids_.push_back(primary_window_id_);
    }
}

void SwitcherItem::add_window(window::WindowId wid) {
    if (wid == window::kInvalidWindowId) return;
    if (!has_window(wid)) {
        window_ids_.push_back(wid);
    }
    if (primary_window_id_ == window::kInvalidWindowId) {
        primary_window_id_ = wid;
    }
}

bool SwitcherItem::remove_window(window::WindowId wid) {
    auto it = std::find(window_ids_.begin(), window_ids_.end(), wid);
    if (it != window_ids_.end()) {
        window_ids_.erase(it);
        if (primary_window_id_ == wid) {
            primary_window_id_ = window_ids_.empty() ? window::kInvalidWindowId : window_ids_.front();
        }
        return true;
    }
    return false;
}

bool SwitcherItem::has_window(window::WindowId wid) const noexcept {
    return std::find(window_ids_.begin(), window_ids_.end(), wid) != window_ids_.end();
}

} // namespace ldde::switcher
