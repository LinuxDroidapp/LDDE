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
    unit/test_input_model.cpp
)

add_executable(ldde_unit_tests ${LDDE_UNIT_TEST_SOURCES})
target_link_libraries(ldde_unit_tests PRIVATE
    ldde_core
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME unit_tests COMMAND ldde_unit_tests)

# Integration Tests Executable
add_executable(ldde_integration_tests integration/test_weston_integration.cpp)
target_link_libraries(ldde_integration_tests PRIVATE
    ldde_core
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME integration_weston COMMAND ldde_integration_tests)

