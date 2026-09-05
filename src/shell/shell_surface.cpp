#include "ldde/shell/shell_surface.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::shell {

ShellSurface::ShellSurface() = default;

ShellSurface::~ShellSurface() {
    destroy();
}

Status ShellSurface::create(wl_compositor* compositor,
                            wl_subcompositor* subcompositor,
                            wl_surface* parent_surface,
                            const core::Rect& geometry,
                            ShellLayer layer) {
    if (!compositor) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Wayland,
                                 core::ErrorCode::WaylandProtocolError,
                                 "Cannot create ShellSurface with null compositor");
    }

    destroy();

    geometry_ = geometry;
    layer_ = layer;

    surface_ = wl_compositor_create_surface(compositor);
    if (!surface_) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Wayland,
                                 core::ErrorCode::WaylandProtocolError,
                                 "wl_compositor_create_surface failed");
    }

    if (parent_surface && subcompositor) {
        subsurface_ = wl_subcompositor_get_subsurface(subcompositor, surface_, parent_surface);
        if (!subsurface_) {
            LDDE_LOG_WARN(Shell, "Failed to create wl_subsurface; surface will run standalone");
        } else {
            wl_subsurface_set_position(subsurface_, geometry_.x, geometry_.y);
            wl_subsurface_set_desync(subsurface_);
        }
    }

    return Status::ok();
}

void ShellSurface::destroy() noexcept {
    if (subsurface_) {
        wl_subsurface_destroy(subsurface_);
        subsurface_ = nullptr;
    }
    if (surface_) {
        wl_surface_destroy(surface_);
        surface_ = nullptr;
    }
    current_buffer_.reset();
}

Status ShellSurface::update_geometry(const core::Rect& new_geometry) {
    geometry_ = new_geometry;
    if (subsurface_) {
        wl_subsurface_set_position(subsurface_, geometry_.x, geometry_.y);
    }
    return Status::ok();
}

Status ShellSurface::commit_buffer(std::shared_ptr<ShmBuffer> buffer) {
    if (!surface_) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Wayland,
                                 core::ErrorCode::WaylandProtocolError,
                                 "Cannot commit buffer to null surface");
    }
    if (!buffer || !buffer->wl_buf()) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Internal,
                                 core::ErrorCode::InvalidArgument,
                                 "Invalid shm buffer for commit");
    }

    current_buffer_ = buffer;
    buffer->set_busy(true);

    wl_surface_attach(surface_, buffer->wl_buf(), 0, 0);
    wl_surface_damage(surface_, 0, 0, geometry_.width, geometry_.height);
    wl_surface_commit(surface_);

    return Status::ok();
}

} // namespace ldde::shell
