#include "ldde/input/seat.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::input {

std::string_view device_type_name(DeviceType type) noexcept {
    switch (type) {
        case DeviceType::Pointer:  return "Pointer";
        case DeviceType::Keyboard: return "Keyboard";
        case DeviceType::Touch:    return "Touch";
    }
    return "Unknown";
}

// -----------------------------------------------------------------------------
// Pointer
// -----------------------------------------------------------------------------
const wl_pointer_listener Pointer::pointer_listener_ = {
    .enter = [](void*, wl_pointer*, uint32_t, wl_surface*, wl_fixed_t, wl_fixed_t) {},
    .leave = [](void*, wl_pointer*, uint32_t, wl_surface*) {},
    .motion = [](void*, wl_pointer*, uint32_t, wl_fixed_t, wl_fixed_t) {},
    .button = [](void*, wl_pointer*, uint32_t, uint32_t, uint32_t, uint32_t) {},
    .axis = [](void*, wl_pointer*, uint32_t, uint32_t, wl_fixed_t) {},
    .frame = [](void*, wl_pointer*) {},
    .axis_source = [](void*, wl_pointer*, uint32_t) {},
    .axis_stop = [](void*, wl_pointer*, uint32_t, uint32_t) {},
    .axis_discrete = [](void*, wl_pointer*, uint32_t, int32_t) {},
#if defined(WL_POINTER_AXIS_VALUE120_SINCE_VERSION)
    .axis_value120 = [](void*, wl_pointer*, uint32_t, int32_t) {},
#endif
#if defined(WL_POINTER_AXIS_RELATIVE_DIRECTION_SINCE_VERSION)
    .axis_relative_direction = [](void*, wl_pointer*, uint32_t, uint32_t) {}
#endif
};

Pointer::Pointer(wayland::UniquePointer pointer)
    : pointer_(std::move(pointer)) {
    if (pointer_) {
        wl_pointer_add_listener(pointer_.get(), &pointer_listener_, this);
        LDDE_LOG_DEBUG(Input, "Pointer device attached");
    }
}

Pointer::~Pointer() {
    LDDE_LOG_DEBUG(Input, "Pointer device released");
}

// -----------------------------------------------------------------------------
// Keyboard
// -----------------------------------------------------------------------------
const wl_keyboard_listener Keyboard::keyboard_listener_ = {
    .keymap = [](void*, wl_keyboard*, uint32_t, int32_t, uint32_t) {},
    .enter = [](void*, wl_keyboard*, uint32_t, wl_surface*, wl_array*) {},
    .leave = [](void*, wl_keyboard*, uint32_t, wl_surface*) {},
    .key = [](void*, wl_keyboard*, uint32_t, uint32_t, uint32_t, uint32_t) {},
    .modifiers = [](void*, wl_keyboard*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) {},
    .repeat_info = [](void*, wl_keyboard*, int32_t, int32_t) {}
};

Keyboard::Keyboard(wayland::UniqueKeyboard keyboard)
    : keyboard_(std::move(keyboard)) {
    if (keyboard_) {
        wl_keyboard_add_listener(keyboard_.get(), &keyboard_listener_, this);
        LDDE_LOG_DEBUG(Input, "Keyboard device attached");
    }
}

Keyboard::~Keyboard() {
    LDDE_LOG_DEBUG(Input, "Keyboard device released");
}

// -----------------------------------------------------------------------------
// Touch
// -----------------------------------------------------------------------------
const wl_touch_listener Touch::touch_listener_ = {
    .down = [](void*, wl_touch*, uint32_t, uint32_t, wl_surface*, int32_t, wl_fixed_t, wl_fixed_t) {},
    .up = [](void*, wl_touch*, uint32_t, uint32_t, int32_t) {},
    .motion = [](void*, wl_touch*, uint32_t, int32_t, wl_fixed_t, wl_fixed_t) {},
    .frame = [](void*, wl_touch*) {},
    .cancel = [](void*, wl_touch*) {},
#if defined(WL_TOUCH_SHAPE_SINCE_VERSION)
    .shape = [](void*, wl_touch*, int32_t, wl_fixed_t, wl_fixed_t) {},
#endif
#if defined(WL_TOUCH_ORIENTATION_SINCE_VERSION)
    .orientation = [](void*, wl_touch*, int32_t, wl_fixed_t) {}
#endif
};

Touch::Touch(wayland::UniqueTouch touch)
    : touch_(std::move(touch)) {
    if (touch_) {
        wl_touch_add_listener(touch_.get(), &touch_listener_, this);
        LDDE_LOG_DEBUG(Input, "Touch device attached");
    }
}

Touch::~Touch() {
    LDDE_LOG_DEBUG(Input, "Touch device released");
}

// -----------------------------------------------------------------------------
// Seat
// -----------------------------------------------------------------------------
const wl_seat_listener Seat::seat_listener_ = {
    .capabilities = [](void* data, wl_seat*, uint32_t capabilities) {
        static_cast<Seat*>(data)->handle_capabilities(capabilities);
    },
    .name = [](void* data, wl_seat*, const char* name) {
        static_cast<Seat*>(data)->handle_name(name);
    }
};

Seat::Seat(uint32_t id, wayland::UniqueSeat seat)
    : id_(id), seat_(std::move(seat)), name_("seat-" + std::to_string(id)) {
    if (seat_) {
        wl_seat_add_listener(seat_.get(), &seat_listener_, this);
    }
}

Seat::~Seat() {
    pointer_.reset();
    keyboard_.reset();
    touch_.reset();
}

void Seat::handle_capabilities(uint32_t capabilities) {
    capabilities_ = capabilities;

    // Pointer
    if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !pointer_) {
        wl_pointer* ptr = wl_seat_get_pointer(seat_.get());
        if (ptr) {
            pointer_ = std::make_unique<Pointer>(wayland::UniquePointer(ptr));
        }
    } else if (!(capabilities & WL_SEAT_CAPABILITY_POINTER) && pointer_) {
        pointer_.reset();
    }

    // Keyboard
    if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && !keyboard_) {
        wl_keyboard* kbd = wl_seat_get_keyboard(seat_.get());
        if (kbd) {
            keyboard_ = std::make_unique<Keyboard>(wayland::UniqueKeyboard(kbd));
        }
    } else if (!(capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && keyboard_) {
        keyboard_.reset();
    }

    // Touch
    if ((capabilities & WL_SEAT_CAPABILITY_TOUCH) && !touch_) {
        wl_touch* tch = wl_seat_get_touch(seat_.get());
        if (tch) {
            touch_ = std::make_unique<Touch>(wayland::UniqueTouch(tch));
        }
    } else if (!(capabilities & WL_SEAT_CAPABILITY_TOUCH) && touch_) {
        touch_.reset();
    }

    LDDE_LOG_INFO(Input, "Seat " << name_ << " capabilities updated: "
                                 << (has_pointer() ? "[Pointer] " : "")
                                 << (has_keyboard() ? "[Keyboard] " : "")
                                 << (has_touch() ? "[Touch] " : ""));
}

void Seat::handle_name(const char* name) {
    if (name) {
        name_ = name;
        LDDE_LOG_DEBUG(Input, "Seat id " << id_ << " named: " << name_);
    }
}

} // namespace ldde::input
