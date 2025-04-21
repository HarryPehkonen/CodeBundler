#include "exceptions.hpp"
#include "options.hpp"
#include "unbundler.hpp"
#include "utilities.hpp" // For reading created files, calculating checksums
#include <filesystem> // Requires C++17
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>

// Test fixture for unbundler tests
class UnbundlerTest : public ::testing::Test {
protected:
    std::filesystem::path test_output_dir;
    std::filesystem::path original_cwd;

    void SetUp() override
    {
        original_cwd = std::filesystem::current_path();
        test_output_dir = std::filesystem::temp_directory_path() / "codebundler_unbundler_test_out";
        cleanup_test_output(); // Clean up from previous runs
        std::filesystem::create_directories(test_output_dir);
    }

    void TearDown() override
    {
        std::filesystem::current_path(original_cwd); // Restore CWD
        cleanup_test_output();
    }

    void cleanup_test_output()
    {
        std::error_code ec;
        std::filesystem::remove_all(test_output_dir, ec);
        // Ignore errors during cleanup
    }

    // Helper to create a basic valid bundle string
    // Default separator matches the one used in main.cpp for consistency
    std::string create_valid_bundle(const std::string& sep = "========== BOUNDARY ==========")
    {
        std::stringstream ss;

        // Ensure content for checksum calculation matches what unbundler writes/reads
        // Unbundler adds a newline after each line read via getline, including the last one.
        std::string content1 = "File 1 content.\n";
        std::string content2 = "File 2\nMore lines.\n";
        std::string checksum1 = codebundler::utilities::calculateSHA256(content1);
        std::string checksum2 = codebundler::utilities::calculateSHA256(content2);

        ss << sep << "\n"; // The separator line itself
        ss << "Description: A test bundle\n"; // Optional header line
        ss << sep << "\n"; // Separator after header
        ss << "Filename: fileA.txt\n";
        ss << "Checksum: SHA256:" << checksum1 << "\n";
        ss << content1; // Content doesn't include trailing newline in *this* stream
        ss << sep << "\n";
        ss << "Filename: data/fileB.txt\n";
        ss << "Checksum: SHA256:" << checksum2 << "\n";
        ss << content2; // Content doesn't include trailing newline in *this* stream
        ss << sep << "\n"; // Final separator

        return ss.str();
    }
};

TEST_F(UnbundlerTest, UnbundleFromStreamBasic)
{
    using namespace codebundler;
    Options options;
    Unbundler unbundler(options); // Separator no longer needed in options
    std::string bundle_str = create_valid_bundle();
    std::stringstream input_stream(bundle_str);

    ASSERT_NO_THROW(unbundler.unbundleFromStream(input_stream, test_output_dir));

    // Verify files were created
    std::filesystem::path fileA_path = test_output_dir / "fileA.txt";
    std::filesystem::path fileB_path = test_output_dir / "data/fileB.txt";

    ASSERT_TRUE(std::filesystem::exists(fileA_path));
    ASSERT_TRUE(std::filesystem::exists(fileB_path));
    ASSERT_TRUE(std::filesystem::is_directory(test_output_dir / "data"));

    // Verify content - should match the content used for checksum calculation
    std::string contentA = utilities::readFileContent(fileA_path);
    std::string contentB = utilities::readFileContent(fileB_path);

    EXPECT_EQ(contentA, "File 1 content.\n");
    EXPECT_EQ(contentB, "File 2\nMore lines.\n");
}

TEST_F(UnbundlerTest, UnbundleWithNoVerify)
{
    using namespace codebundler;
    Options options;
    options.verify = false; // Disable verification; separator auto-detected
    Unbundler unbundler(options);

    // Create a bundle with an INCORRECT checksum
    std::string sep = "========== BOUNDARY ==========";
    std::stringstream ss;
    std::string content1 = "Some data.\n"; // Content needs newline for file write/read consistency
    std::string bad_checksum = std::string(
        "incorrectchecksum1234567890abcdef1234567890abcdef1234567890abcdefghijk")
                                   .substr(0, 64);

    ss << sep << "\n";
    ss << "Filename: bad_checksum.txt\n";
    ss << "Checksum: SHA256:" << bad_checksum << "\n";
    ss << content1; // Write content
    ss << sep << "\n";

    std::stringstream input_stream(ss.str());

    // Should NOT throw ChecksumMismatchException
    ASSERT_NO_THROW(unbundler.unbundleFromStream(input_stream, test_output_dir));

    // Verify file was still created
    std::filesystem::path file_path = test_output_dir / "bad_checksum.txt";
    ASSERT_TRUE(std::filesystem::exists(file_path));
    EXPECT_EQ(utilities::readFileContent(file_path), content1);
}

TEST_F(UnbundlerTest, UnbundleThrowsOnChecksumMismatch)
{
    using namespace codebundler;
    Options options;
    options.trialRun = true; // Verification ENABLED (default); separator auto-detected
    Unbundler unbundler(options);

    // Create a bundle with an INCORRECT checksum
    std::string sep = "========== BOUNDARY ==========";
    std::stringstream ss;
    std::string content1 = "Some data.\n";
    std::string bad_checksum = std::string(
        "incorrectchecksum1234567890abcdef1234567890abcdef1234567890abcdefghijglmn")
                                   .substr(0, 64);

    ss << sep << "\n";
    ss << "Filename: bad_checksum.txt\n";
    ss << "Checksum: SHA256:" << bad_checksum << "\n";
    ss << content1;
    ss << sep << "\n";

    std::stringstream input_stream(ss.str());

    // Should THROW ChecksumMismatchException
    EXPECT_THROW(unbundler.unbundleFromStream(input_stream, test_output_dir), ChecksumMismatchException);

    // Verify file was NOT created (or maybe partially created then stopped, depends on implementation)
    // A robust implementation might delete partially extracted files on error.
    // For simplicity, we just check it doesn't exist fully.
    std::filesystem::path file_path = test_output_dir / "bad_checksum.txt";
    EXPECT_FALSE(std::filesystem::exists(file_path));
}

TEST_F(UnbundlerTest, UnbundleWithMissingChecksumAndVerifyEnabled)
{
    using namespace codebundler;
    Options options;
    options.verify = true; // Verification enabled;
    Unbundler unbundler(options);

    // Create a bundle missing a Checksum line
    std::string sep = "========== BOUNDARY ==========";
    std::stringstream ss;
    std::string content1 = "Data without checksum line.\n";

    ss << sep << "\n";
    ss << "Filename: no_checksum.txt\n";
    // Missing Checksum line here
    ss << content1;
    ss << sep << "\n";

    std::stringstream input_stream(ss.str());

    EXPECT_THROW(unbundler.unbundleFromStream(input_stream, test_output_dir), ChecksumMismatchException);

    // Verify file was created
    std::filesystem::path file_path = test_output_dir / "no_checksum.txt";
    ASSERT_FALSE(std::filesystem::exists(file_path));
}

TEST_F(UnbundlerTest, UnbundleWithCustomSeparator)
{
    using namespace codebundler;
    Options options;
    Unbundler unbundler(options); // Auto-detects separator
    std::string custom_sep = "%%% CUSTOM SEPARATOR %%%";
    std::string bundle_str = create_valid_bundle(custom_sep); // Create bundle *with* custom sep
    std::stringstream input_stream(bundle_str);

    ASSERT_NO_THROW(unbundler.unbundleFromStream(input_stream, test_output_dir));

    // Verify files were created (basic check)
    ASSERT_TRUE(std::filesystem::exists(test_output_dir / "fileA.txt"));
    ASSERT_TRUE(std::filesystem::exists(test_output_dir / "data/fileB.txt"));
    // Content check (optional, done in basic test)
    EXPECT_EQ(utilities::readFileContent(test_output_dir / "fileA.txt"), "File 1 content.\n");
}

TEST_F(UnbundlerTest, MalformedBundleMissingFilename)
{
    using namespace codebundler;
    Options options;
    Unbundler unbundler(options);
    std::string sep = "---SEP---";
    std::stringstream bad_stream;
    bad_stream << sep << "\n";
    bad_stream << "Checksum: SHA256:...\n"; // Missing Filename line
    bad_stream << "content\n";
    bad_stream << sep << "\n";

    EXPECT_THROW(unbundler.unbundleFromStream(bad_stream, test_output_dir), BundleFormatException);
}

TEST_F(UnbundlerTest, BundleEndsAbruptlyAfterFilename)
{
    using namespace codebundler;
    Options options;
    options.verify = false;
    Unbundler unbundler(options);
    std::string sep = "---SEP---";
    std::stringstream bad_stream;
    bad_stream << sep << "\n";
    bad_stream << "Filename: incomplete.txt"; // No newline, no checksum, no content, no final separator

    // It should process the entry (likely with warnings) but might not throw depending on strictness.
    // Current implementation logs warning and processes empty content. Check log output if important.
    EXPECT_THROW(unbundler.unbundleFromStream(bad_stream, test_output_dir), BundleFormatException);
    // Verify the file was created, likely empty
    std::filesystem::path file_path = test_output_dir / "incomplete.txt";
    ASSERT_FALSE(std::filesystem::exists(file_path));
}

// Add more tests:
// - Bundles with binary content (ensure no corruption) -> Covered by bundler test data, verify here too if needed
// - Error handling for unwritable output directory/files (might need more setup)
// - Bundle containing only header
