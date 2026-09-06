find_package(GTest REQUIRED)

# Unit Tests Executable
set(LDDE_UNIT_TEST_SOURCES
    unit/test_lifecycle.cpp
    unit/test_error.cpp
    unit/test_config.cpp
    unit/test_logging.cpp
    unit/test_event_loop.cpp
    unit/test_readiness.cpp
    unit/test_display_model.cpp
    unit/test_display_policy.cpp
    unit/test_input_model.cpp
    unit/test_shell_layout.cpp
    unit/test_shell_tokens.cpp
    unit/test_shell_lifecycle.cpp
    unit/test_window_model.cpp
    unit/test_window_registry.cpp
    unit/test_window_events.cpp
    unit/test_transient_windows.cpp
    unit/test_window_placement.cpp
    unit/test_window_state_controller.cpp
    unit/test_window_focus.cpp
    unit/test_window_stacking.cpp
    unit/test_window_interaction.cpp
    unit/test_window_controls.cpp
    unit/test_window_manager.cpp
    unit/test_touch_interaction.cpp
    unit/test_application_discovery.cpp
    unit/test_launcher.cpp
    unit/test_dock.cpp
    unit/test_switcher.cpp
)

add_executable(ldde_unit_tests ${LDDE_UNIT_TEST_SOURCES})
target_link_libraries(ldde_unit_tests PRIVATE
    ldde_core
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME unit_tests COMMAND ldde_unit_tests)

# Integration Tests Executable
add_executable(ldde_integration_tests
    integration/test_weston_integration.cpp
    integration/test_application_integration.cpp
    integration/test_launcher_integration.cpp
    integration/test_dock_integration.cpp
    integration/test_switcher_integration.cpp
)
target_link_libraries(ldde_integration_tests PRIVATE
    ldde_core
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME integration_weston COMMAND ldde_integration_tests)

