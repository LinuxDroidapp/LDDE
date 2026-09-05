#include <gtest/gtest.h>
#include "ldde/input/seat.hpp"

using namespace ldde::input;

TEST(InputModelTest, DeviceTypeNames) {
    EXPECT_EQ(device_type_name(DeviceType::Pointer), "Pointer");
    EXPECT_EQ(device_type_name(DeviceType::Keyboard), "Keyboard");
    EXPECT_EQ(device_type_name(DeviceType::Touch), "Touch");
}

TEST(InputModelTest, SeatNullConstruction) {
    // Constructing Seat with null wl_seat for headless/unit verification
    Seat seat(1, ldde::wayland::UniqueSeat(nullptr));
    EXPECT_EQ(seat.id(), 1u);
    EXPECT_FALSE(seat.has_pointer());
    EXPECT_FALSE(seat.has_keyboard());
    EXPECT_FALSE(seat.has_touch());
    EXPECT_EQ(seat.wl_ptr(), nullptr);

    seat.handle_name("test-seat-0");
    EXPECT_EQ(seat.name(), "test-seat-0");
}

