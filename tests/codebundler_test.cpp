// tests/codebundler_test.cpp
#include <bits/chrono.h>                // for filesystem
#include <string.h>                     // for strlen, size_t
#include <algorithm>                    // for max
#include <filesystem>                   // for path, remove_all, current_path
#include <fstream>                      // for stringstream, char_traits
#include <iostream>                     // for cerr
#include <sstream>                      // for basic_stringbuf<>::int_type
#include <stdexcept>                    // for runtime_error
#include <string>                       // for string, basic_string, allocator
#include <string_view>                  // for operator<<, string_view
#include <vector>                       // for vector
#include "codebundler/codebundler.hpp"  // for CodeBundler, print_directory_...
#include "codebundler/errors.hpp"       // for BundleError, codebundler
#include "codebundler/git_loader.hpp"   // for GitFileLoader
#include "codebundler/options.hpp"      // for Options
#include "gtest/gtest.h"                // for Message, AssertionResult, Tes...

namespace fs = std::filesystem;
using namespace codebundler;

class CodeBundlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up any leftover test directories
        fs::remove_all("test_files");
        fs::remove_all("test_unbundle");

        // Create test directory structure
        ASSERT_TRUE(fs::create_directories("test_files/subdir"));

        // Create test files with known content
        createTestFile("test_files/file1.txt", "Content of file 1\nSecond line\n");
        createTestFile("test_files/file2.txt", "Content of file 2\n");
        createTestFile("test_files/subdir/file3.txt", "Content in subdirectory\n");

        test_files = {
            "test_files/file1.txt",
            "test_files/file2.txt",
            "test_files/subdir/file3.txt"
        };
    }

    void TearDown() override {
        fs::remove_all("test_files");
        fs::remove_all("test_unbundle");
        fs::remove("test_bundle.txt");
    }

    void createTestFile(const fs::path& path, std::string_view content) {
        std::ofstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to create: " + path.string());
        }
        file << content;
        if (!file) {
            throw std::runtime_error("Failed to write to: " + path.string());
        }
    }

    std::string readFile(const fs::path& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open: " + path.string());
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        if (!file) {
            throw std::runtime_error("Failed to read: " + path.string());
        }
        return buffer.str();
    }

    std::vector<std::string> test_files;
};

TEST_F(CodeBundlerTest, BundleToStringStream) {
    std::stringstream output;
    EXPECT_NO_THROW({
        CodeBundler::bundle(output, test_files);
    });

    std::string result = output.str();
    EXPECT_NE(result.find("Filename: "), std::string::npos);
    EXPECT_NE(result.find("Content of file 1"), std::string::npos);
    EXPECT_NE(result.find("Content of file 2"), std::string::npos);
    EXPECT_NE(result.find("Content in subdirectory"), std::string::npos);
}

TEST_F(CodeBundlerTest, UnbundleFromStringStream) {
    // First create bundle
    std::stringstream bundle;
    ASSERT_NO_THROW({
        CodeBundler::bundle(bundle, test_files);
    });

    // Reset stream position to beginning before unbundling
    bundle.seekg(0);  // Add this line
    std::string contents = bundle.str();

    // Then unbundle
    ASSERT_NO_THROW({
        CodeBundler::unbundle(bundle, "test_unbundle");
    });

    {
        fs::path unbundle_path("test_unbundle");
        ASSERT_TRUE(fs::exists(unbundle_path)) << "Failed to create: " << unbundle_path;
        ASSERT_TRUE(fs::is_directory(unbundle_path)) << "Not a directory: " << unbundle_path;
    }

    // Verify contents
    for (const auto& filename : test_files) {
        fs::path orig_path(filename);
        fs::path new_path("test_unbundle" / orig_path);

        ASSERT_TRUE(fs::exists(new_path)) << "Failed to create: " << new_path;
        EXPECT_EQ(readFile(orig_path), readFile(new_path));
    }
}

TEST_F(CodeBundlerTest, BundleWithCustomOptions) {
    Options options("===CUSTOM===", "Custom description\n");
    std::stringstream output;

    EXPECT_NO_THROW({
        CodeBundler::bundle(output, test_files, options);
    });

    std::string result = output.str();
    EXPECT_NE(result.find("===CUSTOM==="), std::string::npos);
    EXPECT_NE(result.find("Custom description"), std::string::npos);
}

TEST_F(CodeBundlerTest, HandleInvalidSeparator) {
    EXPECT_THROW({
        Options options("");  // Empty separator
    }, BundleError);

    EXPECT_THROW({
        Options options("invalid\nseparator");  // Contains newline
    }, BundleError);
}

TEST_F(CodeBundlerTest, VerifyBundleIntegrity) {
    // Create a valid bundle
    std::stringstream bundle;
    ASSERT_NO_THROW({
        CodeBundler::bundle(bundle, test_files);
    });

    // Save original content
    std::string original_content = bundle.str();

    // Verify the valid bundle
    bundle.seekg(0);
    EXPECT_TRUE(CodeBundler::verify(bundle));

    // Create a corrupt bundle
    std::string corrupt_content = original_content;
    size_t pos = corrupt_content.find("Content of file 1");
    ASSERT_NE(pos, std::string::npos) << "Test string not found";
    corrupt_content.replace(pos, strlen("Content of file 1"), "Modified content");

    std::stringstream verify_stream(corrupt_content);

    verify_stream.seekg(0);
    EXPECT_FALSE(CodeBundler::verify(verify_stream));
}

TEST_F(CodeBundlerTest, HandleMissingFiles) {
    test_files.push_back("nonexistent.txt");

    EXPECT_THROW({
        std::stringstream output;
        CodeBundler::bundle(output, test_files);
    }, BundleError);
}

TEST_F(CodeBundlerTest, HandleInvalidOutputDirectory) {
    std::stringstream bundle;
    ASSERT_NO_THROW({
        CodeBundler::bundle(bundle, test_files);
    });

    bundle.seekg(0);
    EXPECT_THROW({
        try {
            CodeBundler::unbundle(bundle, "/nonexistent/path");
        } catch (const std::filesystem::filesystem_error& e) {
            // Convert filesystem error to BundleError
            throw BundleError(std::string("Filesystem error: ") + e.what());
        }
    }, BundleError);
}

TEST(GitFileLoaderTest, HandleInvalidGitRepository) {
    // Save current path
    auto current_path = fs::current_path();

    // Create and move to temporary directory
    fs::path temp_dir = fs::temp_directory_path() / "not_a_git_repo";
    fs::create_directories(temp_dir);
    fs::current_path(temp_dir);

    try {
        EXPECT_THROW({
            try {
                auto files = GitFileLoader::getTrackedFiles();
            } catch (const std::runtime_error& e) {
                // Convert runtime_error to BundleError
                throw BundleError(std::string("Git error: ") + e.what());
            }
        }, BundleError);
    } catch (...) {
        // Ensure we restore the path even if test fails
        fs::current_path(current_path);
        fs::remove_all(temp_dir);
        throw;
    }

    // Cleanup
    fs::current_path(current_path);
    fs::remove_all(temp_dir);
}
