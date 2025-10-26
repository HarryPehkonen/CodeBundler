cmake_minimum_required(VERSION 3.14)
project(CodeBundler LANGUAGES CXX)

# --- Standard C++ Settings ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Options ---
option(CODEBUNDLER_ENABLE_FORMATTING "Format code with clang-format target" ON)
option(CODEBUNDLER_ENABLE_TESTING "Build unit tests" ON)
option(CODEBUNDLER_INSTALL "Enable installation" ON)

# --- Include subdirectories ---
add_subdirectory(external) # Process dependencies first
add_subdirectory(src)

# --- Testing Setup (Requires GoogleTest) ---
if(CODEBUNDLER_ENABLE_TESTING)
    if(TARGET GTest::gtest AND TARGET GTest::gmock)
        enable_testing()
        add_subdirectory(tests)
        message(STATUS "Testing enabled.")
    else()
        message(WARNING "GoogleTest target not found. Disabling tests. Make sure external/CMakeLists.txt ran correctly.")
        set(CODEBUNDLER_ENABLE_TESTING OFF) # Ensure test subdirectory isn't processed if GTest failed
    endif()
else()
     message(STATUS "Testing disabled by option.")
endif()

# --- Formatting Target (Optional) ---
if(CODEBUNDLER_ENABLE_FORMATTING)
    find_program(CLANG_FORMAT_EXE NAMES clang-format)
    if(CLANG_FORMAT_EXE)
        file(GLOB_RECURSE ALL_CXX_SOURCES
             "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
             "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
             "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp" # Only if testing is enabled
             "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.hpp" # Only if testing is enabled
        )
        if(ALL_CXX_SOURCES)
             add_custom_target(format COMMAND ${CLANG_FORMAT_EXE} -i ${ALL_CXX_SOURCES}
                               COMMENT "Formatting code with clang-format")
             message(STATUS "Added 'format' target using ${CLANG_FORMAT_EXE}")
        else()
             message(WARNING "No source files found for formatting.")
        endif()

    else()
        message(WARNING "clang-format executable not found. 'format' target disabled.")
    endif()
endif()

# --- Installation ---
if(CODEBUNDLER_INSTALL)
    include(GNUInstallDirs)
    install(TARGETS codebundler
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    message(STATUS "Installation enabled to prefix: ${CMAKE_INSTALL_PREFIX}")
else()
    message(STATUS "Installation disabled.")
endif()

message(STATUS "CodeBundler project configuration complete.")
message(STATUS "  Build tests: ${CODEBUNDLER_ENABLE_TESTING}")
message(STATUS "  Enable formatting target: ${CODEBUNDLER_ENABLE_FORMATTING}")
message(STATUS "  Enable install target: ${CODEBUNDLER_INSTALL}")
