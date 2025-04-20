# Only proceed if testing is enabled and GoogleTest is found
if(NOT CODEBUNDLER_ENABLE_TESTING)
    message(STATUS "Skipping test configuration as testing is disabled.")
    return()
endif()

if(NOT TARGET GTest::gtest)
    message(WARNING "GoogleTest target not found. Cannot configure tests.")
    return()
endif()

message(STATUS "Configuring CodeBundler tests...")

# Add test executable
add_executable(codebundler_tests
    test_bundler.cpp      # Add test files here
    test_unbundler.cpp
    # Include implementations needed for tests, or link the main library if built as one
    ../src/bundler.cpp
    ../src/unbundler.cpp
    ../src/bundleparser.cpp
    ../src/utilities.cpp
)

# Link GoogleTest and necessary project libraries/dependencies
target_link_libraries(codebundler_tests PRIVATE
    GTest::gtest
    GTest::gmock # Usually link both
    GTest::gtest_main
    PicoSHA2::PicoSHA2
)

# Add include directories for project headers
target_include_directories(codebundler_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include # To find bundler.hpp, unbundler.hpp etc.
)
# PicoSHA2 include dir should be handled by its target

# Discover and add tests to CTest
include(GoogleTest)
gtest_discover_tests(codebundler_tests)

message(STATUS "Tests configured.")
