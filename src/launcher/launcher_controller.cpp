#include "ldde/launcher/launcher_controller.hpp"
#include "ldde/core/logging.hpp"
#include <cmath>
#include <algorithm>

namespace ldde::launcher {

LauncherController::LauncherController(
    std::shared_ptr<ApplicationLauncher> launcher_backend,
    std::string theme_name)
    : model_(std::move(theme_name)),
      launcher_backend_(std::move(launcher_backend)) {
    if (!launcher_backend_) {
        launcher_backend_ = std::make_shared<LinuxSessionApplicationLauncher>();
    }

    model_.on_model_changed([this]() {
        request_render();
    });

    state_machine_.on_state_changed([this](LauncherState /*old_state*/, LauncherState new_state) {
        if (new_state == LauncherState::Closed) {
            if (on_closed_) {
                on_closed_();
            }
        }
        request_render();
    });
}

core::Status LauncherController::open() {
    core::Status s = state_machine_.request_open();
    if (s.is_error()) return s;
    s = state_machine_.finish_open();
    if (s.is_error()) return s;

    scroll_y_ = 0;
    focus_area_ = LauncherFocusArea::SearchBar;
    last_error_message_.clear();

    if (model_.item_count() > 0 && !model_.selected_index()) {
        model_.select_index(0);
    }

    request_render();
    return core::Status::ok();
}

core::Status LauncherController::close() {
    if (pressed_item_index_) {
        if (auto* it = const_cast<LauncherItem*>(model_.item_at(*pressed_item_index_))) {
            it->set_pressed(false);
        }
        pressed_item_index_.reset();
    }
    touch_active_ = false;
    is_scrolling_ = false;

    core::Status s = state_machine_.request_close();
    if (s.is_error()) return s;
    s = state_machine_.finish_close();
    if (s.is_error()) return s;

    request_render();
    return core::Status::ok();
}

core::Status LauncherController::toggle() {
    if (is_open()) {
        return close();
    } else {
        return open();
    }
}

void LauncherController::update_layout(const display::DisplayPolicy& policy) {
    layout_.update(policy,
                   static_cast<int>(model_.item_count()),
                   static_cast<int>(model_.categories().size()));
    scroll_y_ = std::clamp(scroll_y_, 0, layout_.max_scroll_y());
    request_render();
}

void LauncherController::request_render() {
    if (on_request_render_) {
        on_request_render_();
    }
}

bool LauncherController::handle_key_down(uint32_t key_symbol, uint32_t unicode_codepoint) {
    if (!is_open()) return false;

    // Escape
    if (key_symbol == 0xff1b || key_symbol == 27 || key_symbol == 1) {
        if (!model_.filter().search_query.empty()) {
            model_.clear_search();
            state_machine_.stop_searching();
            scroll_y_ = 0;
        } else {
            close();
        }
        return true;
    }

    // Enter / Return
    if (key_symbol == 0xff0d || key_symbol == 0xff8d || key_symbol == 13 || key_symbol == 10 || key_symbol == 28) {
        launch_selected();
        return true;
    }

    // Tab (cycle focus between Search, Categories, Grid)
    if (key_symbol == 0xff09 || key_symbol == 9 || key_symbol == 15) {
        if (focus_area_ == LauncherFocusArea::SearchBar) {
            focus_area_ = LauncherFocusArea::Categories;
        } else if (focus_area_ == LauncherFocusArea::Categories) {
            focus_area_ = LauncherFocusArea::Grid;
            if (!model_.selected_index() && model_.item_count() > 0) {
                model_.select_index(0);
            }
        } else {
            focus_area_ = LauncherFocusArea::SearchBar;
        }
        request_render();
        return true;
    }

    // Backspace
    if (key_symbol == 0xff08 || key_symbol == 8 || key_symbol == 14) {
        std::string q = model_.filter().search_query;
        if (!q.empty()) {
            q.pop_back();
            model_.set_search_query(q);
            scroll_y_ = 0;
            if (q.empty()) {
                state_machine_.stop_searching();
            }
            return true;
        }
        return true;
    }

    // Arrow Left
    if (key_symbol == 0xff51 || key_symbol == 105) {
        if (focus_area_ == LauncherFocusArea::Categories) {
            const auto& cats = model_.categories();
            for (size_t i = 0; i < cats.size(); ++i) {
                if (cats[i].id == model_.filter().category) {
                    size_t prev = (i == 0) ? (cats.size() - 1) : (i - 1);
                    model_.set_category(cats[prev].id);
                    scroll_y_ = 0;
                    break;
                }
            }
        } else {
            model_.select_previous();
            if (model_.selected_index()) {
                core::Rect ir = layout_.item_rect(*model_.selected_index(), scroll_y_);
                if (ir.y < layout_.grid_rect().y) {
                    scroll_y_ = std::max(0, layout_.item_rect(*model_.selected_index(), 0).y - layout_.grid_rect().y);
                }
            }
        }
        return true;
    }

    // Arrow Right
    if (key_symbol == 0xff53 || key_symbol == 106) {
        if (focus_area_ == LauncherFocusArea::Categories) {
            const auto& cats = model_.categories();
            for (size_t i = 0; i < cats.size(); ++i) {
                if (cats[i].id == model_.filter().category) {
                    size_t next = (i + 1) % cats.size();
                    model_.set_category(cats[next].id);
                    scroll_y_ = 0;
                    break;
                }
            }
        } else {
            model_.select_next();
            if (model_.selected_index()) {
                core::Rect ir = layout_.item_rect(*model_.selected_index(), scroll_y_);
                if (ir.y + ir.height > layout_.grid_rect().y + layout_.grid_rect().height) {
                    scroll_y_ = std::min(layout_.max_scroll_y(),
                                         layout_.item_rect(*model_.selected_index(), 0).y + ir.height - (layout_.grid_rect().y + layout_.grid_rect().height));
                }
            }
        }
        return true;
    }

    // Arrow Up
    if (key_symbol == 0xff52 || key_symbol == 103) {
        model_.select_up(layout_.columns());
        if (model_.selected_index()) {
            core::Rect ir = layout_.item_rect(*model_.selected_index(), scroll_y_);
            if (ir.y < layout_.grid_rect().y) {
                scroll_y_ = std::max(0, layout_.item_rect(*model_.selected_index(), 0).y - layout_.grid_rect().y);
            }
        }
        return true;
    }

    // Arrow Down
    if (key_symbol == 0xff54 || key_symbol == 108) {
        model_.select_down(layout_.columns());
        if (model_.selected_index()) {
            core::Rect ir = layout_.item_rect(*model_.selected_index(), scroll_y_);
            if (ir.y + ir.height > layout_.grid_rect().y + layout_.grid_rect().height) {
                scroll_y_ = std::min(layout_.max_scroll_y(),
                                     layout_.item_rect(*model_.selected_index(), 0).y + ir.height - (layout_.grid_rect().y + layout_.grid_rect().height));
            }
        }
        return true;
    }

    // Home / End / PageUp / PageDown
    if (key_symbol == 0xff50) {
        model_.select_first();
        scroll_y_ = 0;
        return true;
    }
    if (key_symbol == 0xff57) {
        model_.select_last();
        scroll_y_ = layout_.max_scroll_y();
        return true;
    }
    if (key_symbol == 0xff55) {
        scroll_y_ = std::max(0, scroll_y_ - layout_.grid_rect().height);
        request_render();
        return true;
    }
    if (key_symbol == 0xff56) {
        scroll_y_ = std::min(layout_.max_scroll_y(), scroll_y_ + layout_.grid_rect().height);
        request_render();
        return true;
    }

    // Printable character input
    char c = 0;
    if (unicode_codepoint >= 32 && unicode_codepoint <= 126) {
        c = static_cast<char>(unicode_codepoint);
    } else if (key_symbol >= 32 && key_symbol <= 126) {
        c = static_cast<char>(key_symbol);
    }

    if (c != 0) {
        std::string q = model_.filter().search_query;
        q.push_back(c);
        model_.set_search_query(q);
        scroll_y_ = 0;
        focus_area_ = LauncherFocusArea::SearchBar;
        state_machine_.start_searching();
        return true;
    }

    return false;
}

bool LauncherController::handle_touch_down(int32_t x, int32_t y) {
    if (!is_open()) return false;

    touch_active_ = true;
    touch_down_point_ = {x, y};
    touch_down_scroll_y_ = scroll_y_;
    is_scrolling_ = false;

    auto hit = layout_.hit_test({x, y}, scroll_y_, model_.item_count(), model_.categories().size());
    switch (hit.type) {
        case LauncherHitAreaType::DismissScrim:
            close();
            return true;

        case LauncherHitAreaType::ClearSearchButton:
            model_.clear_search();
            state_machine_.stop_searching();
            scroll_y_ = 0;
            request_render();
            return true;

        case LauncherHitAreaType::SearchBar:
            focus_area_ = LauncherFocusArea::SearchBar;
            state_machine_.start_searching();
            request_render();
            return true;

        case LauncherHitAreaType::CategoryChip:
            if (hit.index < model_.categories().size()) {
                model_.set_category(model_.categories()[hit.index].id);
                scroll_y_ = 0;
                focus_area_ = LauncherFocusArea::Categories;
                request_render();
            }
            return true;

        case LauncherHitAreaType::GridItem:
            if (hit.index < model_.item_count()) {
                pressed_item_index_ = hit.index;
                if (auto* it = const_cast<LauncherItem*>(model_.item_at(hit.index))) {
                    it->set_pressed(true);
                }
                focus_area_ = LauncherFocusArea::Grid;
                request_render();
            }
            return true;

        case LauncherHitAreaType::None:
            return true;
    }

    return true;
}

bool LauncherController::handle_touch_motion(int32_t x, int32_t y) {
    if (!touch_active_) return false;

    int dx = x - touch_down_point_.x;
    int dy = y - touch_down_point_.y;
    double dist = std::hypot(dx, dy);

    if (dist > 8.0) {
        is_scrolling_ = true;
        if (pressed_item_index_) {
            if (auto* it = const_cast<LauncherItem*>(model_.item_at(*pressed_item_index_))) {
                it->set_pressed(false);
            }
            pressed_item_index_.reset();
        }

        int new_scroll = touch_down_scroll_y_ - dy;
        scroll_y_ = std::clamp(new_scroll, 0, layout_.max_scroll_y());
        request_render();
    }

    return true;
}

bool LauncherController::handle_touch_up(int32_t /*x*/, int32_t /*y*/) {
    if (!touch_active_) return false;

    if (!is_scrolling_ && pressed_item_index_) {
        size_t idx = *pressed_item_index_;
        if (auto* it = const_cast<LauncherItem*>(model_.item_at(idx))) {
            it->set_pressed(false);
        }
        model_.select_index(idx);
        launch_item(idx);
    }

    touch_active_ = false;
    is_scrolling_ = false;
    pressed_item_index_.reset();
    request_render();
    return true;
}

void LauncherController::handle_touch_cancel() {
    if (pressed_item_index_) {
        if (auto* it = const_cast<LauncherItem*>(model_.item_at(*pressed_item_index_))) {
            it->set_pressed(false);
        }
    }
    touch_active_ = false;
    is_scrolling_ = false;
    pressed_item_index_.reset();
    request_render();
}

LaunchResult LauncherController::launch_item(size_t index) {
    const auto* item = model_.item_at(index);
    if (!item) {
        LDDE_LOG_WARN(Launcher, "Cannot launch item: index " << index << " out of bounds");
        return LaunchResult::failure(LaunchStatus::NotFound, "Application item not found");
    }

    core::Status s = state_machine_.request_launch();
    if (s.is_error()) {
        return LaunchResult::failure(LaunchStatus::InvalidMetadata, "Launcher not in valid state to launch");
    }

    if (!launcher_backend_) {
        last_error_message_ = "Launch backend unavailable";
        state_machine_.fail_launch();
        request_render();
        return LaunchResult::failure(LaunchStatus::BackendUnavailable, "Launch backend unavailable");
    }

    LaunchRequest req;
    req.id = item->id();
    req.name = item->name();
    req.executable = item->executable();
    req.arguments = item->exec_args();
    req.terminal = item->terminal();
    req.desktop_entry_path = item->desktop_entry_path();
    req.startup_wm_class = item->startup_wm_class();
    req.startup_notify = item->startup_notify();

    LDDE_LOG_INFO(Launcher, "Executing launch request for application '" << req.name << "' (" << req.id.value() << ")");
    auto result = launcher_backend_->launch(req);

    if (result.is_success()) {
        state_machine_.finish_launch();
        if (on_launched_) {
            on_launched_(item->id());
        }
        close();
    } else {
        last_error_message_ = result.error_message;
        state_machine_.fail_launch();
        LDDE_LOG_WARN(Launcher, "Launch failed for '" << req.name << "': " << result.error_message);
        request_render();
    }

    return result;
}

LaunchResult LauncherController::launch_selected() {
    if (auto idx = model_.selected_index()) {
        return launch_item(*idx);
    }
    if (model_.item_count() > 0) {
        return launch_item(0);
    }
    return LaunchResult::failure(LaunchStatus::NotFound, "No application selected");
}

} // namespace ldde::launcher
