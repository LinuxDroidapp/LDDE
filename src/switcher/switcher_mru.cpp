#include "ldde/switcher/switcher_mru.hpp"
#include <algorithm>

namespace ldde::switcher {

void SwitcherMru::record_focus(window::WindowId wid, const application::ApplicationId& app_id) {
    if (wid != window::kInvalidWindowId) {
        auto it = std::find(mru_windows_.begin(), mru_windows_.end(), wid);
        if (it != mru_windows_.end()) {
            mru_windows_.erase(it);
        }
        mru_windows_.insert(mru_windows_.begin(), wid);
    }

    if (!app_id.empty()) {
        auto it = std::find(mru_apps_.begin(), mru_apps_.end(), app_id);
        if (it != mru_apps_.end()) {
            mru_apps_.erase(it);
        }
        mru_apps_.insert(mru_apps_.begin(), app_id);
    }
}

void SwitcherMru::record_window_destroyed(window::WindowId wid) {
    auto it = std::find(mru_windows_.begin(), mru_windows_.end(), wid);
    if (it != mru_windows_.end()) {
        mru_windows_.erase(it);
    }
}

void SwitcherMru::record_app_removed(const application::ApplicationId& app_id) {
    auto it = std::find(mru_apps_.begin(), mru_apps_.end(), app_id);
    if (it != mru_apps_.end()) {
        mru_apps_.erase(it);
    }
}

int SwitcherMru::get_window_rank(window::WindowId wid) const noexcept {
    for (size_t i = 0; i < mru_windows_.size(); ++i) {
        if (mru_windows_[i] == wid) {
            return static_cast<int>(i);
        }
    }
    return INT_MAX;
}

int SwitcherMru::get_app_rank(const application::ApplicationId& app_id) const noexcept {
    for (size_t i = 0; i < mru_apps_.size(); ++i) {
        if (mru_apps_[i] == app_id) {
            return static_cast<int>(i);
        }
    }
    return INT_MAX;
}

void SwitcherMru::clear() noexcept {
    mru_windows_.clear();
    mru_apps_.clear();
}

} // namespace ldde::switcher
