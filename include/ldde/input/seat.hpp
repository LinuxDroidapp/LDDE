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

struct TouchDownEvent {
    uint32_t serial = 0;
    uint32_t time_ms = 0;
    wl_surface* surface = nullptr;
    int32_t id = 0;
    double x = 0.0;
    double y = 0.0;
};

struct TouchUpEvent {
    uint32_t serial = 0;
    uint32_t time_ms = 0;
    int32_t id = 0;
};

struct TouchMotionEvent {
    uint32_t time_ms = 0;
    int32_t id = 0;
    double x = 0.0;
    double y = 0.0;
};

class Touch : public InputDevice {
public:
    using TouchDownCallback = std::function<void(const TouchDownEvent&)>;
    using TouchUpCallback = std::function<void(const TouchUpEvent&)>;
    using TouchMotionCallback = std::function<void(const TouchMotionEvent&)>;
    using TouchFrameCallback = std::function<void()>;
    using TouchCancelCallback = std::function<void()>;

    explicit Touch(wayland::UniqueTouch touch);
    ~Touch() override;

    [[nodiscard]] DeviceType type() const noexcept override { return DeviceType::Touch; }
    [[nodiscard]] std::string_view name() const noexcept override { return "Touch"; }
    [[nodiscard]] wl_touch* wl_ptr() const noexcept { return touch_.get(); }

    void on_down(TouchDownCallback cb) { on_down_ = std::move(cb); }
    void on_up(TouchUpCallback cb) { on_up_ = std::move(cb); }
    void on_motion(TouchMotionCallback cb) { on_motion_ = std::move(cb); }
    void on_frame(TouchFrameCallback cb) { on_frame_ = std::move(cb); }
    void on_cancel(TouchCancelCallback cb) { on_cancel_ = std::move(cb); }

    void handle_down(uint32_t serial, uint32_t time_ms, wl_surface* surface, int32_t id, double x, double y);
    void handle_up(uint32_t serial, uint32_t time_ms, int32_t id);
    void handle_motion(uint32_t time_ms, int32_t id, double x, double y);
    void handle_frame();
    void handle_cancel();

private:
    wayland::UniqueTouch touch_;
    TouchDownCallback on_down_;
    TouchUpCallback on_up_;
    TouchMotionCallback on_motion_;
    TouchFrameCallback on_frame_;
    TouchCancelCallback on_cancel_;
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

