#include <gtest/gtest.h>
#include "ldde/core/readiness.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cstdlib>
#include <fstream>

using namespace ldde::core;

TEST(ReadinessTest, ReadyFdSignaling) {
    int fds[2];
    ASSERT_EQ(pipe(fds), 0);

    ReadinessManager rm;
    rm.set_ready_fd(fds[1]);

    EXPECT_FALSE(rm.is_ready_reported());
    Status s = rm.report_ready();
    EXPECT_TRUE(s.is_ok());
    EXPECT_TRUE(rm.is_ready_reported());

    char buf[8] = {0};
    ssize_t n = read(fds[0], buf, sizeof(buf));
    EXPECT_EQ(n, 1);
    EXPECT_EQ(buf[0], '\n');

    close(fds[0]);
}

TEST(ReadinessTest, NotifySocketSignaling) {
    std::string sock_path = "/tmp/ldde_test_notify.sock";
    unlink(sock_path.c_str());

    int sfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    ASSERT_GE(sfd, 0);

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);
    ASSERT_EQ(bind(sfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);

    ReadinessManager rm;
    rm.set_notify_socket(sock_path);

    Status s = rm.report_ready();
    EXPECT_TRUE(s.is_ok());

    char buf[32] = {0};
    ssize_t n = recv(sfd, buf, sizeof(buf) - 1, 0);
    EXPECT_GT(n, 0);
    EXPECT_EQ(std::string(buf), "READY=1\n");

    close(sfd);
    unlink(sock_path.c_str());
}

TEST(ReadinessTest, ReadinessFileSignaling) {
    std::string file_path = "/tmp/ldde_test_readiness.ready";
    unlink(file_path.c_str());

    ReadinessManager rm;
    rm.set_readiness_file(file_path);

    EXPECT_FALSE(rm.is_ready_reported());
    Status s = rm.report_ready();
    EXPECT_TRUE(s.is_ok());
    EXPECT_TRUE(rm.is_ready_reported());

    std::ifstream ifs(file_path);
    ASSERT_TRUE(ifs.is_open());
    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("STATUS=READY\n"), std::string::npos);
    EXPECT_NE(content.find("VERSION=1\n"), std::string::npos);
    EXPECT_NE(content.find("PID="), std::string::npos);

    unlink(file_path.c_str());
}

TEST(ReadinessTest, EnvironmentDetection) {
    setenv("LDDE_READY_FD", "17", 1);
    setenv("NOTIFY_SOCKET", "/tmp/mock_notify.sock", 1);
    setenv("LDDE_READINESS_FILE", "/tmp/mock_ready.file", 1);

    ReadinessManager rm;
    rm.detect_environment();

    EXPECT_EQ(rm.ready_fd(), 17);
    EXPECT_EQ(rm.notify_socket(), "/tmp/mock_notify.sock");
    EXPECT_EQ(rm.readiness_file(), "/tmp/mock_ready.file");

    unsetenv("LDDE_READY_FD");
    unsetenv("NOTIFY_SOCKET");
    unsetenv("LDDE_READINESS_FILE");
}

