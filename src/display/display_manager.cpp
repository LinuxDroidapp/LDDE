#include "ldde/display/display_manager.hpp"
#include "ldde/core/logging.hpp"

#include <algorithm>

namespace ldde::display {

const wl_output_listener DisplayManager::output_listener_ = {
    .geometry = [](void* data, wl_output*, int32_t x, int32_t y,
                   int32_t physical_width, int32_t physical_height,
                   int32_t subpixel, const char* make, const char* model,
                   int32_t transform) {
        auto* handle = static_cast<OutputHandle*>(data);
        handle->manager->handle_geometry(handle, x, y, physical_width, physical_height,
                                         subpixel, make, model, transform);
    },
    .mode = [](void* data, wl_output*, uint32_t flags, int32_t width,
               int32_t height, int32_t refresh) {
        auto* handle = static_cast<OutputHandle*>(data);
        handle->manager->handle_mode(handle, flags, width, height, refresh);
    },
    .done = [](void* data, wl_output*) {
        auto* handle = static_cast<OutputHandle*>(data);
        handle->manager->handle_done(handle);
    },
    .scale = [](void* data, wl_output*, int32_t factor) {
        auto* handle = static_cast<OutputHandle*>(data);
        handle->manager->handle_scale(handle, factor);
    },
#if defined(WL_OUTPUT_NAME_SINCE_VERSION)
    .name = [](void* data, wl_output*, const char* name) {
        auto* handle = static_cast<OutputHandle*>(data);
        handle->manager->handle_name(handle, name);
    },
#endif
#if defined(WL_OUTPUT_DESCRIPTION_SINCE_VERSION)
    .description = [](void* data, wl_output*, const char* description) {
        auto* handle = static_cast<OutputHandle*>(data);
        handle->manager->handle_description(handle, description);
    }
#endif
};

DisplayManager::DisplayManager() = default;

DisplayManager::~DisplayManager() {
    reset();
}

Status DisplayManager::initialize(wayland::WaylandRegistry& registry) {
    registry.add_global_listener(
        "wl_output",
        [this, &registry](uint32_t name, std::string_view, uint32_t version) {
            uint32_t bind_version = std::min(version, 4u);
            auto* wl_out = registry.bind<wl_output>(name, &wl_output_interface, bind_version);
            if (!wl_out) {
                LDDE_LOG_ERROR(Display, "Failed to bind wl_output for id " << name);
                return;
            }

            auto handle = std::make_unique<OutputHandle>();
            handle->id = name;
            handle->output.reset(wl_out);
            handle->manager = this;
            handle->pending_info.id = name;
            handle->pending_info.name = "output-" + std::to_string(name);

            OutputHandle* handle_ptr = handle.get();
            outputs_[name] = std::move(handle);

            wl_output_add_listener(wl_out, &output_listener_, handle_ptr);
            LDDE_LOG_DEBUG(Display, "Bound wl_output id " << name << " (v" << bind_version << ")");
        },
        [this](uint32_t name) {
            auto it = outputs_.find(name);
            if (it == outputs_.end()) {
                return;
            }

            DisplayInfo removed_info = it->second->info;
            outputs_.erase(it);
            rebuild_display_list();

            LDDE_LOG_INFO(Display, "Output removed: " << removed_info.name << " (id " << name << ")");
            if (on_removed_) {
                on_removed_(removed_info);
            }
        });

    return Status::ok();
}

void DisplayManager::reset() noexcept {
    outputs_.clear();
    display_list_.clear();
}

void DisplayManager::handle_geometry(OutputHandle* handle, int32_t x, int32_t y,
                                     int32_t physical_width, int32_t physical_height,
                                     int32_t /*subpixel*/, const char* make, const char* model,
                                     int32_t transform) {
    handle->pending_info.geometry.x = x;
    handle->pending_info.geometry.y = y;
    handle->pending_info.physical_width_mm = physical_width;
    handle->pending_info.physical_height_mm = physical_height;
    if (make) handle->pending_info.make = make;
    if (model) handle->pending_info.model = model;
    handle->pending_info.transform = static_cast<DisplayTransform>(transform);
}

void DisplayManager::handle_mode(OutputHandle* handle, uint32_t flags, int32_t width,
                                 int32_t height, int32_t refresh) {
    DisplayMode mode{
        .width = width,
        .height = height,
        .refresh_rate_mhz = refresh,
        .is_current = (flags & WL_OUTPUT_MODE_CURRENT) != 0,
        .is_preferred = (flags & WL_OUTPUT_MODE_PREFERRED) != 0
    };

    handle->pending_info.modes.push_back(mode);

    if (mode.is_current) {
        handle->pending_info.width = width;
        handle->pending_info.height = height;
        handle->pending_info.geometry.width = width;
        handle->pending_info.geometry.height = height;
        handle->pending_info.refresh_rate_mhz = refresh;
    }
}

void DisplayManager::handle_scale(OutputHandle* handle, int32_t factor) {
    handle->pending_info.scale = factor;
}

void DisplayManager::handle_name(OutputHandle* handle, const char* name) {
    if (name) {
        handle->pending_info.name = name;
    }
}

void DisplayManager::handle_description(OutputHandle* handle, const char* description) {
    if (description) {
        handle->pending_info.description = description;
    }
}

void DisplayManager::handle_done(OutputHandle* handle) {
    bool is_new = (handle->info.width == 0 && handle->info.height == 0);
    handle->info = handle->pending_info;
    handle->pending_info.modes.clear();

    rebuild_display_list();

    LDDE_LOG_INFO(Display, "Display " << handle->info.name << " ready: "
                                      << handle->info.width << "x" << handle->info.height
                                      << "@" << (handle->info.refresh_rate_mhz / 1000) << "Hz, scale="
                                      << handle->info.scale << "x ("
                                      << handle->info.make << " " << handle->info.model << ")");

    if (is_new) {
        if (on_added_) {
            on_added_(handle->info);
        }
    } else {
        if (on_changed_) {
            on_changed_(handle->info);
        }
    }
}

void DisplayManager::rebuild_display_list() {
    display_list_.clear();
    display_list_.reserve(outputs_.size());
    for (const auto& [id, handle] : outputs_) {
        display_list_.push_back(handle->info);
    }
}

std::optional<DisplayInfo> DisplayManager::primary_display() const noexcept {
    if (display_list_.empty()) {
        return std::nullopt;
    }
    return display_list_.front();
}

std::optional<DisplayInfo> DisplayManager::find_display_by_id(uint32_t id) const noexcept {
    auto it = outputs_.find(id);
    if (it != outputs_.end()) {
        return it->second->info;
    }
    return std::nullopt;
}

std::optional<DisplayInfo> DisplayManager::find_display_by_name(std::string_view name) const noexcept {
    for (const auto& disp : display_list_) {
        if (disp.name == name) {
            return disp;
        }
    }
    return std::nullopt;
}

} // namespace ldde::display

