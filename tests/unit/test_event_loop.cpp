#include <gtest/gtest.h>
#include "ldde/core/event_loop.hpp"
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <thread>

using namespace ldde::core;

TEST(EventLoopTest, InitializeAndDispatch) {
    EventLoop loop;
    ASSERT_TRUE(loop.initialize().is_ok());

    // Dispatch with immediate timeout (0ms)
    EXPECT_TRUE(loop.dispatch(0).is_ok());
}

TEST(EventLoopTest, PostAndWakeup) {
    EventLoop loop;
    ASSERT_TRUE(loop.initialize().is_ok());

    std::atomic<bool> called = false;
    loop.post([&called]() {
        called = true;
    });

    EXPECT_TRUE(loop.dispatch(50).is_ok());
    EXPECT_TRUE(called.load());
}

TEST(EventLoopTest, TimerFiring) {
    EventLoop loop;
    ASSERT_TRUE(loop.initialize().is_ok());

    std::atomic<int> counter = 0;
    TimerId tid = loop.add_timer(std::chrono::milliseconds(20), false, [&counter]() {
        counter++;
    });
    EXPECT_GT(tid, 0u);

    // Initial dispatch immediately should not fire timer yet
    loop.dispatch(0);
    EXPECT_EQ(counter.load(), 0);

    // Sleep 30ms and dispatch
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    loop.dispatch(50);
    EXPECT_EQ(counter.load(), 1);
}

TEST(EventLoopTest, FdWatcherWithPipe) {
    EventLoop loop;
    ASSERT_TRUE(loop.initialize().is_ok());

    int pipefds[2];
    ASSERT_EQ(pipe(pipefds), 0);

    std::atomic<bool> read_called = false;
    char read_buf[16] = {0};

    Status s = loop.add_fd(pipefds[0], FdEvent::Readable, [&read_called, &read_buf](int fd, FdEvent) {
        ssize_t n = read(fd, read_buf, sizeof(read_buf));
        if (n > 0) {
            read_called = true;
        }
    });
    ASSERT_TRUE(s.is_ok());

    // Write to pipe
    const char msg[] = "test";
    write(pipefds[1], msg, sizeof(msg));

    loop.dispatch(50);
    EXPECT_TRUE(read_called.load());
    EXPECT_STREQ(read_buf, "test");

    loop.remove_fd(pipefds[0]);
    close(pipefds[0]);
    close(pipefds[1]);
}

