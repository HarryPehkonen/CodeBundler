// tests/file_extractor_test.cpp

#include <gtest/gtest.h>
#include "../src/file_extractor.hpp"
#include "../src/constants.hpp"
#include <fstream>
#include <filesystem>
#include <sstream>

class FileExtractorTest : public ::testing::Test {
protected:
    FileExtractor extractor;
    const std::string test_file = "test_combined.txt";
    const std::string test_dir = "test_extract";

    void SetUp() override {
        // Create a test combined file
        std::ofstream combined_file(test_file);
        combined_file << BOUNDARY_STRING << "\n";
        combined_file << "Path: " << test_dir << "/file1.cpp\n\n";
        combined_file << "Test content 1\n";
        combined_file << BOUNDARY_STRING << "\n";
        combined_file << "Path: " << test_dir << "/file2.cpp\n\n";
        combined_file << "Test content 2\n";
        combined_file << BOUNDARY_STRING << "\n";
    }

    void TearDown() override {
        // Clean up test files and directories
        std::filesystem::remove(test_file);
        std::filesystem::remove_all(test_dir);
    }
};

TEST_F(FileExtractorTest, ExtractsFilesFromFile) {
    extractor.extractFiles(test_file);

    EXPECT_TRUE(std::filesystem::exists(test_dir + "/file1.cpp"));
    EXPECT_TRUE(std::filesystem::exists(test_dir + "/file2.cpp"));

    std::ifstream file1(test_dir + "/file1.cpp");
    std::string content1((std::istreambuf_iterator<char>(file1)),
                          std::istreambuf_iterator<char>());
    EXPECT_EQ(content1, "Test content 1\n");

    std::ifstream file2(test_dir + "/file2.cpp");
    std::string content2((std::istreambuf_iterator<char>(file2)),
                          std::istreambuf_iterator<char>());
    EXPECT_EQ(content2, "Test content 2\n");
}

TEST_F(FileExtractorTest, ExtractsFilesToStream) {
    std::ifstream input_file(test_file);
    std::ostringstream output_stream;

    extractor.extractFiles(input_file, &output_stream);

    std::string content = output_stream.str();
    EXPECT_TRUE(content.find("Extracted file: " + test_dir + "/file1.cpp") != std::string::npos);
    EXPECT_TRUE(content.find("Test content 1") != std::string::npos);
    EXPECT_TRUE(content.find("Extracted file: " + test_dir + "/file2.cpp") != std::string::npos);
    EXPECT_TRUE(content.find("Test content 2") != std::string::npos);
}

TEST_F(FileExtractorTest, ThrowsOnMalformedInput) {
    std::ofstream bad_file("bad_combined.txt");
    bad_file << BOUNDARY_STRING << "\n";
    bad_file << "Path: " << test_dir << "/bad_file.cpp\n";
    bad_file << "Missing empty line\n";
    bad_file << BOUNDARY_STRING << "\n";
    bad_file.close();

    EXPECT_THROW(extractor.extractFiles("bad_combined.txt"), std::runtime_error);

    std::ifstream bad_input("bad_combined.txt");
    std::ostringstream output;
    EXPECT_THROW(extractor.extractFiles(bad_input, &output), std::runtime_error);

    std::filesystem::remove("bad_combined.txt");
}
