#include "ldde/switcher/switcher_model.hpp"
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

namespace ldde::switcher {

SwitcherModel::SwitcherModel(application::ApplicationCatalog& catalog,
                             window::WindowRegistry& registry,
                             window::WindowManager& window_manager)
    : catalog_(catalog),
      registry_(registry),
      window_manager_(window_manager) {
    initialize_listeners();
    rebuild_items();
}

SwitcherModel::~SwitcherModel() {
    if (window_listener_id_ != 0) {
        registry_.remove_listener(window_listener_id_);
        window_listener_id_ = 0;
    }
}

void SwitcherModel::initialize_listeners() {
    if (window_listener_id_ != 0) return;

    window_listener_id_ = registry_.add_listener([this](const window::WindowEvent& ev) {
        if (ev.type == window::WindowEventType::FocusChanged) {
            auto active_wid = registry_.active_window_id();
            if (active_wid.has_value() && *active_wid == ev.window_id) {
                auto win = registry_.lookup(ev.window_id);
                if (win) {
                    application::ApplicationId resolved_id;
                    resolve_metadata(*win, resolved_id);
                    mru_.record_focus(ev.window_id, resolved_id);
                }
            }
            rebuild_items();
        } else if (ev.type == window::WindowEventType::Closed ||
                   ev.type == window::WindowEventType::Destroyed) {
            mru_.record_window_destroyed(ev.window_id);
            rebuild_items();
        } else if (ev.type == window::WindowEventType::Created ||
                   ev.type == window::WindowEventType::TitleChanged ||
                   ev.type == window::WindowEventType::AppIdChanged ||
                   ev.type == window::WindowEventType::StateChanged ||
                   ev.type == window::WindowEventType::VisibilityChanged) {
            rebuild_items();
        }
    });
}

void SwitcherModel::set_presentation_mode(SwitcherPresentationMode mode) {
    if (mode_ != mode) {
        mode_ = mode;
        rebuild_items();
    }
}

bool SwitcherModel::is_switchable(const window::Window& win) const noexcept {
    if (win.lifecycle_state() == window::WindowLifecycleState::Destroyed ||
        win.lifecycle_state() == window::WindowLifecycleState::Closing ||
        win.lifecycle_state() == window::WindowLifecycleState::Failed) {
        return false;
    }

    if (mode_ == SwitcherPresentationMode::Application && win.parent_id().has_value()) {
        // Transient dialogs are represented under their parent application in Application mode
        return false;
    }

    return true;
}

const application::ApplicationMetadata* SwitcherModel::resolve_metadata(
    const window::Window& win,
    application::ApplicationId& resolved_id) const noexcept {
    const std::string& win_app = win.app_id();

    if (!win_app.empty()) {
        // 1. Exact match
        application::ApplicationId id(win_app);
        if (const auto* meta = catalog_.find(id)) {
            resolved_id = id;
            return meta;
        }

        // 2. Append .desktop
        application::ApplicationId id_desktop(win_app + ".desktop");
        if (const auto* meta = catalog_.find(id_desktop)) {
            resolved_id = id_desktop;
            return meta;
        }

        // 3. Search all apps in catalog for startup_wm_class or executable
        for (const auto& app : catalog_.all()) {
            if (!app.startup_wm_class().empty() && app.startup_wm_class() == win_app) {
                resolved_id = app.id();
                return &app;
            }
            if (!app.executable().empty() && app.executable() == win_app) {
                resolved_id = app.id();
                return &app;
            }
        }

        resolved_id = application::ApplicationId(win_app);
        return nullptr;
    }

    resolved_id = application::ApplicationId("window_" + std::to_string(win.id()));
    return nullptr;
}

void SwitcherModel::rebuild_items(std::optional<window::WindowId> /*target_selected_wid*/) {
    items_.clear();

    auto active_win_id = window_manager_.active_window_id();
    if (!active_win_id.has_value()) {
        active_win_id = registry_.active_window_id();
    }
    auto all_windows = registry_.windows();

    if (mode_ == SwitcherPresentationMode::Application) {
        struct AppGroup {
            application::ApplicationId app_id;
            const application::ApplicationMetadata* meta = nullptr;
            std::vector<window::WindowId> window_ids;
            window::WindowId primary_wid = window::kInvalidWindowId;
            std::string sample_title;
            bool is_current = false;
            bool all_minimized = true;
        };

        std::vector<AppGroup> groups;
        auto find_group = [&](const application::ApplicationId& id) -> AppGroup* {
            for (auto& g : groups) {
                if (g.app_id == id) return &g;
            }
            return nullptr;
        };

        for (const auto& win : all_windows) {
            if (!win || !is_switchable(*win)) continue;

            application::ApplicationId app_id;
            const auto* meta = resolve_metadata(*win, app_id);

            auto* group = find_group(app_id);
            if (!group) {
                groups.push_back({app_id, meta, {}, window::kInvalidWindowId, win->title(), false, true});
                group = &groups.back();
            }

            group->window_ids.push_back(win->id());
            if (active_win_id.has_value() && win->id() == *active_win_id) {
                group->is_current = true;
            }
            if (win->state() != window::WindowState::Minimized) {
                group->all_minimized = false;
            }

            // Determine primary window for the group by MRU rank
            if (group->primary_wid == window::kInvalidWindowId) {
                group->primary_wid = win->id();
                group->sample_title = win->title();
            } else {
                int current_rank = mru_.get_window_rank(group->primary_wid);
                int win_rank = mru_.get_window_rank(win->id());
                if (win_rank < current_rank) {
                    group->primary_wid = win->id();
                    group->sample_title = win->title();
                }
            }
        }

        // Also check if any transient windows belong to parents in these groups
        for (const auto& win : all_windows) {
            if (!win || win->lifecycle_state() == window::WindowLifecycleState::Destroyed) continue;
            if (win->parent_id().has_value()) {
                window::WindowId pid = *win->parent_id();
                for (auto& g : groups) {
                    if (std::find(g.window_ids.begin(), g.window_ids.end(), pid) != g.window_ids.end()) {
                        g.window_ids.push_back(win->id());
                        break;
                    }
                }
            }
        }

        // Convert groups to SwitcherItems
        for (const auto& g : groups) {
            std::string name;
            std::string icon;
            if (g.meta) {
                name = g.meta->name();
                icon = g.meta->icon().raw();
            } else {
                name = g.app_id.basename_without_extension();
                icon = g.app_id.basename_without_extension();
            }
            if (name.empty()) {
                name = g.sample_title.empty() ? "Application" : g.sample_title;
            }

            SwitcherItem item(g.app_id, g.primary_wid, name, icon, g.sample_title, g.all_minimized);
            // Replace window_ids with all grouped window_ids
            for (auto wid : g.window_ids) {
                item.add_window(wid);
            }
            item.set_is_current(g.is_current);
            items_.push_back(std::move(item));
        }

        // Sort items by MRU rank of primary window
        std::sort(items_.begin(), items_.end(), [this](const SwitcherItem& a, const SwitcherItem& b) {
            int rank_a = mru_.get_window_rank(a.primary_window_id());
            int rank_b = mru_.get_window_rank(b.primary_window_id());
            if (rank_a != rank_b) {
                return rank_a < rank_b;
            }
            return a.primary_window_id() < b.primary_window_id();
        });

    } else { // SwitcherPresentationMode::Window
        for (const auto& win : all_windows) {
            if (!win || !is_switchable(*win)) continue;

            application::ApplicationId app_id;
            const auto* meta = resolve_metadata(*win, app_id);

            std::string name = win->title().empty() ? (meta ? meta->name() : app_id.value()) : win->title();
            std::string icon = meta ? meta->icon().raw() : app_id.basename_without_extension();

            SwitcherItem item(app_id, win->id(), name, icon, win->title(),
                             win->state() == window::WindowState::Minimized);
            item.set_is_current(active_win_id.has_value() && win->id() == *active_win_id);
            items_.push_back(std::move(item));
        }

        // Sort items by window MRU rank
        std::sort(items_.begin(), items_.end(), [this](const SwitcherItem& a, const SwitcherItem& b) {
            int rank_a = mru_.get_window_rank(a.primary_window_id());
            int rank_b = mru_.get_window_rank(b.primary_window_id());
            if (rank_a != rank_b) {
                return rank_a < rank_b;
            }
            return a.primary_window_id() < b.primary_window_id();
        });
    }

    notify_changed();
}

const SwitcherItem* SwitcherModel::item_at(size_t index) const noexcept {
    if (index >= items_.size()) return nullptr;
    return &items_[index];
}

SwitcherItem* SwitcherModel::item_at(size_t index) noexcept {
    if (index >= items_.size()) return nullptr;
    return &items_[index];
}

const SwitcherItem* SwitcherModel::find_by_window_id(window::WindowId wid) const noexcept {
    for (const auto& item : items_) {
        if (item.has_window(wid)) return &item;
    }
    return nullptr;
}

const SwitcherItem* SwitcherModel::find_by_app_id(const application::ApplicationId& id) const noexcept {
    for (const auto& item : items_) {
        if (item.app_id() == id) return &item;
    }
    return nullptr;
}

std::optional<size_t> SwitcherModel::find_index_by_window_id(window::WindowId wid) const noexcept {
    for (size_t i = 0; i < items_.size(); ++i) {
        if (items_[i].has_window(wid)) return i;
    }
    return std::nullopt;
}

void SwitcherModel::notify_changed() {
    for (const auto& cb : model_changed_callbacks_) {
        if (cb) {
            cb();
        }
    }
}

} // namespace ldde::switcher
