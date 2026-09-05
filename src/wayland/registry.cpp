#include "ldde/wayland/registry.hpp"
#include "ldde/core/logging.hpp"

#include <cstring>

namespace ldde::wayland {

const wl_registry_listener WaylandRegistry::registry_listener_ = {
    .global = [](void* data, wl_registry*, uint32_t name, const char* interface, uint32_t version) {
        static_cast<WaylandRegistry*>(data)->on_global_added(name, interface, version);
    },
    .global_remove = [](void* data, wl_registry*, uint32_t name) {
        static_cast<WaylandRegistry*>(data)->on_global_removed(name);
    }
};

WaylandRegistry::WaylandRegistry() = default;

WaylandRegistry::~WaylandRegistry() {
    reset();
}

Status WaylandRegistry::initialize(wl_display* display) {
    if (!display) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Wayland,
                                 core::ErrorCode::WaylandDisconnected,
                                 "Cannot initialize registry on null display");
    }

    wl_registry* reg = wl_display_get_registry(display);
    if (!reg) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Wayland,
                                 core::ErrorCode::WaylandProtocolError,
                                 "wl_display_get_registry failed");
    }

    registry_.reset(reg);
    wl_registry_add_listener(reg, &registry_listener_, this);

    LDDE_LOG_DEBUG(Wayland, "Wayland registry listener attached");
    return Status::ok();
}

void WaylandRegistry::reset() noexcept {
    globals_.clear();
    registry_.reset();
}

bool WaylandRegistry::has_global(std::string_view interface) const {
    for (const auto& [name, info] : globals_) {
        if (info.interface == interface) {
            return true;
        }
    }
    return false;
}

std::optional<GlobalInfo> WaylandRegistry::get_global(std::string_view interface) const {
    for (const auto& [name, info] : globals_) {
        if (info.interface == interface) {
            return info;
        }
    }
    return std::nullopt;
}

Status WaylandRegistry::verify_required_globals() const {
    // Required base protocols for any Wayland client desktop
    static constexpr std::string_view kRequiredGlobals[] = {
        "wl_compositor",
        "wl_shm"
    };

    for (const auto& req : kRequiredGlobals) {
        if (!has_global(req)) {
            return LDDE_STATUS_ERROR(core::ErrorCategory::Wayland,
                                     core::ErrorCode::WaylandGlobalMissing,
                                     "Required Wayland global interface missing: " + std::string(req));
        }
    }

    return Status::ok();
}

void WaylandRegistry::add_global_listener(std::string_view interface,
                                         GlobalAddCallback on_add,
                                         GlobalRemoveCallback on_remove) {
    listeners_.push_back(ListenerEntry{
        .interface = std::string(interface),
        .on_add = std::move(on_add),
        .on_remove = std::move(on_remove)
    });

    // Check existing globals
    for (const auto& [name, info] : globals_) {
        if (info.interface == interface) {
            if (listeners_.back().on_add) {
                listeners_.back().on_add(name, info.interface, info.version);
            }
        }
    }
}

void WaylandRegistry::on_global_added(uint32_t name, const char* interface, uint32_t version) {
    GlobalInfo info{name, interface ? interface : "", version};
    globals_[name] = info;

    LDDE_LOG_TRACE(Wayland, "Discovered global: " << info.interface
                                                 << " (v" << info.version << ", id " << name << ")");

    for (const auto& listener : listeners_) {
        if (listener.interface == info.interface && listener.on_add) {
            listener.on_add(name, info.interface, info.version);
        }
    }
}

void WaylandRegistry::on_global_removed(uint32_t name) {
    auto it = globals_.find(name);
    if (it == globals_.end()) {
        return;
    }

    GlobalInfo info = it->second;
    globals_.erase(it);

    LDDE_LOG_TRACE(Wayland, "Removed global: " << info.interface << " (id " << name << ")");

    for (const auto& listener : listeners_) {
        if (listener.interface == info.interface && listener.on_remove) {
            listener.on_remove(name);
        }
    }
}

} // namespace ldde::wayland

