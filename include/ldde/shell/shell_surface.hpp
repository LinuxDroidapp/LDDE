#pragma once

#include <wayland-client.h>
#include <memory>
#include "ldde/core/types.hpp"
#include "ldde/core/error.hpp"
#include "ldde/shell/types.hpp"
#include "ldde/shell/shm_buffer.hpp"

namespace ldde::shell {

using core::Status;

class ShellSurface {
public:
    ShellSurface();
    virtual ~ShellSurface();

    ShellSurface(const ShellSurface&) = delete;
    ShellSurface& operator=(const ShellSurface&) = delete;

    Status create(wl_compositor* compositor,
                  wl_subcompositor* subcompositor,
                  wl_surface* parent_surface,
                  const core::Rect& geometry,
                  ShellLayer layer);

    virtual void destroy() noexcept;

    Status update_geometry(const core::Rect& new_geometry);
    Status commit_buffer(std::shared_ptr<ShmBuffer> buffer);

    [[nodiscard]] wl_surface* surface() const noexcept { return surface_; }
    [[nodiscard]] wl_subsurface* subsurface() const noexcept { return subsurface_; }
    [[nodiscard]] const core::Rect& geometry() const noexcept { return geometry_; }
    [[nodiscard]] ShellLayer layer() const noexcept { return layer_; }
    [[nodiscard]] bool is_created() const noexcept { return surface_ != nullptr; }

    virtual void render(ShmBufferPool& pool) = 0;

protected:
    wl_surface* surface_ = nullptr;
    wl_subsurface* subsurface_ = nullptr;
    core::Rect geometry_;
    ShellLayer layer_ = ShellLayer::Background;
    std::shared_ptr<ShmBuffer> current_buffer_;
};

} // namespace ldde::shell
