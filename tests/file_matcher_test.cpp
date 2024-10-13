// tests/file_matcher_test.cpp

#include <gtest/gtest.h>
#include "../src/file_matcher.hpp"

TEST(ExtensionMatcherTest, MatchesCorrectExtension) {
    ExtensionMatcher matcher(".cpp");
    EXPECT_TRUE(matcher.matches("test.cpp"));
    EXPECT_FALSE(matcher.matches("test.hpp"));
}

TEST(ExactNameMatcherTest, MatchesExactName) {
    ExactNameMatcher matcher("CMakeLists.txt");
    EXPECT_TRUE(matcher.matches("CMakeLists.txt"));
    EXPECT_FALSE(matcher.matches("test.cpp"));
}
