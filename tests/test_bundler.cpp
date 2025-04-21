#include "bundler.hpp"
#include "exceptions.hpp"
#include "utilities.hpp" // For helpers if needed
#include <cstdlib> // For system()
#include <filesystem> // Requires C++17
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>

// Helper function to create dummy files and initialize a git repo
// WARNING: This modifies the filesystem and runs git commands. Use with caution.
// Should ideally run in a dedicated temporary test directory.
void setup_test_git_repo(const std::filesystem::path& repo_path)
{
    std::filesystem::create_directories(repo_path);
    std::ofstream(repo_path / "file1.txt") << "Content of file 1.\n"; // Text file is fine with <<

    // --- FIX FOR BINARY FILE ---
    std::filesystem::create_directories(repo_path / "subdir"); // Ensure subdir exists
    // Open in binary mode explicitly
    std::ofstream bin_file_stream(repo_path / "subdir" / "file2.bin", std::ios::binary | std::ios::out | std::ios::trunc);
    if (!bin_file_stream) {
        // Fail the test setup if file cannot be opened
        GTEST_FAIL() << "Failed to open test file for writing: " << (repo_path / "subdir/file2.bin").string();
        return; // Prevent further execution if failed
    }
    // Use write() for precise control over binary data including nulls
    const char data_part1[] = "Binary\0Data"; // Note: sizeof includes the *string literal's* null terminator
    bin_file_stream.write(data_part1, sizeof(data_part1) - 1); // Write 11 bytes ('B' through 'a')

    std::string data_part2(10, '\x01');
    bin_file_stream.write(data_part2.data(), data_part2.size()); // Write the 10 '\x01' bytes

    bin_file_stream.close(); // Explicit close (though destructor would handle it)
    // --- END FIX ---

    // VERY basic git init and add. Assumes git is in PATH.
    // Redirect output to avoid polluting test results.
    std::string cd_cmd = "cd \"" + repo_path.string() + "\"";
    int ret; // Check return codes for basic sanity
    ret = std::system((cd_cmd + " && git init -b main -q").c_str());
    if (ret != 0)
        GTEST_LOG_(WARNING) << "git init failed in test setup";
    ret = std::system((cd_cmd + " && git config user.email \"test@example.com\"").c_str());
    if (ret != 0)
        GTEST_LOG_(WARNING) << "git config user.email failed in test setup";
    ret = std::system((cd_cmd + " && git config user.name \"Test User\"").c_str());
    if (ret != 0)
        GTEST_LOG_(WARNING) << "git config user.name failed in test setup";
    ret = std::system((cd_cmd + " && git add .").c_str());
    if (ret != 0)
        GTEST_LOG_(WARNING) << "git add failed in test setup";
    // Add a commit so ls-files doesn't complain on some git versions
    ret = std::system((cd_cmd + " && git commit -m \"Initial test commit\" -q").c_str());
    if (ret != 0)
        GTEST_LOG_(WARNING) << "git commit failed in test setup";
}
void cleanup_test_git_repo(const std::filesystem::path& repo_path)
{
    std::error_code ec;
    std::filesystem::remove_all(repo_path, ec);
    // Ignore errors during cleanup
}

// Test fixture for bundler tests that need a git repo
class BundlerGitTest : public ::testing::Test {
protected:
    std::filesystem::path test_repo_path;
    std::filesystem::path original_cwd;

    void SetUp() override
    {
        original_cwd = std::filesystem::current_path();
        test_repo_path = std::filesystem::temp_directory_path() / "codebundler_bundler_test_repo";
        cleanup_test_git_repo(test_repo_path); // Clean up any previous runs
        setup_test_git_repo(test_repo_path);
        std::filesystem::current_path(test_repo_path); // Change CWD for git commands
    }

    void TearDown() override
    {
        std::filesystem::current_path(original_cwd); // Restore CWD
        cleanup_test_git_repo(test_repo_path);
    }
};

TEST_F(BundlerGitTest, BundleToStreamBasic)
{
    using namespace codebundler;
    Options options;
    Bundler bundler(options);
    std::stringstream output;

    // Since we changed CWD, getGitTrackedFiles should work if git commands run
    ASSERT_NO_THROW(bundler.bundleToStream(output));

    std::string bundleContent = output.str();

    // Basic checks - expecting separator, filename headers, content
    EXPECT_NE(bundleContent.find("========= BOUNDARY =========="), std::string::npos);
    EXPECT_NE(bundleContent.find("Filename: file1.txt"), std::string::npos);
    EXPECT_NE(bundleContent.find("Checksum: SHA256:"), std::string::npos);
    EXPECT_NE(bundleContent.find("Content of file 1."), std::string::npos);

#ifdef _WIN32 // Git might use backslashes on Windows
    EXPECT_TRUE(bundleContent.find("Filename: subdir\\file2.bin") != std::string::npos || bundleContent.find("Filename: subdir/file2.bin") != std::string::npos);
#else
    EXPECT_NE(bundleContent.find("Filename: subdir/file2.bin"), std::string::npos);
#endif

    EXPECT_NE(bundleContent.find("Binary\0Data", 0, 11), std::string::npos); // Check for binary data part

    // Verify checksum for file1.txt (calculate expected manually or via utility)
    std::string file1_content = "Content of file 1.\n";
    std::string file1_expected_checksum = utilities::calculateSHA256(file1_content);
    EXPECT_NE(bundleContent.find("SHA256:" + file1_expected_checksum), std::string::npos);
}

TEST_F(BundlerGitTest, BundleWithCustomSeparatorAndDescription)
{
    using namespace codebundler;
    Options options;
    std::string custom_sep = "=== My_Separator ===";
    options.separator = custom_sep; // Set custom separator
    std::string description = "Test bundle description";
    Bundler bundler(options);
    std::stringstream output;

    ASSERT_NO_THROW(bundler.bundleToStream(output, description));

    std::string bundleContent = output.str();

    EXPECT_NE(bundleContent.find(custom_sep), std::string::npos);
    EXPECT_NE(bundleContent.find("Description: " + description), std::string::npos);
    EXPECT_NE(bundleContent.find("Filename: file1.txt"), std::string::npos);

    // Check that default separator is NOT present
    EXPECT_EQ(bundleContent.find("========== BOUNDARY =========="), std::string::npos);
}

TEST(BundlerTest, ConstructorEmptySeparator)
{
    using namespace codebundler;
    Options options;
    options.separator = ""; // Empty separator
    EXPECT_THROW(Bundler bundler(options), std::invalid_argument);
}

// Add more tests:
// - Bundling an empty repository
// - Bundling when git command fails (might need mocking or more complex setup)
// - Bundling specific files (if that feature is added)
// - Error handling for unreadable files
