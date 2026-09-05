#include <gtest/gtest.h>
#include "ldde/core/lifecycle.hpp"

using namespace ldde::core;

TEST(LifecycleTest, InitialStateIsStarting) {
    LifecycleManager mgr;
    EXPECT_EQ(mgr.state(), LifecycleState::Starting);
    EXPECT_FALSE(mgr.is_ready());
    EXPECT_FALSE(mgr.is_running());
    EXPECT_FALSE(mgr.is_stopped());
    EXPECT_FALSE(mgr.is_failed());
}

TEST(LifecycleTest, NormalProgression) {
    LifecycleManager mgr;

    EXPECT_TRUE(mgr.transition_to(LifecycleState::Initializing).is_ok());
    EXPECT_EQ(mgr.state(), LifecycleState::Initializing);

    EXPECT_TRUE(mgr.transition_to(LifecycleState::ConnectingWayland).is_ok());
    EXPECT_EQ(mgr.state(), LifecycleState::ConnectingWayland);

    EXPECT_TRUE(mgr.transition_to(LifecycleState::InitializingComponents).is_ok());
    EXPECT_EQ(mgr.state(), LifecycleState::InitializingComponents);

    EXPECT_TRUE(mgr.transition_to(LifecycleState::Ready).is_ok());
    EXPECT_EQ(mgr.state(), LifecycleState::Ready);
    EXPECT_TRUE(mgr.is_ready());
    EXPECT_FALSE(mgr.is_running());

    EXPECT_TRUE(mgr.transition_to(LifecycleState::Running).is_ok());
    EXPECT_EQ(mgr.state(), LifecycleState::Running);
    EXPECT_TRUE(mgr.is_ready());
    EXPECT_TRUE(mgr.is_running());

    EXPECT_TRUE(mgr.transition_to(LifecycleState::Stopping).is_ok());
    EXPECT_EQ(mgr.state(), LifecycleState::Stopping);
    EXPECT_FALSE(mgr.is_running());

    EXPECT_TRUE(mgr.transition_to(LifecycleState::Stopped).is_ok());
    EXPECT_EQ(mgr.state(), LifecycleState::Stopped);
    EXPECT_TRUE(mgr.is_stopped());
}

TEST(LifecycleTest, InvalidTransitions) {
    LifecycleManager mgr;

    // Cannot jump from Starting directly to Running
    Status s1 = mgr.transition_to(LifecycleState::Running);
    EXPECT_TRUE(s1.is_error());
    EXPECT_EQ(s1.error().code(), ErrorCode::InvalidLifecycleTransition);

    // Progression to Stopped
    EXPECT_TRUE(mgr.transition_to(LifecycleState::Initializing).is_ok());
    EXPECT_TRUE(mgr.transition_to(LifecycleState::Stopping).is_ok());
    EXPECT_TRUE(mgr.transition_to(LifecycleState::Stopped).is_ok());

    // Once Stopped, cannot transition anywhere
    Status s2 = mgr.transition_to(LifecycleState::Starting);
    EXPECT_TRUE(s2.is_error());
}

TEST(LifecycleTest, FailureTransition) {
    LifecycleManager mgr;
    EXPECT_TRUE(mgr.transition_to(LifecycleState::Initializing).is_ok());
    EXPECT_TRUE(mgr.transition_to(LifecycleState::Failed).is_ok());
    EXPECT_TRUE(mgr.is_failed());

    // Failed can transition to Stopped during cleanup
    EXPECT_TRUE(mgr.transition_to(LifecycleState::Stopped).is_ok());
    EXPECT_TRUE(mgr.is_stopped());
}

TEST(LifecycleTest, ObserverCallbacks) {
    LifecycleManager mgr;
    std::vector<std::pair<LifecycleState, LifecycleState>> transitions;

    mgr.add_observer([&transitions](LifecycleState from, LifecycleState to) {
        transitions.emplace_back(from, to);
    });

    EXPECT_TRUE(mgr.transition_to(LifecycleState::Initializing).is_ok());
    EXPECT_TRUE(mgr.transition_to(LifecycleState::ConnectingWayland).is_ok());

    ASSERT_EQ(transitions.size(), 2u);
    EXPECT_EQ(transitions[0].first, LifecycleState::Starting);
    EXPECT_EQ(transitions[0].second, LifecycleState::Initializing);
    EXPECT_EQ(transitions[1].first, LifecycleState::Initializing);
    EXPECT_EQ(transitions[1].second, LifecycleState::ConnectingWayland);
}

TEST(LifecycleTest, StateNames) {
    EXPECT_EQ(lifecycle_state_name(LifecycleState::Starting), "STARTING");
    EXPECT_EQ(lifecycle_state_name(LifecycleState::Initializing), "INITIALIZING");
    EXPECT_EQ(lifecycle_state_name(LifecycleState::ConnectingWayland), "CONNECTING_WAYLAND");
    EXPECT_EQ(lifecycle_state_name(LifecycleState::InitializingComponents), "INITIALIZING_COMPONENTS");
    EXPECT_EQ(lifecycle_state_name(LifecycleState::Ready), "READY");
    EXPECT_EQ(lifecycle_state_name(LifecycleState::Running), "RUNNING");
    EXPECT_EQ(lifecycle_state_name(LifecycleState::Stopping), "STOPPING");
    EXPECT_EQ(lifecycle_state_name(LifecycleState::Stopped), "STOPPED");
    EXPECT_EQ(lifecycle_state_name(LifecycleState::Failed), "FAILED");
}

