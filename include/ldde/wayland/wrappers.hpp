#pragma once

#include <wayland-client.h>
#include <memory>

namespace ldde::wayland {

struct DisplayDeleter {
    void operator()(wl_display* display) const noexcept {
        if (display) {
            wl_display_disconnect(display);
        }
    }
};
using UniqueDisplay = std::unique_ptr<wl_display, DisplayDeleter>;

struct RegistryDeleter {
    void operator()(wl_registry* registry) const noexcept {
        if (registry) {
            wl_registry_destroy(registry);
        }
    }
};
using UniqueRegistry = std::unique_ptr<wl_registry, RegistryDeleter>;

struct CompositorDeleter {
    void operator()(wl_compositor* compositor) const noexcept {
        if (compositor) {
            wl_compositor_destroy(compositor);
        }
    }
};
using UniqueCompositor = std::unique_ptr<wl_compositor, CompositorDeleter>;

struct ShmDeleter {
    void operator()(wl_shm* shm) const noexcept {
        if (shm) {
            wl_shm_destroy(shm);
        }
    }
};
using UniqueShm = std::unique_ptr<wl_shm, ShmDeleter>;

struct OutputDeleter {
    void operator()(wl_output* output) const noexcept {
        if (output) {
#if defined(WL_OUTPUT_RELEASE_SINCE_VERSION)
            if (wl_output_get_version(output) >= WL_OUTPUT_RELEASE_SINCE_VERSION) {
                wl_output_release(output);
                return;
            }
#endif
            wl_output_destroy(output);
        }
    }
};
using UniqueOutput = std::unique_ptr<wl_output, OutputDeleter>;

struct SeatDeleter {
    void operator()(wl_seat* seat) const noexcept {
        if (seat) {
#if defined(WL_SEAT_RELEASE_SINCE_VERSION)
            if (wl_seat_get_version(seat) >= WL_SEAT_RELEASE_SINCE_VERSION) {
                wl_seat_release(seat);
                return;
            }
#endif
            wl_seat_destroy(seat);
        }
    }
};
using UniqueSeat = std::unique_ptr<wl_seat, SeatDeleter>;

struct PointerDeleter {
    void operator()(wl_pointer* pointer) const noexcept {
        if (pointer) {
#if defined(WL_POINTER_RELEASE_SINCE_VERSION)
            if (wl_pointer_get_version(pointer) >= WL_POINTER_RELEASE_SINCE_VERSION) {
                wl_pointer_release(pointer);
                return;
            }
#endif
            wl_pointer_destroy(pointer);
        }
    }
};
using UniquePointer = std::unique_ptr<wl_pointer, PointerDeleter>;

struct KeyboardDeleter {
    void operator()(wl_keyboard* keyboard) const noexcept {
        if (keyboard) {
#if defined(WL_KEYBOARD_RELEASE_SINCE_VERSION)
            if (wl_keyboard_get_version(keyboard) >= WL_KEYBOARD_RELEASE_SINCE_VERSION) {
                wl_keyboard_release(keyboard);
                return;
            }
#endif
            wl_keyboard_destroy(keyboard);
        }
    }
};
using UniqueKeyboard = std::unique_ptr<wl_keyboard, KeyboardDeleter>;

struct TouchDeleter {
    void operator()(wl_touch* touch) const noexcept {
        if (touch) {
#if defined(WL_TOUCH_RELEASE_SINCE_VERSION)
            if (wl_touch_get_version(touch) >= WL_TOUCH_RELEASE_SINCE_VERSION) {
                wl_touch_release(touch);
                return;
            }
#endif
            wl_touch_destroy(touch);
        }
    }
};
using UniqueTouch = std::unique_ptr<wl_touch, TouchDeleter>;

} // namespace ldde::wayland

