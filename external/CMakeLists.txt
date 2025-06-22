include(FetchContent)

# --- PicoSHA2 ---
# Option 1: Use FetchContent directly if CMake > 3.11 (which we require anyway)
FetchContent_Declare(
    PicoSHA2
    GIT_REPOSITORY https://github.com/okdshin/PicoSHA2.git
    GIT_TAG        master # Or a specific commit/tag for stability e.g., 2c6a4a5a84f440b5ac6f9b14793ed9f618a558a5
    # Consider adding GIT_SHALLOW TRUE for faster downloads
)
FetchContent_MakeAvailable(PicoSHA2)

# Add PicoSHA2 header directory to include paths (CMake target handles this automatically if PicoSHA2 provides one, otherwise do it manually)
# PicoSHA2 is header-only, so we don't technically need a library target,
# but creating an INTERFACE library is good practice for usage requirements.
if(NOT TARGET PicoSHA2::PicoSHA2)
    add_library(PicoSHA2 INTERFACE)
    target_include_directories(PicoSHA2 INTERFACE ${picosha2_SOURCE_DIR}) # Correct variable name is lowercased by FetchContent
    add_library(PicoSHA2::PicoSHA2 ALIAS PicoSHA2) # Provide namespaced target
    message(STATUS "PicoSHA2 source fetched and interface target created.")
else()
     message(STATUS "PicoSHA2 target already available (perhaps from parent project or previous run).")
endif()

# --- GoogleTest ---
# Only fetch Google Test if testing is enabled in the parent scope
if(CODEBUNDLER_ENABLE_TESTING)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        release-1.12.1 # Use a specific stable tag
        # Consider adding GIT_SHALLOW TRUE
    )
    # Prevent GTest from installing globally and building its own tests
    set(INSTALL_GTEST OFF CACHE BOOL "Disable installation of GTest")
    set(BUILD_GMOCK ON CACHE BOOL "Build GMock") # Usually needed with GTest
    set(BUILD_GTEST ON CACHE BOOL "Build GTest") # Ensure GTest itself is built

    FetchContent_MakeAvailable(googletest)
    message(STATUS "GoogleTest source fetched and made available.")

    # Check if targets were created successfully
    if(TARGET GTest::gtest AND TARGET GTest::gmock)
         message(STATUS "GoogleTest targets GTest::gtest and GTest::gmock are available.")
    else()
         message(WARNING "GoogleTest targets GTest::gtest / GTest::gmock were NOT created successfully by FetchContent_MakeAvailable.")
    endif()

else()
    message(STATUS "GoogleTest fetching skipped as testing is disabled.")
endif()
