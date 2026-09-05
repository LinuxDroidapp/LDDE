#include "ldde/window/window_tracker.hpp"
#include <wayland-server-protocol.h>
#include "ldde/wayland/xdg-shell-server-protocol.h"
#include "ldde/core/logging.hpp"
#include <algorithm>
#include <cstring>
#include <unistd.h>

namespace ldde::window {

const xdg_wm_base_listener WindowTracker::wm_base_listener_ = {
    .ping = [](void* data, xdg_wm_base* /*wm_base*/, uint32_t serial) {
        static_cast<WindowTracker*>(data)->on_wm_base_ping(serial);
    }
};

const xdg_surface_listener WindowTracker::surface_listener_ = {
    .configure = [](void* data, xdg_surface* surface, uint32_t serial) {
        static_cast<WindowTracker*>(data)->on_surface_configure(surface, serial);
    }
};

const xdg_toplevel_listener WindowTracker::toplevel_listener_ = {
    .configure = [](void* data, xdg_toplevel* toplevel, int32_t width, int32_t height, wl_array* states) {
        static_cast<WindowTracker*>(data)->on_toplevel_configure(toplevel, width, height, states);
    },
    .close = [](void* data, xdg_toplevel* toplevel) {
        static_cast<WindowTracker*>(data)->on_toplevel_close(toplevel);
    },
    .configure_bounds = [](void*, xdg_toplevel*, int32_t, int32_t) {},
    .wm_capabilities = [](void*, xdg_toplevel*, wl_array*) {}
};

WindowTracker::WindowTracker() = default;

WindowTracker::~WindowTracker() {
    shutdown();
}

WindowId WindowTracker::generate_id() noexcept {
    return next_window_id_++;
}

Status WindowTracker::initialize(wayland::WaylandConnection& /*connection*/,
                                 wayland::WaylandRegistry& registry,
                                 WindowRegistry& window_registry,
                                 const config::Config& config) {
    if (initialized_) {
        return Status::ok();
    }

    registry_ = &window_registry;

    // 1. Discover and bind xdg_wm_base from compositor
    auto wm_base_global = registry.get_global("xdg_wm_base");
    if (wm_base_global) {
        xdg_version_ = std::min(wm_base_global->version, 5u);
        wm_base_ = registry.bind<xdg_wm_base>(wm_base_global->name, &xdg_wm_base_interface, xdg_version_);
        if (wm_base_) {
            xdg_wm_base_add_listener(wm_base_, &wm_base_listener_, this);
            LDDE_LOG_INFO(Window, "Bound xdg_wm_base version " << xdg_version_ << " from compositor");
        } else {
            LDDE_LOG_WARN(Window, "Failed to bind xdg_wm_base");
        }
    } else {
        LDDE_LOG_WARN(Window, "xdg_wm_base protocol not advertised by compositor");
    }

    // 2. Start Application Tracking Server if configured
    bool server_enabled = config.get_bool("window_tracking", "enabled").value_or(true);
    if (server_enabled) {
        std::string sock = config.get_string("window_tracking", "socket_name").value_or("wayland-ldde-apps");
        Status ss = start_application_server(sock);
        if (ss.is_error()) {
            LDDE_LOG_WARN(Window, "Could not start application tracking server: " << ss.to_string());
        }
    }

    initialized_ = true;
    LDDE_LOG_INFO(Window, "WindowTracker initialized successfully");
    return Status::ok();
}

void WindowTracker::shutdown() noexcept {
    if (!initialized_) {
        return;
    }

    stop_application_server();

    if (wm_base_) {
        xdg_wm_base_destroy(wm_base_);
        wm_base_ = nullptr;
    }

    if (registry_) {
        registry_->clear();
        registry_ = nullptr;
    }

    initialized_ = false;
    LDDE_LOG_INFO(Window, "WindowTracker shut down successfully");
}

std::shared_ptr<Window> WindowTracker::create_tracked_window(wl_surface* surface,
                                                            std::string_view title,
                                                            std::string_view app_id) {
    if (!surface) {
        LDDE_LOG_ERROR(Window, "Cannot track window with null surface");
        return nullptr;
    }

    if (!wm_base_) {
        LDDE_LOG_ERROR(Window, "Cannot create tracked window without xdg_wm_base");
        return nullptr;
    }

    xdg_surface* xdg_surf = xdg_wm_base_get_xdg_surface(wm_base_, surface);
    if (!xdg_surf) {
        LDDE_LOG_ERROR(Window, "xdg_wm_base_get_xdg_surface failed");
        return nullptr;
    }

    xdg_toplevel* toplevel = xdg_surface_get_toplevel(xdg_surf);
    if (!toplevel) {
        xdg_surface_destroy(xdg_surf);
        LDDE_LOG_ERROR(Window, "xdg_surface_get_toplevel failed");
        return nullptr;
    }

    WindowId id = generate_id();
    auto window = std::make_shared<Window>(id, surface, xdg_surf, toplevel);

    if (!title.empty()) {
        xdg_toplevel_set_title(toplevel, std::string(title).c_str());
        window->set_title(title);
    }
    if (!app_id.empty()) {
        xdg_toplevel_set_app_id(toplevel, std::string(app_id).c_str());
        window->set_app_id(app_id);
    }

    xdg_surface_add_listener(xdg_surf, &surface_listener_, this);
    xdg_toplevel_add_listener(toplevel, &toplevel_listener_, this);

    static_cast<void>(window->transition_to(WindowLifecycleState::Initializing));

    if (registry_) {
        static_cast<void>(registry_->add_window(window));
    }

    return window;
}

void WindowTracker::destroy_window(WindowId id) {
    if (!registry_) return;

    auto window = registry_->lookup(id);
    if (!window) return;

    if (window->toplevel()) {
        xdg_toplevel_destroy(window->toplevel());
    }
    if (window->xdg_surf()) {
        xdg_surface_destroy(window->xdg_surf());
    }

    static_cast<void>(registry_->remove_window(id));
}

void WindowTracker::on_wm_base_ping(uint32_t serial) {
    if (wm_base_) {
        xdg_wm_base_pong(wm_base_, serial);
    }
}

void WindowTracker::on_toplevel_configure(xdg_toplevel* toplevel, int32_t width, int32_t height, wl_array* states) {
    if (!registry_ || !toplevel) return;

    auto window = registry_->find_by_toplevel(toplevel);
    if (!window) return;

    bool maximized = false;
    bool fullscreen = false;
    bool activated = false;

    if (states && states->data) {
        const auto* state_entry = static_cast<const uint32_t*>(states->data);
        size_t count = states->size / sizeof(uint32_t);
        for (size_t i = 0; i < count; ++i) {
            switch (state_entry[i]) {
                case XDG_TOPLEVEL_STATE_MAXIMIZED:  maximized = true; break;
                case XDG_TOPLEVEL_STATE_FULLSCREEN: fullscreen = true; break;
                case XDG_TOPLEVEL_STATE_ACTIVATED:  activated = true; break;
                default: break;
            }
        }
    }

    WindowState new_state = WindowState::Normal;
    if (fullscreen) {
        new_state = WindowState::Fullscreen;
    } else if (maximized) {
        new_state = WindowState::Maximized;
    }

    if (window->state() != new_state) {
        window->set_state(new_state);
        registry_->dispatch_event(WindowEvent{
            .type = WindowEventType::StateChanged,
            .window_id = window->id(),
            .window = window,
            .property_name = "state"
        });
    }

    if (window->is_active() != activated) {
        window->set_active(activated);
        registry_->dispatch_event(WindowEvent{
            .type = WindowEventType::FocusChanged,
            .window_id = window->id(),
            .window = window,
            .property_name = activated ? "focused" : "unfocused"
        });
    }

    if (width > 0 && height > 0) {
        core::Rect new_geom = window->geometry();
        new_geom.width = width;
        new_geom.height = height;
        window->set_geometry(new_geom);
        window->set_surface_size(core::Size{width, height});
    }
}

void WindowTracker::on_surface_configure(xdg_surface* surface, uint32_t serial) {
    if (!registry_ || !surface) return;

    for (const auto& win : registry_->windows()) {
        if (win && win->xdg_surf() == surface) {
            win->ack_configure(serial);

            if (win->lifecycle_state() == WindowLifecycleState::Initializing) {
                static_cast<void>(win->transition_to(WindowLifecycleState::Ready));
                static_cast<void>(win->transition_to(WindowLifecycleState::Visible));
                win->set_visible(true);
                registry_->dispatch_event(WindowEvent{
                    .type = WindowEventType::VisibilityChanged,
                    .window_id = win->id(),
                    .window = win,
                    .property_name = "visible"
                });
            }

            registry_->dispatch_event(WindowEvent{
                .type = WindowEventType::GeometryChanged,
                .window_id = win->id(),
                .window = win,
                .property_name = "configure"
            });
            break;
        }
    }
}

void WindowTracker::on_toplevel_close(xdg_toplevel* toplevel) {
    if (!registry_ || !toplevel) return;

    auto window = registry_->find_by_toplevel(toplevel);
    if (!window) return;

    window->request_close();
    registry_->dispatch_event(WindowEvent{
        .type = WindowEventType::Closed,
        .window_id = window->id(),
        .window = window,
        .property_name = "closed"
    });
}

// -------------------------------------------------------------------------
// Application Server Endpoint Implementation
// -------------------------------------------------------------------------

namespace {

struct ServerSurfaceData {
    WindowTracker* tracker = nullptr;
    wl_resource* surface_res = nullptr;
    std::shared_ptr<Window> window;
};

struct ServerXdgSurfaceData {
    WindowTracker* tracker = nullptr;
    wl_resource* xdg_surface_res = nullptr;
    ServerSurfaceData* surface_data = nullptr;
    std::shared_ptr<Window> window;
};

struct ServerToplevelData {
    WindowTracker* tracker = nullptr;
    WindowId window_id = kInvalidWindowId;
    wl_resource* toplevel_res = nullptr;
    ServerXdgSurfaceData* xdg_surface_data = nullptr;
    std::shared_ptr<Window> window;
};

// Region interface
const struct wl_region_interface server_region_interface = {
    .destroy = [](wl_client*, wl_resource* resource) {
        wl_resource_destroy(resource);
    },
    .add = [](wl_client*, wl_resource*, int32_t, int32_t, int32_t, int32_t) {},
    .subtract = [](wl_client*, wl_resource*, int32_t, int32_t, int32_t, int32_t) {}
};

// Surface interface
const struct wl_surface_interface server_surface_interface = {
    .destroy = [](wl_client*, wl_resource* resource) {
        wl_resource_destroy(resource);
    },
    .attach = [](wl_client*, wl_resource*, wl_resource*, int32_t, int32_t) {},
    .damage = [](wl_client*, wl_resource*, int32_t, int32_t, int32_t, int32_t) {},
    .frame = [](wl_client* client, wl_resource*, uint32_t callback) {
        wl_resource* cb = wl_resource_create(client, &wl_callback_interface, 1, callback);
        if (cb) {
            wl_callback_send_done(cb, 0);
            wl_resource_destroy(cb);
        }
    },
    .set_opaque_region = [](wl_client*, wl_resource*, wl_resource*) {},
    .set_input_region = [](wl_client*, wl_resource*, wl_resource*) {},
    .commit = [](wl_client*, wl_resource* resource) {
        auto* surf_data = static_cast<ServerSurfaceData*>(wl_resource_get_user_data(resource));
        if (surf_data && surf_data->window) {
            auto& win = surf_data->window;
            if (win->lifecycle_state() == WindowLifecycleState::Initializing) {
                static_cast<void>(win->transition_to(WindowLifecycleState::Ready));
                static_cast<void>(win->transition_to(WindowLifecycleState::Visible));
                win->set_visible(true);
                if (surf_data->tracker && surf_data->tracker->registry()) {
                    surf_data->tracker->registry()->dispatch_event(WindowEvent{
                        .type = WindowEventType::VisibilityChanged,
                        .window_id = win->id(),
                        .window = win,
                        .property_name = "visible"
                    });
                }
            }
        }
    },
    .set_buffer_transform = [](wl_client*, wl_resource*, int32_t) {},
    .set_buffer_scale = [](wl_client*, wl_resource*, int32_t) {},
    .damage_buffer = [](wl_client*, wl_resource*, int32_t, int32_t, int32_t, int32_t) {},
    .offset = [](wl_client*, wl_resource*, int32_t, int32_t) {}
};

// Compositor interface
const struct wl_compositor_interface server_compositor_interface = {
    .create_surface = [](wl_client* client, wl_resource* resource, uint32_t id) {
        auto* tracker = static_cast<WindowTracker*>(wl_resource_get_user_data(resource));
        wl_resource* surf_res = wl_resource_create(client, &wl_surface_interface, wl_resource_get_version(resource), id);
        if (!surf_res) return;

        auto* surf_data = new ServerSurfaceData{tracker, surf_res, nullptr};
        wl_resource_set_implementation(surf_res, &server_surface_interface, surf_data, [](wl_resource* res) {
            delete static_cast<ServerSurfaceData*>(wl_resource_get_user_data(res));
        });
    },
    .create_region = [](wl_client* client, wl_resource* resource, uint32_t id) {
        wl_resource* reg_res = wl_resource_create(client, &wl_region_interface, wl_resource_get_version(resource), id);
        if (reg_res) {
            wl_resource_set_implementation(reg_res, &server_region_interface, nullptr, nullptr);
        }
    }
};

static void server_compositor_bind(wl_client* client, void* data, uint32_t version, uint32_t id) {
    wl_resource* res = wl_resource_create(client, &wl_compositor_interface, static_cast<int>(version), id);
    if (res) {
        wl_resource_set_implementation(res, &server_compositor_interface, data, nullptr);
    }
}

// Positioner interface
const struct xdg_positioner_interface server_positioner_interface = {
    .destroy = [](wl_client*, wl_resource* resource) {
        wl_resource_destroy(resource);
    },
    .set_size = [](wl_client*, wl_resource*, int32_t, int32_t) {},
    .set_anchor_rect = [](wl_client*, wl_resource*, int32_t, int32_t, int32_t, int32_t) {},
    .set_anchor = [](wl_client*, wl_resource*, uint32_t) {},
    .set_gravity = [](wl_client*, wl_resource*, uint32_t) {},
    .set_constraint_adjustment = [](wl_client*, wl_resource*, uint32_t) {},
    .set_offset = [](wl_client*, wl_resource*, int32_t, int32_t) {},
    .set_reactive = [](wl_client*, wl_resource*) {},
    .set_parent_size = [](wl_client*, wl_resource*, int32_t, int32_t) {},
    .set_parent_configure = [](wl_client*, wl_resource*, uint32_t) {}
};

// Popup interface
const struct xdg_popup_interface server_popup_interface = {
    .destroy = [](wl_client*, wl_resource* resource) {
        wl_resource_destroy(resource);
    },
    .grab = [](wl_client*, wl_resource*, wl_resource*, uint32_t) {},
    .reposition = [](wl_client*, wl_resource*, wl_resource*, uint32_t) {}
};

// Toplevel interface
const struct xdg_toplevel_interface server_toplevel_interface = {
    .destroy = [](wl_client*, wl_resource* resource) {
        wl_resource_destroy(resource);
    },
    .set_parent = [](wl_client*, wl_resource* resource, wl_resource* parent_resource) {
        auto* data = static_cast<ServerToplevelData*>(wl_resource_get_user_data(resource));
        if (!data || !data->window) return;

        std::optional<WindowId> parent_id;
        if (parent_resource) {
            auto* pdata = static_cast<ServerToplevelData*>(wl_resource_get_user_data(parent_resource));
            if (pdata) parent_id = pdata->window_id;
        }

        data->window->set_parent_id(parent_id);
        if (data->tracker && data->tracker->registry()) {
            data->tracker->registry()->dispatch_event(WindowEvent{
                .type = WindowEventType::ParentChanged,
                .window_id = data->window->id(),
                .window = data->window,
                .property_name = "parent"
            });
        }
    },
    .set_title = [](wl_client*, wl_resource* resource, const char* title) {
        auto* data = static_cast<ServerToplevelData*>(wl_resource_get_user_data(resource));
        if (!data || !data->window) return;

        data->window->set_title(title ? title : "");
        if (data->tracker && data->tracker->registry()) {
            data->tracker->registry()->dispatch_event(WindowEvent{
                .type = WindowEventType::TitleChanged,
                .window_id = data->window->id(),
                .window = data->window,
                .property_name = "title"
            });
        }
    },
    .set_app_id = [](wl_client*, wl_resource* resource, const char* app_id) {
        auto* data = static_cast<ServerToplevelData*>(wl_resource_get_user_data(resource));
        if (!data || !data->window) return;

        data->window->set_app_id(app_id ? app_id : "");
        if (data->tracker && data->tracker->registry()) {
            data->tracker->registry()->dispatch_event(WindowEvent{
                .type = WindowEventType::AppIdChanged,
                .window_id = data->window->id(),
                .window = data->window,
                .property_name = "app_id"
            });
        }
    },
    .show_window_menu = [](wl_client*, wl_resource*, wl_resource*, uint32_t, int32_t, int32_t) {},
    .move = [](wl_client*, wl_resource*, wl_resource*, uint32_t) {},
    .resize = [](wl_client*, wl_resource*, wl_resource*, uint32_t, uint32_t) {},
    .set_max_size = [](wl_client*, wl_resource*, int32_t, int32_t) {},
    .set_min_size = [](wl_client*, wl_resource*, int32_t, int32_t) {},
    .set_maximized = [](wl_client*, wl_resource* resource) {
        auto* data = static_cast<ServerToplevelData*>(wl_resource_get_user_data(resource));
        if (!data || !data->window) return;

        if (data->window->state() != WindowState::Maximized) {
            data->window->set_state(WindowState::Maximized);
            if (data->tracker && data->tracker->registry()) {
                data->tracker->registry()->dispatch_event(WindowEvent{
                    .type = WindowEventType::StateChanged,
                    .window_id = data->window->id(),
                    .window = data->window,
                    .property_name = "state"
                });
            }
        }
    },
    .unset_maximized = [](wl_client*, wl_resource* resource) {
        auto* data = static_cast<ServerToplevelData*>(wl_resource_get_user_data(resource));
        if (!data || !data->window) return;

        if (data->window->state() != WindowState::Normal) {
            data->window->set_state(WindowState::Normal);
            if (data->tracker && data->tracker->registry()) {
                data->tracker->registry()->dispatch_event(WindowEvent{
                    .type = WindowEventType::StateChanged,
                    .window_id = data->window->id(),
                    .window = data->window,
                    .property_name = "state"
                });
            }
        }
    },
    .set_fullscreen = [](wl_client*, wl_resource* resource, wl_resource*) {
        auto* data = static_cast<ServerToplevelData*>(wl_resource_get_user_data(resource));
        if (!data || !data->window) return;

        if (data->window->state() != WindowState::Fullscreen) {
            data->window->set_state(WindowState::Fullscreen);
            if (data->tracker && data->tracker->registry()) {
                data->tracker->registry()->dispatch_event(WindowEvent{
                    .type = WindowEventType::StateChanged,
                    .window_id = data->window->id(),
                    .window = data->window,
                    .property_name = "state"
                });
            }
        }
    },
    .unset_fullscreen = [](wl_client*, wl_resource* resource) {
        auto* data = static_cast<ServerToplevelData*>(wl_resource_get_user_data(resource));
        if (!data || !data->window) return;

        if (data->window->state() != WindowState::Normal) {
            data->window->set_state(WindowState::Normal);
            if (data->tracker && data->tracker->registry()) {
                data->tracker->registry()->dispatch_event(WindowEvent{
                    .type = WindowEventType::StateChanged,
                    .window_id = data->window->id(),
                    .window = data->window,
                    .property_name = "state"
                });
            }
        }
    },
    .set_minimized = [](wl_client*, wl_resource* resource) {
        auto* data = static_cast<ServerToplevelData*>(wl_resource_get_user_data(resource));
        if (!data || !data->window) return;

        if (data->window->state() != WindowState::Minimized) {
            data->window->set_state(WindowState::Minimized);
            if (data->tracker && data->tracker->registry()) {
                data->tracker->registry()->dispatch_event(WindowEvent{
                    .type = WindowEventType::StateChanged,
                    .window_id = data->window->id(),
                    .window = data->window,
                    .property_name = "state"
                });
            }
        }
    }
};

// XDG Surface interface
const struct xdg_surface_interface server_xdg_surface_interface = {
    .destroy = [](wl_client*, wl_resource* resource) {
        wl_resource_destroy(resource);
    },
    .get_toplevel = [](wl_client* client, wl_resource* resource, uint32_t id) {
        auto* xdg_surf_data = static_cast<ServerXdgSurfaceData*>(wl_resource_get_user_data(resource));
        if (!xdg_surf_data || !xdg_surf_data->tracker) return;

        WindowTracker* tracker = xdg_surf_data->tracker;
        WindowId win_id = tracker->generate_id();

        wl_resource* toplevel_res = wl_resource_create(client, &xdg_toplevel_interface, wl_resource_get_version(resource), id);
        if (!toplevel_res) return;

        auto window = std::make_shared<Window>(win_id, nullptr, nullptr, nullptr);
        static_cast<void>(window->transition_to(WindowLifecycleState::Initializing));

        auto* toplevel_data = new ServerToplevelData{
            .tracker = tracker,
            .window_id = win_id,
            .toplevel_res = toplevel_res,
            .xdg_surface_data = xdg_surf_data,
            .window = window
        };

        xdg_surf_data->window = window;
        if (xdg_surf_data->surface_data) {
            xdg_surf_data->surface_data->window = window;
        }

        tracker->register_server_toplevel(win_id, toplevel_data);

        wl_resource_set_implementation(toplevel_res, &server_toplevel_interface, toplevel_data, [](wl_resource* res) {
            auto* data = static_cast<ServerToplevelData*>(wl_resource_get_user_data(res));
            if (data) {
                if (data->tracker) {
                    data->tracker->unregister_server_toplevel(data->window_id);
                    if (data->tracker->registry() && data->window) {
                        static_cast<void>(data->window->transition_to(WindowLifecycleState::Closing));
                        static_cast<void>(data->tracker->registry()->remove_window(data->window_id));
                    }
                }
                delete data;
            }
        });

        if (tracker->registry()) {
            static_cast<void>(tracker->registry()->add_window(window));
        }

        // Emit initial configure sequence to client
        wl_array states;
        wl_array_init(&states);
        xdg_toplevel_send_configure(toplevel_res, 0, 0, &states);
        wl_array_release(&states);

        uint32_t serial = wl_display_next_serial(tracker->server_display());
        window->set_last_configure_serial(serial);
        xdg_surface_send_configure(resource, serial);
    },
    .get_popup = [](wl_client* client, wl_resource* resource, uint32_t id, wl_resource*, wl_resource*) {
        wl_resource* popup_res = wl_resource_create(client, &xdg_popup_interface, wl_resource_get_version(resource), id);
        if (popup_res) {
            wl_resource_set_implementation(popup_res, &server_popup_interface, nullptr, nullptr);
        }
    },
    .set_window_geometry = [](wl_client*, wl_resource* resource, int32_t x, int32_t y, int32_t width, int32_t height) {
        auto* xdg_surf_data = static_cast<ServerXdgSurfaceData*>(wl_resource_get_user_data(resource));
        if (xdg_surf_data && xdg_surf_data->window) {
            core::Rect new_geom{x, y, width, height};
            xdg_surf_data->window->set_geometry(new_geom);
            xdg_surf_data->window->set_surface_size(core::Size{width, height});
            if (xdg_surf_data->tracker && xdg_surf_data->tracker->registry()) {
                xdg_surf_data->tracker->registry()->dispatch_event(WindowEvent{
                    .type = WindowEventType::GeometryChanged,
                    .window_id = xdg_surf_data->window->id(),
                    .window = xdg_surf_data->window,
                    .property_name = "geometry"
                });
            }
        }
    },
    .ack_configure = [](wl_client*, wl_resource* resource, uint32_t serial) {
        auto* xdg_surf_data = static_cast<ServerXdgSurfaceData*>(wl_resource_get_user_data(resource));
        if (xdg_surf_data && xdg_surf_data->window) {
            xdg_surf_data->window->ack_configure(serial);
        }
    }
};

// XDG WM Base interface
const struct xdg_wm_base_interface server_wm_base_interface = {
    .destroy = [](wl_client*, wl_resource* resource) {
        wl_resource_destroy(resource);
    },
    .create_positioner = [](wl_client* client, wl_resource* resource, uint32_t id) {
        wl_resource* pos_res = wl_resource_create(client, &xdg_positioner_interface, wl_resource_get_version(resource), id);
        if (pos_res) {
            wl_resource_set_implementation(pos_res, &server_positioner_interface, nullptr, nullptr);
        }
    },
    .get_xdg_surface = [](wl_client* client, wl_resource* resource, uint32_t id, wl_resource* surface) {
        auto* tracker = static_cast<WindowTracker*>(wl_resource_get_user_data(resource));
        auto* surf_data = static_cast<ServerSurfaceData*>(wl_resource_get_user_data(surface));

        wl_resource* xdg_surf_res = wl_resource_create(client, &xdg_surface_interface, wl_resource_get_version(resource), id);
        if (!xdg_surf_res) return;

        auto* xdg_surf_data = new ServerXdgSurfaceData{
            .tracker = tracker,
            .xdg_surface_res = xdg_surf_res,
            .surface_data = surf_data,
            .window = nullptr
        };

        wl_resource_set_implementation(xdg_surf_res, &server_xdg_surface_interface, xdg_surf_data, [](wl_resource* res) {
            delete static_cast<ServerXdgSurfaceData*>(wl_resource_get_user_data(res));
        });
    },
    .pong = [](wl_client*, wl_resource*, uint32_t) {}
};

static void server_wm_base_bind(wl_client* client, void* data, uint32_t version, uint32_t id) {
    wl_resource* res = wl_resource_create(client, &xdg_wm_base_interface, static_cast<int>(version), id);
    if (res) {
        wl_resource_set_implementation(res, &server_wm_base_interface, data, nullptr);
    }
}

} // namespace

Status WindowTracker::start_application_server(std::string_view socket_name) {
    if (server_display_) {
        return Status::ok();
    }

    server_display_ = wl_display_create();
    if (!server_display_) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Wayland,
                                 core::ErrorCode::WaylandConnectionFailed,
                                 "Failed to create Wayland server display for application tracking");
    }

    app_socket_name_ = socket_name.empty() ? "wayland-ldde-apps" : std::string(socket_name);
    int ret = wl_display_add_socket(server_display_, app_socket_name_.c_str());
    if (ret != 0) {
        wl_display_destroy(server_display_);
        server_display_ = nullptr;
        return LDDE_STATUS_ERROR(core::ErrorCategory::Wayland,
                                 core::ErrorCode::WaylandConnectionFailed,
                                 "Failed to add socket for application tracking: " + app_socket_name_);
    }

    setup_server_globals();

    auto* loop = wl_display_get_event_loop(server_display_);
    server_fd_ = wl_event_loop_get_fd(loop);

    LDDE_LOG_INFO(Window, "Application tracking Wayland server listening on " << app_socket_name_);
    return Status::ok();
}

void WindowTracker::stop_application_server() noexcept {
    if (server_display_) {
        LDDE_LOG_INFO(Window, "Stopping application tracking Wayland server");
        if (compositor_global_) {
            wl_global_destroy(compositor_global_);
            compositor_global_ = nullptr;
        }
        if (wm_base_global_) {
            wl_global_destroy(wm_base_global_);
            wm_base_global_ = nullptr;
        }
        server_toplevels_.clear();
        wl_display_destroy(server_display_);
        server_display_ = nullptr;
        server_fd_ = -1;
    }
}

void WindowTracker::dispatch_server() {
    if (!server_display_) return;

    auto* loop = wl_display_get_event_loop(server_display_);
    wl_event_loop_dispatch(loop, 0);
    wl_display_flush_clients(server_display_);
}

void WindowTracker::setup_server_globals() {
    if (!server_display_) return;

    compositor_global_ = wl_global_create(server_display_, &wl_compositor_interface, 4, this, server_compositor_bind);
    wm_base_global_ = wl_global_create(server_display_, &xdg_wm_base_interface, 5, this, server_wm_base_bind);
}

void WindowTracker::register_server_toplevel(WindowId id, void* toplevel_data) {
    server_toplevels_[id] = toplevel_data;
}

void WindowTracker::unregister_server_toplevel(WindowId id) {
    server_toplevels_.erase(id);
}

void WindowTracker::send_close_to_client(WindowId id) {
    auto it = server_toplevels_.find(id);
    if (it != server_toplevels_.end() && it->second) {
        auto* data = static_cast<ServerToplevelData*>(it->second);
        if (data->toplevel_res) {
            xdg_toplevel_send_close(data->toplevel_res);
            if (data->window) {
                data->window->request_close();
                if (registry_) {
                    registry_->dispatch_event(WindowEvent{
                        .type = WindowEventType::Closed,
                        .window_id = id,
                        .window = data->window,
                        .property_name = "closed"
                    });
                }
            }
        }
    }
}

void WindowTracker::send_configure_to_client(WindowId id, int32_t width, int32_t height, const std::vector<uint32_t>& states) {
    auto it = server_toplevels_.find(id);
    if (it != server_toplevels_.end() && it->second) {
        auto* data = static_cast<ServerToplevelData*>(it->second);
        if (data->toplevel_res && data->xdg_surface_data && data->xdg_surface_data->xdg_surface_res) {
            wl_array wl_states;
            wl_array_init(&wl_states);
            for (uint32_t s : states) {
                uint32_t* p = static_cast<uint32_t*>(wl_array_add(&wl_states, sizeof(uint32_t)));
                if (p) *p = s;
            }
            xdg_toplevel_send_configure(data->toplevel_res, width, height, &wl_states);
            wl_array_release(&wl_states);

            uint32_t serial = wl_display_next_serial(server_display_);
            if (data->window) {
                data->window->set_last_configure_serial(serial);
            }
            xdg_surface_send_configure(data->xdg_surface_data->xdg_surface_res, serial);
        }
    }
}

} // namespace ldde::window
