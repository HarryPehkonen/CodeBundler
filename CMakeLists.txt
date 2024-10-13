cmake_minimum_required(VERSION 3.10)
project(CodeBundler VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add compiler warnings
if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Source files (excluding main.cpp)
set(LIB_SOURCES
    src/file_matcher.cpp
    src/file_processor.cpp
    src/file_combiner.cpp
    src/file_extractor.cpp
    src/utils.cpp
)

# Header files
set(HEADERS
    src/file_matcher.hpp
    src/file_processor.hpp
    src/file_combiner.hpp
    src/file_extractor.hpp
    src/utils.hpp
    src/constants.hpp
    src/types.hpp
)

# Create a static library with the common code
add_library(code_bundler_lib STATIC ${LIB_SOURCES} ${HEADERS})

# Main executable
add_executable(code_bundler src/main.cpp)
target_link_libraries(code_bundler PRIVATE code_bundler_lib)

# Testing
enable_testing()
find_package(GTest REQUIRED)
include(GoogleTest)

# Test executable
set(TEST_SOURCES
    tests/file_matcher_test.cpp
    tests/file_processor_test.cpp
    tests/file_combiner_test.cpp
    tests/file_extractor_test.cpp
    tests/utils_test.cpp
)

add_executable(run_tests ${TEST_SOURCES})
target_link_libraries(run_tests PRIVATE code_bundler_lib GTest::GTest GTest::Main)
gtest_discover_tests(run_tests)
