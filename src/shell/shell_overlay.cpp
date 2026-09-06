#include "ldde/shell/shell_overlay.hpp"
#include "ldde/shell/cairo_renderer.hpp"
#include "ldde/core/logging.hpp"
#include <cairo.h>
#include <algorithm>

namespace ldde::shell {

ShellOverlay::ShellOverlay() = default;

ShellOverlay::~ShellOverlay() = default;

void ShellOverlay::render(ShmBufferPool& pool) {
    if (!is_created() || geometry_.width <= 0 || geometry_.height <= 0) {
        return;
    }

    auto buffer = pool.acquire_buffer(geometry_.width, geometry_.height);
    if (!buffer) {
        LDDE_LOG_ERROR(Shell, "Failed to acquire buffer for ShellOverlay ("
                              << geometry_.width << "x" << geometry_.height << ")");
        return;
    }

    if (is_active_) {
        if (render_callback_) {
            render_callback_(*buffer, theme_);
        } else {
            int modal_w = std::min(geometry_.width - 64, 480);
            int modal_h = std::min(geometry_.height - 64, 320);
            int modal_x = (geometry_.width - modal_w) / 2;
            int modal_y = (geometry_.height - modal_h) / 2;
            core::Rect modal_rect{modal_x, modal_y, modal_w, modal_h};
            CairoRenderer::render_overlay(*buffer, theme_, modal_rect);
        }
    } else {
        if (buffer->data()) {
            cairo_surface_t* surface = cairo_image_surface_create_for_data(
                static_cast<unsigned char*>(buffer->data()),
                CAIRO_FORMAT_ARGB32,
                buffer->width(),
                buffer->height(),
                buffer->stride());
            cairo_t* cr = cairo_create(surface);
            cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
            cairo_paint(cr);
            cairo_surface_flush(surface);
            cairo_destroy(cr);
            cairo_surface_destroy(surface);
        }
    }
    commit_buffer(buffer);
}

} // namespace ldde::shell
