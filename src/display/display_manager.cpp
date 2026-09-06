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

void DisplayManager::load_config(const config::Config& config) {
    auto preferred = config.get_string("display", "primary");
    if (preferred.has_value()) {
        preferred_primary_name_ = preferred.value();
    }

    auto layout_str = config.get_string("display", "layout_class");
    if (layout_str.has_value()) {
        config_layout_class_ = parse_layout_class(layout_str.value());
    }

    auto scale_opt = config.get_int("display", "scale");
    if (scale_opt.has_value() && *scale_opt > 0) {
        config_scale_ = static_cast<int32_t>(*scale_opt);
    }

    auto top = config.get_int("display", "safe_area.top");
    auto right = config.get_int("display", "safe_area.right");
    auto bottom = config.get_int("display", "safe_area.bottom");
    auto left = config.get_int("display", "safe_area.left");
    if (top.has_value() || right.has_value() || bottom.has_value() || left.has_value()) {
        config_safe_insets_ = SafeInsets{
            .left = static_cast<int32_t>(left.value_or(0)),
            .top = static_cast<int32_t>(top.value_or(0)),
            .right = static_cast<int32_t>(right.value_or(0)),
            .bottom = static_cast<int32_t>(bottom.value_or(0))
        };
    }
}

Status DisplayManager::initialize(wayland::WaylandRegistry& registry, const config::Config* config) {
    if (config) {
        load_config(*config);
    }

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
            handle->policy = std::make_unique<DisplayPolicy>();

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
            update_primary_display();

            LDDE_LOG_INFO(Display, "Output removed: " << removed_info.name << " (id " << name << ")");
            emit_event(DisplayEventType::DisplayRemoved, name, removed_info);
            if (on_removed_) {
                on_removed_(removed_info);
            }
        });

    return Status::ok();
}

void DisplayManager::reset() noexcept {
    outputs_.clear();
    display_list_.clear();
    primary_id_.reset();
}

void DisplayManager::handle_geometry(OutputHandle* handle, int32_t x, int32_t y,
                                     int32_t physical_width, int32_t physical_height,
                                     int32_t /*subpixel*/, const char* make, const char* model,
                                     int32_t transform) {
    handle->pending_info.logical_x = x;
    handle->pending_info.logical_y = y;
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
        handle->pending_info.pixel_width = width;
        handle->pending_info.pixel_height = height;
        handle->pending_info.refresh_rate_mhz = refresh;
    }
}

void DisplayManager::handle_scale(OutputHandle* handle, int32_t factor) {
    if (config_scale_.has_value()) {
        handle->pending_info.scale = config_scale_.value();
    } else {
        handle->pending_info.scale = factor > 0 ? factor : 1;
    }
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
    bool is_new = (handle->info.pixel_width == 0 && handle->info.pixel_height == 0 && handle->info.width == 0);

    // 1. Calculate transformed physical pixels
    int32_t trans_w = handle->pending_info.pixel_width;
    int32_t trans_h = handle->pending_info.pixel_height;
    apply_transform_to_dimensions(handle->pending_info.transform,
                                  handle->pending_info.pixel_width,
                                  handle->pending_info.pixel_height,
                                  trans_w, trans_h);

    // 2. Derive logical coordinates from transformed pixels and scale
    int32_t eff_scale = handle->pending_info.scale > 0 ? handle->pending_info.scale : 1;
    int32_t log_w = physical_to_logical(trans_w, eff_scale);
    int32_t log_h = physical_to_logical(trans_h, eff_scale);

    handle->pending_info.logical_width = log_w;
    handle->pending_info.logical_height = log_h;
    handle->pending_info.width = log_w;
    handle->pending_info.height = log_h;
    handle->pending_info.geometry.width = log_w;
    handle->pending_info.geometry.height = log_h;

    // 3. Derive Orientation
    handle->pending_info.orientation = derive_orientation(handle->pending_info.transform, log_w, log_h);

    // 4. Safe Area configuration
    if (config_safe_insets_.has_value()) {
        handle->pending_info.safe_insets = config_safe_insets_->rotated_for(handle->pending_info.transform);
    }

    // 5. Detect granular changes
    bool geom_changed = (handle->info.logical_width != log_w ||
                         handle->info.logical_height != log_h ||
                         handle->info.logical_x != handle->pending_info.logical_x ||
                         handle->info.logical_y != handle->pending_info.logical_y);
    bool scale_changed = (handle->info.scale != handle->pending_info.scale);
    bool orient_changed = (handle->info.orientation != handle->pending_info.orientation);
    bool safe_changed = !(handle->info.safe_insets == handle->pending_info.safe_insets);

    // 6. Update snapshot
    handle->info = handle->pending_info;
    handle->pending_info.modes.clear();

    // 7. Update DisplayPolicy
    if (!handle->policy) {
        handle->policy = std::make_unique<DisplayPolicy>(handle->info);
    } else {
        handle->policy->update_display(handle->info);
    }
    if (config_layout_class_.has_value()) {
        handle->policy->set_layout_class_override(config_layout_class_);
    }

    // 8. Sync available geometry
    handle->info.available_geometry = handle->policy->available_geometry();

    rebuild_display_list();
    update_primary_display();

    LDDE_LOG_INFO(Display, "Display " << handle->info.name << " ready: "
                                      << handle->info.pixel_width << "x" << handle->info.pixel_height
                                      << " physical, " << log_w << "x" << log_h << " logical ("
                                      << orientation_name(handle->info.orientation) << "), scale="
                                      << handle->info.scale << "x ("
                                      << handle->info.make << " " << handle->info.model << ")");

    // 9. Dispatch structured events
    if (is_new) {
        emit_event(DisplayEventType::DisplayAdded, handle->id, handle->info);
        if (on_added_) {
            on_added_(handle->info);
        }
    } else {
        if (orient_changed) emit_event(DisplayEventType::OrientationChanged, handle->id, handle->info);
        if (scale_changed) emit_event(DisplayEventType::ScaleChanged, handle->id, handle->info);
        if (geom_changed) emit_event(DisplayEventType::GeometryChanged, handle->id, handle->info);
        if (safe_changed) emit_event(DisplayEventType::SafeAreaChanged, handle->id, handle->info);

        emit_event(DisplayEventType::DisplayChanged, handle->id, handle->info);
        if (on_changed_) {
            on_changed_(handle->info);
        }
    }
}

void DisplayManager::register_synthetic_display(const DisplayInfo& info) {
    auto handle = std::make_unique<OutputHandle>();
    handle->id = info.id;
    handle->manager = this;
    handle->info = info;
    handle->policy = std::make_unique<DisplayPolicy>(info);
    if (config_layout_class_.has_value()) {
        handle->policy->set_layout_class_override(config_layout_class_);
    }
    handle->info.available_geometry = handle->policy->available_geometry();

    outputs_[info.id] = std::move(handle);
    rebuild_display_list();
    update_primary_display();

    emit_event(DisplayEventType::DisplayAdded, info.id, info);
    if (on_added_) {
        on_added_(info);
    }
}

void DisplayManager::remove_synthetic_display(DisplayId id) {
    auto it = outputs_.find(id);
    if (it == outputs_.end()) return;

    DisplayInfo removed_info = it->second->info;
    outputs_.erase(it);
    rebuild_display_list();
    update_primary_display();

    emit_event(DisplayEventType::DisplayRemoved, id, removed_info);
    if (on_removed_) {
        on_removed_(removed_info);
    }
}

void DisplayManager::rebuild_display_list() {
    display_list_.clear();
    display_list_.reserve(outputs_.size());
    for (const auto& [id, handle] : outputs_) {
        display_list_.push_back(handle->info);
    }
}

void DisplayManager::update_primary_display() {
    if (outputs_.empty()) {
        primary_id_.reset();
        return;
    }

    // 1. If preferred primary output name is configured, find it
    if (!preferred_primary_name_.empty()) {
        for (const auto& [id, handle] : outputs_) {
            if (handle->info.name == preferred_primary_name_) {
                primary_id_ = id;
                return;
            }
        }
    }

    // 2. If existing primary is still valid, retain it
    if (primary_id_.has_value() && outputs_.find(*primary_id_) != outputs_.end()) {
        return;
    }

    // 3. Fall back to first available output
    primary_id_ = outputs_.begin()->first;
}

std::optional<DisplayInfo> DisplayManager::primary_display() const noexcept {
    if (!primary_id_.has_value()) {
        return std::nullopt;
    }
    return find_display_by_id(*primary_id_);
}

std::optional<DisplayId> DisplayManager::primary_display_id() const noexcept {
    return primary_id_;
}

const DisplayPolicy* DisplayManager::primary_policy() const noexcept {
    if (!primary_id_.has_value()) {
        return nullptr;
    }
    return find_policy_by_id(*primary_id_);
}

DisplayPolicy* DisplayManager::primary_policy() noexcept {
    if (!primary_id_.has_value()) {
        return nullptr;
    }
    return find_policy_by_id(*primary_id_);
}

std::optional<DisplayInfo> DisplayManager::find_display_by_id(DisplayId id) const noexcept {
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

const DisplayPolicy* DisplayManager::find_policy_by_id(DisplayId id) const noexcept {
    auto it = outputs_.find(id);
    if (it != outputs_.end()) {
        return it->second->policy.get();
    }
    return nullptr;
}

DisplayPolicy* DisplayManager::find_policy_by_id(DisplayId id) noexcept {
    auto it = outputs_.find(id);
    if (it != outputs_.end()) {
        return it->second->policy.get();
    }
    return nullptr;
}

void DisplayManager::emit_event(DisplayEventType type, DisplayId id, const DisplayInfo& info) {
    if (on_event_) {
        on_event_(DisplayEvent{.type = type, .display_id = id}, info);
    }
}

} // namespace ldde::display
