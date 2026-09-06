#include "ldde/dock/dock_item.hpp"
#include <algorithm>

namespace ldde::dock {

DockItem::DockItem(application::ApplicationId id,
                   std::string name,
                   application::ApplicationIconReference icon_ref,
                   bool is_pinned)
    : id_(std::move(id)),
      name_(std::move(name)),
      icon_ref_(std::move(icon_ref)),
      is_pinned_(is_pinned) {
    if (name_.empty()) {
        name_ = id_.basename_without_extension();
    }
}

void DockItem::add_window(window::WindowId wid) {
    if (std::find(window_ids_.begin(), window_ids_.end(), wid) == window_ids_.end()) {
        window_ids_.push_back(wid);
    }
    is_running_ = !window_ids_.empty();
}

void DockItem::remove_window(window::WindowId wid) {
    auto it = std::remove(window_ids_.begin(), window_ids_.end(), wid);
    window_ids_.erase(it, window_ids_.end());
    is_running_ = !window_ids_.empty();
    if (!is_running_) {
        is_active_ = false;
        is_minimized_ = false;
    }
}

void DockItem::clear_windows() noexcept {
    window_ids_.clear();
    is_running_ = false;
    is_active_ = false;
    is_minimized_ = false;
}

} // namespace ldde::dock
