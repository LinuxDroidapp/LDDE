#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <optional>
#include <wayland-client.h>
#include "ldde/input/seat.hpp"
#include "ldde/wayland/registry.hpp"

namespace ldde::input {

using core::Status;

class InputManager {
public:
    using SeatCallback = std::function<void(Seat* seat)>;

    InputManager();
    ~InputManager();

    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    Status initialize(wayland::WaylandRegistry& registry);
    void reset() noexcept;

    [[nodiscard]] const std::unordered_map<uint32_t, std::unique_ptr<Seat>>& seats() const noexcept {
        return seats_;
    }

    [[nodiscard]] Seat* primary_seat() const noexcept;
    [[nodiscard]] Seat* find_seat_by_id(uint32_t id) const noexcept;

    void on_seat_added(SeatCallback cb) { on_added_ = std::move(cb); }
    void on_seat_removed(SeatCallback cb) { on_removed_ = std::move(cb); }

private:
    std::unordered_map<uint32_t, std::unique_ptr<Seat>> seats_;
    SeatCallback on_added_;
    SeatCallback on_removed_;
};

} // namespace ldde::input
