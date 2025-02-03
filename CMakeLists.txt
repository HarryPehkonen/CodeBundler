## Root CMakeLists.txt
cmake_minimum_required(VERSION 3.14)
project(codebundler VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
elseif(MSVC)
    add_compile_options(/W4)
endif()

# Enable testing
enable_testing()

# Find GTest package
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

# Create library target
add_library(codebundler_lib INTERFACE)
target_include_directories(codebundler_lib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Create executable target
add_executable(codebundler src/main.cpp)
target_link_libraries(codebundler PRIVATE codebundler_lib)

# Add tests directory
add_subdirectory(tests)
