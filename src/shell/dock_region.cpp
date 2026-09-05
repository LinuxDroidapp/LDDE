#include "ldde/shell/dock_region.hpp"
#include "ldde/shell/cairo_renderer.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::shell {

DockRegion::DockRegion() = default;

DockRegion::~DockRegion() = default;

void DockRegion::render(ShmBufferPool& pool) {
    if (!is_created() || geometry_.width <= 0 || geometry_.height <= 0) {
        return;
    }

    auto buffer = pool.acquire_buffer(geometry_.width, geometry_.height);
    if (!buffer) {
        LDDE_LOG_ERROR(Shell, "Failed to acquire buffer for DockRegion ("
                              << geometry_.width << "x" << geometry_.height << ")");
        return;
    }

    CairoRenderer::render_dock_region(*buffer, theme_, tokens_, slot_count_);
    commit_buffer(buffer);
}

} // namespace ldde::shell
