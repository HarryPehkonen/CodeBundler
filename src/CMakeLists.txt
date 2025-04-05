# Define the main executable
add_executable(codebundler
    main.cpp
    bundler.cpp
    unbundler.cpp
    utilities.cpp
)

# Link required libraries
# PicoSHA2 is header-only, but the INTERFACE target handles include directories
target_link_libraries(codebundler PRIVATE PicoSHA2::PicoSHA2)

target_include_directories(codebundler PRIVATE ${PROJECT_SOURCE_DIR}/include)

set_target_properties(codebundler PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
)
