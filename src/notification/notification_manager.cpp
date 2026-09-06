#include "ldde/notification/notification_manager.hpp"
#include "ldde/notification/internal_notification_backend.hpp"
#include "ldde/notification/dbus_notification_backend.hpp"
#include "ldde/notification/notification_view.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::notification {

NotificationManager::NotificationManager() = default;

NotificationManager::~NotificationManager() {
    shutdown();
}

core::Status NotificationManager::initialize(
    shell::Shell& shell,
    window::WindowManager& window_manager,
    application::ApplicationCatalog& catalog,
    const display::DisplayPolicy& display_policy,
    const config::Config& config,
    core::EventLoop& event_loop,
    std::unique_ptr<NotificationBackend> backend) {
    if (initialized_) {
        return core::Status::ok();
    }

    shell_ = &shell;
    window_manager_ = &window_manager;
    catalog_ = &catalog;
    display_policy_ = display_policy;
    config_ = config;
    event_loop_ = &event_loop;

    // Read config settings
    size_t max_history = 50;
    size_t max_popups = 3;
    uint32_t popup_duration_ms = 5000;

    auto history_opt = config_.get_int("notifications", "max_history_entries");
    if (history_opt && *history_opt > 0) {
        max_history = static_cast<size_t>(*history_opt);
    }

    auto popups_opt = config_.get_int("notifications", "max_visible_popups");
    if (popups_opt && *popups_opt > 0) {
        max_popups = static_cast<size_t>(*popups_opt);
    }

    auto duration_opt = config_.get_int("notifications", "popup_duration_ms");
    if (duration_opt && *duration_opt > 0) {
        popup_duration_ms = static_cast<uint32_t>(*duration_opt);
    }

    store_.set_max_history_entries(max_history);
    presenter_ = std::make_unique<NotificationPresenter>(store_, event_loop_, max_popups, popup_duration_ms);
    controller_ = std::make_unique<NotificationController>(store_, *presenter_, center_state_, layout_);

    if (backend) {
        backend_ = std::move(backend);
    } else {
        backend_ = std::make_unique<DBusNotificationBackend>(event_loop_);
    }

    setup_backend();
    setup_callbacks();
    update_layout();

    initialized_ = true;
    LDDE_LOG_INFO(Notification, "NotificationManager initialized successfully");
    return core::Status::ok();
}

void NotificationManager::shutdown() {
    if (!initialized_) return;

    if (backend_) {
        backend_->stop();
        backend_.reset();
    }

    presenter_.reset();
    controller_.reset();

    shell_ = nullptr;
    window_manager_ = nullptr;
    catalog_ = nullptr;
    event_loop_ = nullptr;
    initialized_ = false;
    LDDE_LOG_INFO(Notification, "NotificationManager shut down");
}

void NotificationManager::setup_backend() {
    if (!backend_) return;

    backend_->on_notification_received([this](Notification n) {
        if (event_loop_) {
            // Process on event loop if needed or synchronously
            return post_notification(std::move(n));
        }
        return post_notification(std::move(n));
    });

    backend_->on_close_requested([this](NotificationId id) {
        return close_notification(id);
    });

    auto status = backend_->start();
    if (status.is_error()) {
        LDDE_LOG_WARN(Notification, "Failed to start notification backend: " << status.to_string());
    }
}

void NotificationManager::setup_callbacks() {
    store_.on_removed([this](NotificationId id, NotificationCloseReason reason) {
        if (backend_) {
            backend_->emit_notification_closed(id, reason);
        }
    });

    controller_->on_action_activated([this](NotificationId id, const std::string& key) {
        handle_action_activated(id, key);
    });

    controller_->on_default_activated([this](NotificationId id) {
        handle_default_activated(id);
    });

    presenter_->on_request_render([this]() {
        update_layout();
        if (request_render_callback_) {
            request_render_callback_();
        } else if (shell_) {
            shell_->render_all();
        }
    });

    center_state_.on_state_changed([this](NotificationCenterState /*old_state*/, NotificationCenterState /*new_state*/) {
        update_layout();
        if (request_render_callback_) {
            request_render_callback_();
        } else if (shell_) {
            shell_->render_all();
        }
    });
}

void NotificationManager::resolve_application(Notification& notif) {
    if (!catalog_) return;

    auto desktop_entry_hint = notif.get_hint("desktop-entry");
    if (desktop_entry_hint) {
        auto app = catalog_->find(application::ApplicationId(*desktop_entry_hint));
        if (app) {
            notif.set_app_id(app->id());
            if (notif.icon().empty()) {
                notif.set_icon(app->icon().raw());
            }
            return;
        }
    }

    // Fallback: search by app_name
    if (!notif.app_name().empty()) {
        auto app = catalog_->find(application::ApplicationId(notif.app_name()));
        if (app) {
            notif.set_app_id(app->id());
            if (notif.icon().empty()) {
                notif.set_icon(app->icon().raw());
            }
        }
    }
}

NotificationId NotificationManager::post_notification(Notification notification) {
    resolve_application(notification);
    NotificationId id = store_.add_or_replace(std::move(notification));
    if (presenter_) {
        presenter_->show(id);
    }
    update_layout();
    if (request_render_callback_) {
        request_render_callback_();
    } else if (shell_) {
        shell_->render_all();
    }
    return id;
}

NotificationId NotificationManager::post_system_notification(
    std::string summary,
    std::string body,
    NotificationUrgency urgency) {
    Notification notif(
        kInvalidNotificationId,
        "System",
        std::move(summary),
        std::move(body),
        "system",
        urgency);
    return post_notification(std::move(notif));
}

bool NotificationManager::close_notification(NotificationId id) {
    if (presenter_) {
        presenter_->dismiss(id);
    }
    bool res = store_.close(id, NotificationCloseReason::ClosedByCall);
    update_layout();
    if (request_render_callback_) {
        request_render_callback_();
    } else if (shell_) {
        shell_->render_all();
    }
    return res;
}

void NotificationManager::open_notification_center() {
    center_state_.transition_to(NotificationCenterState::Open);
}

void NotificationManager::close_notification_center() {
    center_state_.transition_to(NotificationCenterState::Closed);
}

void NotificationManager::toggle_notification_center() {
    if (center_state_.is_open()) {
        close_notification_center();
    } else {
        open_notification_center();
    }
}

bool NotificationManager::is_notification_center_open() const noexcept {
    return center_state_.is_open();
}

bool NotificationManager::has_visible_popups() const noexcept {
    return presenter_ && presenter_->has_visible_popups();
}

void NotificationManager::update_display_policy(const display::DisplayPolicy& policy) {
    display_policy_ = policy;
    update_layout();
}

void NotificationManager::update_layout() {
    if (!shell_ || !presenter_) return;
    layout_.update_popups(display_policy_, shell_->layout(), shell_->tokens(), presenter_->visible_popups());
    layout_.update_notification_center(display_policy_, shell_->layout(), shell_->tokens(), store_.all_notifications(),
                                       controller_ ? controller_->scroll_offset_y() : 0);
}

void NotificationManager::render_popups(
    shell::ShmBuffer& buffer,
    const shell::ShellTheme& theme,
    const shell::DesignTokens& tokens) {
    if (!presenter_) return;
    std::vector<const Notification*> const_popups;
    for (const auto* p : presenter_->visible_popups()) {
        const_popups.push_back(p);
    }
    NotificationView::render_popups(buffer, theme, tokens, layout_, const_popups);
}

void NotificationManager::render_notification_center(
    shell::ShmBuffer& buffer,
    const shell::ShellTheme& theme,
    const shell::DesignTokens& tokens) {
    NotificationView::render_notification_center(buffer, theme, tokens, layout_, store_.all_notifications());
}

bool NotificationManager::handle_touch_down(int32_t x, int32_t y) {
    if (!controller_) return false;
    return controller_->handle_touch_down(x, y);
}

bool NotificationManager::handle_touch_motion(int32_t x, int32_t y) {
    if (!controller_) return false;
    bool handled = controller_->handle_touch_motion(x, y);
    if (handled && center_state_.is_open()) {
        update_layout();
        if (request_render_callback_) {
            request_render_callback_();
        } else if (shell_) {
            shell_->render_all();
        }
    }
    return handled;
}

bool NotificationManager::handle_touch_up(int32_t x, int32_t y) {
    if (!controller_) return false;
    bool handled = controller_->handle_touch_up(x, y);
    if (handled) {
        update_layout();
        if (request_render_callback_) {
            request_render_callback_();
        } else if (shell_) {
            shell_->render_all();
        }
    }
    return handled;
}

void NotificationManager::handle_touch_cancel() {
    if (controller_) {
        controller_->handle_touch_cancel();
    }
}

bool NotificationManager::handle_key(uint32_t key_symbol, uint32_t state, uint32_t modifiers) {
    if (!controller_) return false;
    return controller_->handle_key(key_symbol, state, modifiers);
}

void NotificationManager::handle_action_activated(NotificationId id, const std::string& action_key) {
    LDDE_LOG_INFO(Notification, "Notification action activated [id=" << id << ", key=" << action_key << "]");
    if (backend_) {
        backend_->emit_action_invoked(id, action_key);
    }
}

void NotificationManager::handle_default_activated(NotificationId id) {
    LDDE_LOG_INFO(Notification, "Notification default activated [id=" << id << "]");
    if (backend_) {
        backend_->emit_action_invoked(id, "default");
    }

    auto* notif = store_.find(id);
    if (notif && notif->app_id() && window_manager_) {
        // Find existing window for this application and activate it via D3
        const auto& app_id_str = notif->app_id()->value();
        for (const auto& win : window_manager_->registry().windows()) {
            if (win && win->app_id() == app_id_str) {
                window_manager_->activate(win->id());
                break;
            }
        }
    }
}

} // namespace ldde::notification

