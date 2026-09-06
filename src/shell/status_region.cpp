#include "ldde/shell/status_region.hpp"
#include "ldde/shell/cairo_renderer.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::shell {

StatusRegion::StatusRegion() = default;

StatusRegion::~StatusRegion() = default;

void StatusRegion::render(ShmBufferPool& pool) {
    if (!is_created() || geometry_.width <= 0 || geometry_.height <= 0) {
        return;
    }

    auto buffer = pool.acquire_buffer(geometry_.width, geometry_.height);
    if (!buffer) {
        LDDE_LOG_ERROR(Shell, "Failed to acquire buffer for StatusRegion ("
                              << geometry_.width << "x" << geometry_.height << ")");
        return;
    }

    if (render_callback_) {
        render_callback_(*buffer, theme_, tokens_);
    } else {
        CairoRenderer::render_status_region(*buffer, theme_, tokens_, clock_text_);
    }
    commit_buffer(buffer);
}

} // namespace ldde::shell
