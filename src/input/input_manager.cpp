#include "ldde/input/input_manager.hpp"
#include "ldde/core/logging.hpp"

#include <algorithm>

namespace ldde::input {

InputManager::InputManager() = default;

InputManager::~InputManager() {
    reset();
}

Status InputManager::initialize(wayland::WaylandRegistry& registry) {
    registry.add_global_listener(
        "wl_seat",
        [this, &registry](uint32_t name, std::string_view, uint32_t version) {
            uint32_t bind_version = std::min(version, 5u);
            auto* wl_s = registry.bind<wl_seat>(name, &wl_seat_interface, bind_version);
            if (!wl_s) {
                LDDE_LOG_ERROR(Input, "Failed to bind wl_seat for id " << name);
                return;
            }

            auto seat = std::make_unique<Seat>(name, wayland::UniqueSeat(wl_s));
            Seat* seat_ptr = seat.get();
            seats_[name] = std::move(seat);

            LDDE_LOG_DEBUG(Input, "Bound wl_seat id " << name << " (v" << bind_version << ")");
            if (on_added_) {
                on_added_(seat_ptr);
            }
        },
        [this](uint32_t name) {
            auto it = seats_.find(name);
            if (it == seats_.end()) {
                return;
            }

            LDDE_LOG_INFO(Input, "Seat removed: " << it->second->name() << " (id " << name << ")");
            if (on_removed_) {
                on_removed_(it->second.get());
            }
            seats_.erase(it);
        });

    return Status::ok();
}

void InputManager::reset() noexcept {
    seats_.clear();
}

Seat* InputManager::primary_seat() const noexcept {
    if (seats_.empty()) {
        return nullptr;
    }
    return seats_.begin()->second.get();
}

Seat* InputManager::find_seat_by_id(uint32_t id) const noexcept {
    auto it = seats_.find(id);
    if (it != seats_.end()) {
        return it->second.get();
    }
    return nullptr;
}

} // namespace ldde::input

