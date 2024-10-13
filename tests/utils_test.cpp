// tests/utils_test.cpp

#include <gtest/gtest.h>
#include "../src/utils.hpp"

TEST(UtilsTest, TrimFunction) {
    EXPECT_EQ(utils::trim("  hello  "), "hello");
    EXPECT_EQ(utils::trim("hello  "), "hello");
    EXPECT_EQ(utils::trim("  hello"), "hello");
    EXPECT_EQ(utils::trim("hello"), "hello");
    EXPECT_EQ(utils::trim("  "), "");
    EXPECT_EQ(utils::trim(""), "");
}
