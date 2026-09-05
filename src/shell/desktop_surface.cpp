#include "ldde/shell/desktop_surface.hpp"
#include "ldde/shell/cairo_renderer.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::shell {

DesktopSurface::DesktopSurface() = default;

DesktopSurface::~DesktopSurface() = default;

void DesktopSurface::render(ShmBufferPool& pool) {
    if (!is_created() || geometry_.width <= 0 || geometry_.height <= 0) {
        return;
    }

    auto buffer = pool.acquire_buffer(geometry_.width, geometry_.height);
    if (!buffer) {
        LDDE_LOG_ERROR(Shell, "Failed to acquire buffer for DesktopSurface ("
                              << geometry_.width << "x" << geometry_.height << ")");
        return;
    }

    CairoRenderer::render_desktop(*buffer, theme_);
    commit_buffer(buffer);
}

} // namespace ldde::shell
