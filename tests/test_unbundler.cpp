#include "exceptions.hpp"
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
    std::string create_valid_bundle(const std::string& sep = "--- // ---")
    {
        std::stringstream ss;

        // file contents in a bundle will always end with a newline,
        // even if the last line in the file doesn't have one.
        std::string content1 = "File 1 content.\n";
        std::string content2 = "File 2\nMore lines.\n";
        std::string checksum1 = codebundler::utilities::calculateSHA256(content1);
        std::string checksum2 = codebundler::utilities::calculateSHA256(content2);

        ss << sep << "\n";
        ss << "Description: A test bundle\n";
        ss << sep << "\n";
        ss << "Filename: fileA.txt\n";
        ss << "Checksum: SHA256:" << checksum1 << "\n";
        ss << content1;
        ss << sep << "\n";
        ss << "Filename: data/fileB.txt\n";
        ss << "Checksum: SHA256:" << checksum2 << "\n";
        ss << content2;
        ss << sep << "\n";

        return ss.str();
    }
};

TEST_F(UnbundlerTest, UnbundleFromStreamBasic)
{
    using namespace codebundler;
    Unbundler::Options options;
    Unbundler unbundler(options);
    std::string bundle_str = create_valid_bundle();
    std::stringstream input_stream(bundle_str);

    ASSERT_NO_THROW(unbundler.unbundleFromStream(input_stream, test_output_dir));

    // Verify files were created
    std::filesystem::path fileA_path = test_output_dir / "fileA.txt";
    std::filesystem::path fileB_path = test_output_dir / "data/fileB.txt";

    EXPECT_TRUE(std::filesystem::exists(fileA_path));
    EXPECT_TRUE(std::filesystem::exists(fileB_path));
    EXPECT_TRUE(std::filesystem::is_directory(test_output_dir / "data"));

    // Verify content
    std::string contentA = utilities::readFileContent(fileA_path);
    std::string contentB = utilities::readFileContent(fileB_path);

    EXPECT_EQ(contentA, "File 1 content.\n");
    // Note: The unbundler adds a newline if the last line in the stream buffer didn't have one.
    // Let's check if the original content is present. Adjust expectations based on implementation.
    // The current implementation adds '\n' after every line read, so expect that.
    EXPECT_EQ(contentB, "File 2\nMore lines.\n");
}

TEST_F(UnbundlerTest, UnbundleWithNoVerify)
{
    using namespace codebundler;
    Unbundler::Options options;
    options.verifyChecksums = false; // Disable verification
    Unbundler unbundler(options);

    // Create a bundle with an INCORRECT checksum
    std::string sep = "--- // ---";
    std::stringstream ss;
    std::string content1 = "Some data.";
    std::string bad_checksum = std::string(
        "incorrectchecksum1234567890abcdef1234567890abcdef1234567890abcdefghijk")
                                   .substr(0, 64);

    ss << sep << "\n";
    ss << "Filename: bad_checksum.txt\n";
    ss << "Checksum: SHA256:" << bad_checksum << "\n";
    ss << content1 << "\n";
    ss << sep << "\n";

    std::stringstream input_stream(ss.str());

    // Should NOT throw ChecksumMismatchException
    ASSERT_NO_THROW(unbundler.unbundleFromStream(input_stream, test_output_dir));

    // Verify file was still created
    std::filesystem::path file_path = test_output_dir / "bad_checksum.txt";
    EXPECT_TRUE(std::filesystem::exists(file_path));
    EXPECT_EQ(utilities::readFileContent(file_path), content1 + "\n"); // Expect added newline
}

TEST_F(UnbundlerTest, UnbundleThrowsOnChecksumMismatch)
{
    using namespace codebundler;
    Unbundler::Options options;
    options.verifyChecksums = true; // Verification ENABLED (default)
    Unbundler unbundler(options);

    // Create a bundle with an INCORRECT checksum
    std::string sep = "--- // ---";
    std::stringstream ss;
    std::string content1 = "Some data.";
    std::string bad_checksum = std::string(
        "incorrectchecksum1234567890abcdef1234567890abcdef1234567890abcdefghijglmn")
                                   .substr(0, 64);

    ss << sep << "\n";
    ss << "Filename: bad_checksum.txt\n";
    ss << "Checksum: SHA256:" << bad_checksum << "\n";
    ss << content1 << "\n";
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

TEST_F(UnbundlerTest, VerifyValidBundle)
{
    using namespace codebundler;
    Unbundler::Options options;
    Unbundler unbundler(options);
    std::string bundle_str = create_valid_bundle();
    std::stringstream input_stream(bundle_str);

    bool result = false;
    ASSERT_NO_THROW(result = unbundler.verifyBundleStream(input_stream));
    EXPECT_TRUE(result);

    // Ensure no files were created in output dir
    EXPECT_TRUE(std::filesystem::is_empty(test_output_dir));
}

TEST_F(UnbundlerTest, VerifyInvalidChecksumBundle)
{
    using namespace codebundler;
    Unbundler::Options options;
    Unbundler unbundler(options);
    // Bundle with bad checksum from previous test
    std::string sep = "--- // ---";
    std::stringstream ss;
    std::string content1 = "Some data.";
    std::string bad_checksum = std::string(
        "incorrectchecksum1234567890abcdef1234567890abcdef1234567890abcdefghijk")
                                   .substr(0, 64);
    ss << sep << "\n";
    ss << "Filename: bad_checksum.txt\n";
    ss << "Checksum: SHA256:" << bad_checksum << "\n";
    ss << content1 << "\n";
    ss << sep << "\n";
    std::stringstream input_stream(ss.str());

    bool result = true; // Expect it to be set to false or throw
    // Verify should throw on mismatch
    EXPECT_THROW(result = unbundler.verifyBundleStream(input_stream), ChecksumMismatchException);
    // Result should remain unchanged or be false if exception wasn't thrown (which would be a test failure)

    // Ensure no files were created
    EXPECT_TRUE(std::filesystem::is_empty(test_output_dir));
}

TEST_F(UnbundlerTest, UnbundleWithMissingChecksumAndVerifyEnabled)
{
    using namespace codebundler;
    Unbundler::Options options;
    options.verifyChecksums = true; // Verification enabled
    Unbundler unbundler(options);

    // Create a bundle missing a Checksum line
    std::string sep = "--- // ---";
    std::stringstream ss;
    std::string content1 = "Data without checksum line.";

    ss << sep << "\n";
    ss << "Filename: no_checksum.txt\n";
    // Missing Checksum line here
    ss << content1 << "\n";
    ss << sep << "\n";

    std::stringstream input_stream(ss.str());

    // Should unbundle successfully but likely print a warning (check cout/cerr if testing output)
    // It should NOT throw ChecksumMismatchException
    ASSERT_NO_THROW(unbundler.unbundleFromStream(input_stream, test_output_dir));

    // Verify file was created
    std::filesystem::path file_path = test_output_dir / "no_checksum.txt";
    EXPECT_TRUE(std::filesystem::exists(file_path));
    EXPECT_EQ(utilities::readFileContent(file_path), content1 + "\n");
}

// Add more tests:
// - Malformed bundles (missing separators, bad headers, empty filenames)
// - Bundles with different separators
// - Bundles with binary content
// - Bundles ending abruptly
// - Empty bundle file
// - Error handling for unwritable output directory/files
