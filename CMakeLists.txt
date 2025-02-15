# Root CMakeLists.txt
cmake_minimum_required(VERSION 3.14)
project(codebundler VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard and ensure it's required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall 
        -Wextra 
        -Wpedantic 
        -Werror 
        -Wconversion
        -Wsign-conversion
        -Wcast-qual
        -Wformat=2
        -Wundef
        -Werror=float-equal
        -Werror=missing-declarations
    )
elseif(MSVC)
    add_compile_options(
        /W4
        /WX
        /permissive-
        /w14640
        /w14928
    )
endif()

# Enable testing
enable_testing()

# First try to find system-installed GTest
find_package(GTest QUIET)

# If not found, fetch it
if(NOT GTest_FOUND)
    message(STATUS "System-wide Google Test not found. Fetching from repository...")
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.13.0
        SYSTEM
    )
    FetchContent_MakeAvailable(googletest)
else()
    message(STATUS "Using system-wide Google Test")
endif()

# Rest of the file remains the same...
add_library(codebundler_lib INTERFACE)
target_include_directories(codebundler_lib INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

add_executable(codebundler src/main.cpp)
target_link_libraries(codebundler PRIVATE codebundler_lib)

add_subdirectory(tests)

# Installation rules
include(GNUInstallDirs)
install(TARGETS codebundler codebundler_lib
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/codebundler
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Generate and install CMake config files
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/CodeBundlerConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
