// tests/file_combiner_test.cpp

#include <gtest/gtest.h>
#include "../src/file_combiner.hpp"
#include <fstream>
#include <filesystem>
#include <sstream>

class FileCombinerTest : public ::testing::Test {
protected:
    FileProcessor processor;
    FileCombiner combiner;

    FileCombinerTest() : combiner(processor) {
        processor.addMatcher(std::make_unique<ExtensionMatcher>(".cpp"));
        processor.addMatcher(std::make_unique<ExactNameMatcher>("CMakeLists.txt"));
    }

    void SetUp() override {
        // Create test files and directories
        std::filesystem::create_directories("test_src");
        std::ofstream("test_src/test1.cpp") << "Test content 1";
        std::ofstream("test_src/test2.cpp") << "Test content 2";
        std::ofstream("test_src/CMakeLists.txt") << "Test CMake content";
    }

    void TearDown() override {
        // Clean up test files and directories
        std::filesystem::remove_all("test_src");
        std::filesystem::remove("combined_output.txt");
    }
};

TEST_F(FileCombinerTest, CombinesFilesToFile) {
    combiner.combineFiles({"test_src"}, "combined_output.txt");

    std::ifstream combined_file("combined_output.txt");
    std::string content((std::istreambuf_iterator<char>(combined_file)),
                         std::istreambuf_iterator<char>());

    EXPECT_TRUE(content.find("Path: test_src/test1.cpp") != std::string::npos);
    EXPECT_TRUE(content.find("Test content 1") != std::string::npos);
    EXPECT_TRUE(content.find("Path: test_src/test2.cpp") != std::string::npos);
    EXPECT_TRUE(content.find("Test content 2") != std::string::npos);
    EXPECT_TRUE(content.find("Path: test_src/CMakeLists.txt") != std::string::npos);
    EXPECT_TRUE(content.find("Test CMake content") != std::string::npos);
}

TEST_F(FileCombinerTest, CombinesFilesToStream) {
    std::ostringstream output_stream;
    combiner.combineFiles({"test_src"}, output_stream);

    std::string content = output_stream.str();

    EXPECT_TRUE(content.find("Path: test_src/test1.cpp") != std::string::npos);
    EXPECT_TRUE(content.find("Test content 1") != std::string::npos);
    EXPECT_TRUE(content.find("Path: test_src/test2.cpp") != std::string::npos);
    EXPECT_TRUE(content.find("Test content 2") != std::string::npos);
    EXPECT_TRUE(content.find("Path: test_src/CMakeLists.txt") != std::string::npos);
    EXPECT_TRUE(content.find("Test CMake content") != std::string::npos);
}
