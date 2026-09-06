#include "ldde/dock/dock_model.hpp"
#include "ldde/core/logging.hpp"
#include <sstream>
#include <algorithm>
#include <unordered_set>

namespace ldde::dock {

DockModel::DockModel(application::ApplicationCatalog& catalog,
                     window::WindowRegistry& registry,
                     window::WindowManager& window_manager)
    : catalog_(catalog),
      registry_(registry),
      window_manager_(window_manager) {
    initialize_listeners();
}

DockModel::~DockModel() {
    if (window_listener_id_ != 0) {
        registry_.remove_listener(window_listener_id_);
        window_listener_id_ = 0;
    }
}

void DockModel::initialize_listeners() {
    if (window_listener_id_ != 0) return;

    window_listener_id_ = registry_.add_listener([this](const window::WindowEvent&) {
        rebuild_items();
    });

    catalog_.on_catalog_refreshed([this](const application::CatalogDiff&) {
        rebuild_items();
    });
    catalog_.on_application_added([this](const application::ApplicationMetadata&) {
        rebuild_items();
    });
    catalog_.on_application_removed([this](const application::ApplicationMetadata&) {
        rebuild_items();
    });
    catalog_.on_application_changed([this](const application::ApplicationMetadata&) {
        rebuild_items();
    });

    rebuild_items();
}

void DockModel::load_pinned_from_string(std::string_view pinned_str) {
    pinned_ids_.clear();
    std::string current;
    for (char c : pinned_str) {
        if (c == ',' || c == ';' || c == ' ' || c == '\t' || c == '\n') {
            if (!current.empty()) {
                application::ApplicationId id(current);
                if (id.is_valid() && !is_pinned(id)) {
                    pinned_ids_.push_back(id);
                }
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        application::ApplicationId id(current);
        if (id.is_valid() && !is_pinned(id)) {
            pinned_ids_.push_back(id);
        }
    }

    rebuild_items();
}

std::string DockModel::serialize_pinned() const {
    std::string res;
    for (size_t i = 0; i < pinned_ids_.size(); ++i) {
        if (i > 0) res += ",";
        res += pinned_ids_[i].value();
    }
    return res;
}

bool DockModel::pin(const application::ApplicationId& id) {
    if (!id.is_valid() || is_pinned(id)) {
        return false;
    }
    pinned_ids_.push_back(id);
    rebuild_items();
    return true;
}

bool DockModel::unpin(const application::ApplicationId& id) {
    auto it = std::find(pinned_ids_.begin(), pinned_ids_.end(), id);
    if (it == pinned_ids_.end()) {
        return false;
    }
    pinned_ids_.erase(it);
    rebuild_items();
    return true;
}

bool DockModel::is_pinned(const application::ApplicationId& id) const noexcept {
    return std::find(pinned_ids_.begin(), pinned_ids_.end(), id) != pinned_ids_.end();
}

bool DockModel::window_matches_app(const window::Window& win,
                                   const application::ApplicationId& id,
                                   const application::ApplicationMetadata* meta) const noexcept {
    const std::string& win_app = win.app_id();
    if (win_app.empty()) {
        return false;
    }

    // 1. Direct equality with desktop file ID
    if (win_app == id.value()) return true;

    // 2. Direct equality with basename without .desktop
    if (win_app == id.basename_without_extension()) return true;

    // 3. Reverse check (win_app has .desktop or id doesn't)
    if (id.value() == win_app + ".desktop") return true;

    if (meta) {
        // 4. Check startup_wm_class
        if (!meta->startup_wm_class().empty() && win_app == meta->startup_wm_class()) {
            return true;
        }
        // 5. Check executable name
        if (!meta->executable().empty() && win_app == meta->executable()) {
            return true;
        }
    }

    return false;
}

void DockModel::rebuild_items() {
    items_.clear();
    std::unordered_set<window::WindowId> matched_window_ids;
    auto active_win_id = window_manager_.active_window_id();
    if (active_win_id.has_value()) {
        auto active_win = registry_.lookup(*active_win_id);
        if (!active_win || active_win->lifecycle_state() == window::WindowLifecycleState::Destroyed) {
            active_win_id = std::nullopt;
        }
    }
    auto all_windows = registry_.windows();

    // 1. Add all pinned applications in pinned order
    for (const auto& pid : pinned_ids_) {
        const auto* meta = catalog_.find(pid);
        DockItem item(pid, {}, {}, true);

        if (meta) {
            item.set_name(meta->name());
            item.set_icon_ref(meta->icon());
            item.set_executable(meta->executable());
            item.set_available(true);
        } else {
            item.set_name(pid.basename_without_extension());
            item.set_available(false);
        }

        // Match running windows
        bool has_active = false;
        bool all_minimized = true;

        for (const auto& win : all_windows) {
            if (!win || win->lifecycle_state() == window::WindowLifecycleState::Destroyed) continue;
            if (window_matches_app(*win, pid, meta)) {
                item.add_window(win->id());
                matched_window_ids.insert(win->id());

                if (active_win_id.has_value() && win->id() == *active_win_id) {
                    has_active = true;
                }
                if (win->state() != window::WindowState::Minimized) {
                    all_minimized = false;
                }
            }
        }

        if (item.window_count() > 0) {
            item.set_running(true);
            item.set_active(has_active);
            item.set_minimized(all_minimized);
        } else {
            item.set_running(false);
            item.set_active(false);
            item.set_minimized(false);
        }

        items_.push_back(std::move(item));
    }

    // 2. Add unpinned applications that have running windows
    for (const auto& win : all_windows) {
        if (!win || win->lifecycle_state() == window::WindowLifecycleState::Destroyed || matched_window_ids.count(win->id()) > 0) {
            continue;
        }

        const std::string& win_app = win->app_id();
        // Check if an unpinned item for this app already exists in items_
        bool found_existing = false;
        for (auto& existing_item : items_) {
            if (!existing_item.is_pinned() && existing_item.id().value() == win_app) {
                existing_item.add_window(win->id());
                matched_window_ids.insert(win->id());
                if (active_win_id.has_value() && win->id() == *active_win_id) {
                    existing_item.set_active(true);
                }
                if (win->state() != window::WindowState::Minimized) {
                    existing_item.set_minimized(false);
                }
                found_existing = true;
                break;
            }
        }

        if (found_existing) continue;

        // Try to find application in catalog
        application::ApplicationId app_id(win_app.empty() ? "unknown" : win_app);
        const auto* meta = catalog_.find(app_id);
        if (!meta) {
            meta = catalog_.find(application::ApplicationId(win_app + ".desktop"));
        }

        DockItem new_unpinned_item(meta ? meta->id() : app_id, {}, {}, false);
        if (meta) {
            new_unpinned_item.set_name(meta->name());
            new_unpinned_item.set_icon_ref(meta->icon());
            new_unpinned_item.set_executable(meta->executable());
        } else {
            std::string title = win->title().empty() ? win_app : win->title();
            new_unpinned_item.set_name(title);
        }

        new_unpinned_item.add_window(win->id());
        matched_window_ids.insert(win->id());
        new_unpinned_item.set_running(true);
        new_unpinned_item.set_active(active_win_id.has_value() && win->id() == *active_win_id);
        new_unpinned_item.set_minimized(win->state() == window::WindowState::Minimized);

        items_.push_back(std::move(new_unpinned_item));
    }

    notify_changed();
}

const DockItem* DockModel::item_at(size_t index) const noexcept {
    if (index >= items_.size()) return nullptr;
    return &items_[index];
}

DockItem* DockModel::item_at(size_t index) noexcept {
    if (index >= items_.size()) return nullptr;
    return &items_[index];
}

const DockItem* DockModel::find_by_id(const application::ApplicationId& id) const noexcept {
    for (const auto& it : items_) {
        if (it.id() == id) return &it;
    }
    return nullptr;
}

const DockItem* DockModel::find_by_window_id(window::WindowId wid) const noexcept {
    for (const auto& it : items_) {
        for (auto id : it.window_ids()) {
            if (id == wid) return &it;
        }
    }
    return nullptr;
}

void DockModel::notify_changed() {
    if (on_model_changed_) {
        on_model_changed_();
    }
}

} // namespace ldde::dock
