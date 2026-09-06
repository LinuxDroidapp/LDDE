#include "ldde/settings/settings_controller.hpp"
#include "ldde/core/logging.hpp"
#include <cmath>
#include <algorithm>

namespace ldde::settings {

SettingsController::SettingsController(SettingsStore& store,
                                       SettingsNavigation& navigation,
                                       SettingsLayout& layout,
                                       const SettingsSearch& search)
    : store_(store),
      navigation_(navigation),
      layout_(layout),
      search_(search) {}

void SettingsController::request_render() {
    if (on_request_render_) {
        on_request_render_();
    }
}

std::vector<const SettingDefinition*> SettingsController::current_visible_settings() const {
    if (navigation_.is_searching()) {
        return search_.search(navigation_.search_query());
    }
    return store_.schema().settings_in_category(navigation_.active_category());
}

void SettingsController::apply_slider_value(const std::string& key, double fraction) {
    const auto* def = store_.schema().find(key);
    if (!def) return;

    fraction = std::clamp(fraction, 0.0, 1.0);
    double min_v = def->min_value.value_or(0.0);
    double max_v = def->max_value.value_or(100.0);
    double raw = min_v + fraction * (max_v - min_v);

    if (def->step_value > 0.0) {
        raw = std::round(raw / def->step_value) * def->step_value;
    }
    raw = std::clamp(raw, min_v, max_v);

    if (def->type == SettingType::Int) {
        store_.set(key, SettingsValue(static_cast<int64_t>(std::round(raw))));
    } else {
        store_.set(key, SettingsValue(raw));
    }
}

bool SettingsController::handle_touch_down(int32_t x, int32_t y) {
    SettingsHitResult hit = layout_.hit_test(x, y);
    if (hit.type == SettingsHitType::None) {
        return false;
    }

    is_touch_active_ = true;
    is_scrolling_ = false;
    is_slider_dragging_ = false;
    touch_start_x_ = x;
    touch_start_y_ = y;
    initial_scroll_y_ = layout_.scroll_y();

    if (hit.type == SettingsHitType::SettingSlider) {
        is_slider_dragging_ = true;
        active_slider_key_ = hit.setting_key;
        apply_slider_value(hit.setting_key, hit.slider_fraction);
        request_render();
        return true;
    }

    return true;
}

bool SettingsController::handle_touch_motion(int32_t x, int32_t y) {
    if (!is_touch_active_) return false;

    int32_t dy = y - touch_start_y_;
    int32_t dx = x - touch_start_x_;

    if (is_slider_dragging_) {
        // Find row control rect
        for (const auto& row : layout_.setting_rows()) {
            if (row.key == active_slider_key_) {
                double frac = static_cast<double>(x - row.control_rect.x) / static_cast<double>(row.control_rect.width);
                apply_slider_value(active_slider_key_, frac);
                request_render();
                return true;
            }
        }
    }

    if (!is_scrolling_ && std::abs(dy) > 10) {
        is_scrolling_ = true;
    }

    if (is_scrolling_) {
        layout_.set_scroll_y(initial_scroll_y_ - dy);
        request_render();
        return true;
    }

    (void)dx;
    return true;
}

bool SettingsController::handle_touch_up(int32_t x, int32_t y) {
    if (!is_touch_active_) return false;

    is_touch_active_ = false;
    if (is_slider_dragging_) {
        is_slider_dragging_ = false;
        active_slider_key_.clear();
        return true;
    }

    if (is_scrolling_) {
        is_scrolling_ = false;
        return true;
    }

    // Tap activation
    SettingsHitResult hit = layout_.hit_test(x, y);
    switch (hit.type) {
        case SettingsHitType::CloseButton:
            if (on_request_close_) on_request_close_();
            return true;

        case SettingsHitType::MinimizeButton:
            if (on_request_minimize_) on_request_minimize_();
            return true;

        case SettingsHitType::MaximizeButton:
            if (on_request_maximize_) on_request_maximize_();
            return true;

        case SettingsHitType::BackButton:
            static_cast<void>(navigation_.navigate_back());
            request_render();
            return true;

        case SettingsHitType::SearchClear:
            navigation_.clear_search();
            request_render();
            return true;

        case SettingsHitType::CategoryItem:
            if (layout_.is_portrait()) {
                navigation_.drill_down(hit.category);
            } else {
                navigation_.select_category(hit.category);
            }
            request_render();
            return true;

        case SettingsHitType::SettingToggle: {
            auto cur = store_.get(hit.setting_key);
            bool b = cur.has_value() ? cur->as_bool().value_or(false) : false;
            store_.set(hit.setting_key, SettingsValue(!b));
            request_render();
            return true;
        }

        case SettingsHitType::SettingRadioOption: {
            const auto* def = store_.schema().find(hit.setting_key);
            if (def && hit.option_index < def->enum_values.size()) {
                store_.set(hit.setting_key, SettingsValue(def->enum_values[hit.option_index]));
                request_render();
                return true;
            }
            break;
        }

        case SettingsHitType::SettingAction: {
            if (navigation_.active_category() == SettingsCategory::About) {
                store_.reset_all();
                request_render();
                return true;
            }
            break;
        }

        case SettingsHitType::Titlebar: {
            if (on_start_move_) on_start_move_(x, y);
            return true;
        }

        default:
            break;
    }

    return true;
}

void SettingsController::handle_touch_cancel() {
    is_touch_active_ = false;
    is_scrolling_ = false;
    is_slider_dragging_ = false;
    active_slider_key_.clear();
}

bool SettingsController::handle_pointer_motion(int32_t x, int32_t y) {
    if (is_slider_dragging_) {
        return handle_touch_motion(x, y);
    }
    return layout_.window_rect().contains(core::Point{x, y});
}

bool SettingsController::handle_pointer_button(uint32_t button, uint32_t state, int32_t x, int32_t y) {
    (void)button;
    if (state == 1) {
        return handle_touch_down(x, y);
    } else {
        return handle_touch_up(x, y);
    }
}

bool SettingsController::handle_pointer_axis(double delta_x, double delta_y) {
    (void)delta_x;
    if (layout_.max_scroll_y() <= 0) return false;
    layout_.scroll_by(static_cast<int32_t>(delta_y * 32.0));
    request_render();
    return true;
}

bool SettingsController::handle_key(uint32_t key_symbol, uint32_t state, uint32_t modifiers) {
    (void)modifiers;
    if (state == 0) return false; // Ignore key releases

    // Escape: 0xff1b
    if (key_symbol == 0xff1b) {
        if (navigation_.is_searching()) {
            navigation_.clear_search();
            request_render();
            return true;
        }
        if (navigation_.is_in_category_detail()) {
            static_cast<void>(navigation_.navigate_back());
            request_render();
            return true;
        }
        if (on_request_close_) {
            on_request_close_();
            return true;
        }
    }

    // Backspace: 0xff08
    if (key_symbol == 0xff08) {
        if (navigation_.is_searching()) {
            std::string q = navigation_.search_query();
            if (!q.empty()) {
                q.pop_back();
                navigation_.set_search_query(q);
                request_render();
                return true;
            }
        }
    }

    // Enter: 0xff0d or Space: 0x0020 (when not actively searching)
    if (key_symbol == 0xff0d || (key_symbol == 0x0020 && !navigation_.is_searching())) {
        auto vis = current_visible_settings();
        if (navigation_.selected_index() < vis.size()) {
            const auto* def = vis[navigation_.selected_index()];
            if (def) {
                if (def->type == SettingType::Bool) {
                    auto cur = store_.get(def->key);
                    bool b = cur.has_value() ? cur->as_bool().value_or(false) : false;
                    static_cast<void>(store_.set(def->key, SettingsValue(!b)));
                    request_render();
                    return true;
                } else if (def->type == SettingType::Enum && !def->enum_values.empty()) {
                    auto cur = store_.get(def->key);
                    std::string cur_str = cur.has_value() ? cur->as_string().value_or("") : "";
                    size_t next_opt = 0;
                    for (size_t i = 0; i < def->enum_values.size(); ++i) {
                        if (def->enum_values[i] == cur_str) {
                            next_opt = (i + 1) % def->enum_values.size();
                            break;
                        }
                    }
                    static_cast<void>(store_.set(def->key, SettingsValue(def->enum_values[next_opt])));
                    request_render();
                    return true;
                }
            }
        }
    }

    // Printable ASCII characters for search (when key >= 32)
    if (key_symbol >= 32 && key_symbol <= 126) {
        std::string q = navigation_.search_query();
        q.push_back(static_cast<char>(key_symbol));
        navigation_.set_search_query(q);
        request_render();
        return true;
    }

    // Arrow Up: 0xff52
    if (key_symbol == 0xff52) {
        auto vis = current_visible_settings();
        navigation_.select_prev(vis.size());
        request_render();
        return true;
    }

    // Arrow Down: 0xff54
    if (key_symbol == 0xff54) {
        auto vis = current_visible_settings();
        navigation_.select_next(vis.size());
        request_render();
        return true;
    }

    // Arrow Left: 0xff51
    if (key_symbol == 0xff51) {
        auto vis = current_visible_settings();
        if (navigation_.selected_index() < vis.size()) {
            const auto* def = vis[navigation_.selected_index()];
            if (def && (def->type == SettingType::Int || def->type == SettingType::Double)) {
                double cur = store_.get(def->key).value_or(def->default_value).as_double().value_or(0.0);
                double min_v = def->min_value.value_or(0.0);
                double step = def->step_value > 0.0 ? def->step_value : 1.0;
                double next_v = std::max(min_v, cur - step);
                if (def->type == SettingType::Int) {
                    store_.set(def->key, SettingsValue(static_cast<int64_t>(std::round(next_v))));
                } else {
                    store_.set(def->key, SettingsValue(next_v));
                }
                request_render();
                return true;
            }
        }
    }

    // Arrow Right: 0xff53
    if (key_symbol == 0xff53) {
        auto vis = current_visible_settings();
        if (navigation_.selected_index() < vis.size()) {
            const auto* def = vis[navigation_.selected_index()];
            if (def && (def->type == SettingType::Int || def->type == SettingType::Double)) {
                double cur = store_.get(def->key).value_or(def->default_value).as_double().value_or(0.0);
                double max_v = def->max_value.value_or(100.0);
                double step = def->step_value > 0.0 ? def->step_value : 1.0;
                double next_v = std::min(max_v, cur + step);
                if (def->type == SettingType::Int) {
                    store_.set(def->key, SettingsValue(static_cast<int64_t>(std::round(next_v))));
                } else {
                    store_.set(def->key, SettingsValue(next_v));
                }
                request_render();
                return true;
            }
        }
    }

    return false;
}

} // namespace ldde::settings
