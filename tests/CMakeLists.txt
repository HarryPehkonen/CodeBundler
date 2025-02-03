## tests/CMakeLists.txt
add_executable(codebundler_tests
    codebundler_test.cpp
)

target_link_libraries(codebundler_tests
    PRIVATE
    codebundler_lib
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(codebundler_tests)
