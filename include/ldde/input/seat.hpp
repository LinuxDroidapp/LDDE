#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <vector>
#include <functional>
#include <wayland-client.h>
#include "ldde/wayland/wrappers.hpp"

namespace ldde::input {

enum class DeviceType {
    Pointer,
    Keyboard,
    Touch
};

[[nodiscard]] std::string_view device_type_name(DeviceType type) noexcept;

class InputDevice {
public:
    virtual ~InputDevice() = default;
    [[nodiscard]] virtual DeviceType type() const noexcept = 0;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
};

class Pointer : public InputDevice {
public:
    explicit Pointer(wayland::UniquePointer pointer);
    ~Pointer() override;

    [[nodiscard]] DeviceType type() const noexcept override { return DeviceType::Pointer; }
    [[nodiscard]] std::string_view name() const noexcept override { return "Pointer"; }
    [[nodiscard]] wl_pointer* wl_ptr() const noexcept { return pointer_.get(); }

private:
    wayland::UniquePointer pointer_;
    static const wl_pointer_listener pointer_listener_;
};

class Keyboard : public InputDevice {
public:
    explicit Keyboard(wayland::UniqueKeyboard keyboard);
    ~Keyboard() override;

    [[nodiscard]] DeviceType type() const noexcept override { return DeviceType::Keyboard; }
    [[nodiscard]] std::string_view name() const noexcept override { return "Keyboard"; }
    [[nodiscard]] wl_keyboard* wl_ptr() const noexcept { return keyboard_.get(); }

private:
    wayland::UniqueKeyboard keyboard_;
    static const wl_keyboard_listener keyboard_listener_;
};

class Touch : public InputDevice {
public:
    explicit Touch(wayland::UniqueTouch touch);
    ~Touch() override;

    [[nodiscard]] DeviceType type() const noexcept override { return DeviceType::Touch; }
    [[nodiscard]] std::string_view name() const noexcept override { return "Touch"; }
    [[nodiscard]] wl_touch* wl_ptr() const noexcept { return touch_.get(); }

private:
    wayland::UniqueTouch touch_;
    static const wl_touch_listener touch_listener_;
};

class Seat {
public:
    Seat(uint32_t id, wayland::UniqueSeat seat);
    ~Seat();

    Seat(const Seat&) = delete;
    Seat& operator=(const Seat&) = delete;

    [[nodiscard]] uint32_t id() const noexcept { return id_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] wl_seat* wl_ptr() const noexcept { return seat_.get(); }

    [[nodiscard]] bool has_pointer() const noexcept { return pointer_ != nullptr; }
    [[nodiscard]] bool has_keyboard() const noexcept { return keyboard_ != nullptr; }
    [[nodiscard]] bool has_touch() const noexcept { return touch_ != nullptr; }

    [[nodiscard]] Pointer* pointer() const noexcept { return pointer_.get(); }
    [[nodiscard]] Keyboard* keyboard() const noexcept { return keyboard_.get(); }
    [[nodiscard]] Touch* touch() const noexcept { return touch_.get(); }

    void handle_capabilities(uint32_t capabilities);
    void handle_name(const char* name);

private:
    uint32_t id_ = 0;
    wayland::UniqueSeat seat_;
    std::string name_;
    uint32_t capabilities_ = 0;

    std::unique_ptr<Pointer> pointer_;
    std::unique_ptr<Keyboard> keyboard_;
    std::unique_ptr<Touch> touch_;

    static const wl_seat_listener seat_listener_;
};

} // namespace ldde::input

