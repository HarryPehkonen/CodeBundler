# tests/CMakeLists.txt
add_executable(codebundler_tests
    codebundler_test.cpp
)

target_link_libraries(codebundler_tests
    PRIVATE
    codebundler_lib
    GTest::gtest_main
)

# Add compiler flags specific to tests
target_compile_options(codebundler_tests
    PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:
        -Wno-unused-parameter
        -Wno-sign-conversion  # For GTest internal warnings
    >
    $<$<CXX_COMPILER_ID:MSVC>:
        /wd4996  # Disable deprecation warnings in GTest
    >
)

include(GoogleTest)
gtest_discover_tests(codebundler_tests
    PROPERTIES
    LABELS "unit"
    TIMEOUT 10  # Maximum test duration in seconds
)

# Add custom target for test coverage if gcc is used
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(codebundler_tests PRIVATE --coverage)
    target_link_options(codebundler_tests PRIVATE --coverage)

    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}/coverage
                ${CMAKE_COMMAND} -E env LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/coverage/coverage.profraw
                $<TARGET_FILE:codebundler_tests>
        COMMAND gcovr -r ${CMAKE_SOURCE_DIR} --html --html-details
                -o ${CMAKE_BINARY_DIR}/coverage/coverage.html
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS codebundler_tests
    )
endif()
