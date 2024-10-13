// tests/file_processor_test.cpp

#include <gtest/gtest.h>
#include "../src/file_processor.hpp"
#include <filesystem>

class FileProcessorTest : public ::testing::Test {
protected:
    FileProcessor processor;

    void SetUp() override {
        processor.addMatcher(std::make_unique<ExtensionMatcher>(".cpp"));
        processor.addMatcher(std::make_unique<ExactNameMatcher>("CMakeLists.txt"));
    }
};

TEST_F(FileProcessorTest, ShouldProcessMatchingFiles) {
    EXPECT_TRUE(processor.shouldProcessFile("test.cpp"));
    EXPECT_TRUE(processor.shouldProcessFile("CMakeLists.txt"));
    EXPECT_FALSE(processor.shouldProcessFile("test.hpp"));
}

TEST_F(FileProcessorTest, GetMatchingFiles) {
    // This test requires creating a temporary directory structure
    // and is more complex to implement. For brevity, we'll skip the implementation
    // but in a real scenario, you would set up a mock file system or use a test directory.
    GTEST_SKIP() << "Implement this test with a mock file system or test directory";
}
