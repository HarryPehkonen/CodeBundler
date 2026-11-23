#include "bundler.hpp"
#include "constants.hpp"
#include "exceptions.hpp"
#include "options.hpp"
#include "unbundler.hpp"
#include "utilities.hpp" // For helpers if needed
#include <cstdlib> // For system()
#include <filesystem> // Requires C++17
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>

// Helper function to create dummy files and initialize a git repo
// WARNING: This modifies the filesystem and runs git commands. Use with caution.
// Should ideally run in a dedicated temporary test directory.
// VERY basic git init, add and commit. Assumes git is in PATH.
void git_init_and_commit(const std::filesystem::path& repo_path)
{
    const std::string cd_cmd = "cd \"" + repo_path.string() + "\"";
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

// Create a repo at repo_path holding exactly these files (path -> raw bytes).
void setup_repo_with_files(const std::filesystem::path& repo_path,
    const std::vector<std::pair<std::string, std::string>>& files)
{
    for (const auto& [rel_path, bytes] : files) {
        const std::filesystem::path full_path = repo_path / rel_path;
        std::filesystem::create_directories(full_path.parent_path());
        std::ofstream out(full_path, std::ios::binary | std::ios::trunc);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    git_init_and_commit(repo_path);
}

void setup_test_git_repo(const std::filesystem::path& repo_path, bool with_binary = true)
{
    std::filesystem::create_directories(repo_path);
    std::ofstream(repo_path / "file1.txt") << "Content of file 1.\n"; // Text file is fine with <<

    if (with_binary) {
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
    }

    git_init_and_commit(repo_path);
}
void cleanup_test_git_repo(const std::filesystem::path& repo_path)
{
    std::error_code ec;
    std::filesystem::remove_all(repo_path, ec);
    // Ignore errors during cleanup
}

// Bundle the repository at repo_path with these options and return the bundle
// text. Bundling shells out to `git ls-files`, so the bundler has to run with
// that repository as the working directory.
std::string bundle_repo_with(const std::filesystem::path& repo_path, const codebundler::Options& options)
{
    const std::filesystem::path original_cwd = std::filesystem::current_path();
    std::string bundleContent;
    std::filesystem::current_path(repo_path);
    try {
        codebundler::Bundler bundler(options);
        std::stringstream output;
        bundler.bundleToStream(output);
        bundleContent = output.str();
    } catch (...) {
        std::filesystem::current_path(original_cwd);
        throw;
    }
    std::filesystem::current_path(original_cwd);
    return bundleContent;
}

// Bundle the repository at repo_path with default options and return the text.
std::string bundle_repo(const std::filesystem::path& repo_path)
{
    codebundler::Options options;
    return bundle_repo_with(repo_path, options);
}

// A tiny tree with a source directory, a test directory and a document - the
// shape a selection rule is asked to cut.
void setup_selection_repo(const std::filesystem::path& repo_path)
{
    setup_repo_with_files(repo_path,
        { { "src/main.cpp", "int main() { return 0; }\n" },
            { "src/util.cpp", "int util();\n" },
            { "tests/test_main.cpp", "TEST(Main, Runs) {}\n" },
            { "tests/test_util.cpp", "TEST(Util, Runs) {}\n" },
            { "docs/NOTES.md", "notes about the tree\n" } });
}

// Write a repository's selection file. Written after the fixture's commit, so it
// is untracked: the file supplies rules, it does not have to be bundled itself.
void write_ignore_file(const std::filesystem::path& repo_path, const std::string& contents)
{
    std::ofstream(repo_path / codebundler::IGNORE_FILE_NAME, std::ios::binary | std::ios::trunc).write(contents.data(), static_cast<std::streamsize>(contents.size()));
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
    EXPECT_NE(bundleContent.find(options.separator), std::string::npos);
    EXPECT_NE(bundleContent.find(codebundler::FILENAME_PREFIX + "file1.txt"), std::string::npos);
    EXPECT_NE(bundleContent.find(codebundler::CHECKSUM_PREFIX), std::string::npos);
    EXPECT_NE(bundleContent.find("Content of file 1."), std::string::npos);

    // file2.bin holds NUL bytes: it is binary, so it must never become an entry.
    // Its content cannot survive a line-based format, and inlining it risks
    // bytes that look like bundle structure (a "Filename: " line, the separator).
#ifdef _WIN32 // Git might use backslashes on Windows
    EXPECT_EQ(bundleContent.find(codebundler::FILENAME_PREFIX + "subdir\\file2.bin"), std::string::npos);
    EXPECT_EQ(bundleContent.find(codebundler::FILENAME_PREFIX + "subdir/file2.bin"), std::string::npos);
#else
    EXPECT_EQ(bundleContent.find(codebundler::FILENAME_PREFIX + "subdir/file2.bin"), std::string::npos);
#endif

    EXPECT_EQ(bundleContent.find("Binary\0Data", 0, 11), std::string::npos); // binary content is not inlined

    // ...but the skipped file is named, so nobody wonders where it went.
#ifdef _WIN32
    EXPECT_NE(bundleContent.find(codebundler::SKIPPED_BINARY_PREFIX + "subdir\\file2.bin"), std::string::npos);
#else
    EXPECT_NE(bundleContent.find(codebundler::SKIPPED_BINARY_PREFIX + "subdir/file2.bin"), std::string::npos);
#endif

    // Verify checksum for file1.txt (calculate expected manually or via utility)
    std::string file1_content = "Content of file 1.\n";
    std::string file1_expected_checksum = utilities::calculateSHA256(file1_content);
    std::string look_for = codebundler::CHECKSUM_PREFIX + file1_expected_checksum;
    std::cerr << "Bundle:  " << bundleContent << "\nThat's it" << std::endl;
    std::cerr << "Looking for: " << look_for << std::endl;
    EXPECT_NE(bundleContent.find(look_for), std::string::npos);
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
    EXPECT_NE(bundleContent.find(codebundler::FILENAME_PREFIX + "file1.txt"), std::string::npos);

    // Check that default separator is NOT present
    codebundler::Options default_options;
    EXPECT_EQ(bundleContent.find(default_options.separator), std::string::npos);
}

TEST(BundlerTest, ConstructorEmptySeparator)
{
    using namespace codebundler;
    Options options;
    options.separator = ""; // Empty separator
    EXPECT_THROW(Bundler bundler(options), std::invalid_argument);
}

// Test that demonstrates the issue: readFileLines fails on directories
TEST(UtilitiesTest, ReadFileLinesFailsOnDirectory)
{
    using namespace codebundler;

    // Create a temporary directory
    std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "codebundler_dir_test";
    std::filesystem::create_directories(temp_dir);

    // Trying to read a directory as a file should throw
    EXPECT_THROW(utilities::readFileLines(temp_dir), FileIOException);

    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

// Integration test: bundler should skip directories gracefully
TEST_F(BundlerGitTest, BundleSkipsDirectories)
{
    using namespace codebundler;

    // Create an empty directory
    std::filesystem::create_directory(test_repo_path / "empty_dir");

    // Manually add a file that lists this directory (simulating git ls-files returning it)
    // Since we can't easily make git ls-files return a directory, we'll test this
    // by verifying the bundler can handle it if it encounters one

    // For now, just verify that if a directory exists in the repo, it doesn't break bundling
    Options options;
    options.verbose = 1;
    Bundler bundler(options);
    std::stringstream output;

    // This should work - bundler will only process git-tracked files
    ASSERT_NO_THROW(bundler.bundleToStream(output));

    // The real test is in test_utilities - checking that directories cause an exception
    // The fix will make the bundler check for directories before calling readFileLines
}

TEST(UtilitiesTest, IsBinaryFileDetectsNulByte)
{
    using namespace codebundler;

    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "codebundler_binary_check";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    // UTF-8 text with a non-ASCII character is still text.
    std::ofstream(dir / "text.txt", std::ios::binary) << "plain text, utf-8: \xC3\xA9\n";
    {
        std::ofstream bin(dir / "data.bin", std::ios::binary);
        bin.write("Binary\0Data", 11);
    }

    EXPECT_FALSE(utilities::isBinaryFile(dir / "text.txt"));
    EXPECT_TRUE(utilities::isBinaryFile(dir / "data.bin"));
    EXPECT_THROW(utilities::isBinaryFile(dir / "no_such_file"), FileIOException);

    std::filesystem::remove_all(dir);
}

// The sniff window is git's: a NUL in the first 8000 bytes means binary; past
// the window the file counts as text (git stops reading there too).
TEST(UtilitiesTest, IsBinaryFileSniffsFirst8000Bytes)
{
    using namespace codebundler;

    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "codebundler_binary_window";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    const auto write_with_nul_at = [&dir](const std::string& name, std::size_t offset) {
        std::string content(offset, 'a');
        content.push_back('\0');
        content += "tail\n";
        std::ofstream out(dir / name, std::ios::binary);
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
    };

    write_with_nul_at("nul_on_last_byte_of_window.bin", 7999); // 8000th byte
    write_with_nul_at("nul_just_past_window.bin", 8000); // 8001st byte

    EXPECT_TRUE(utilities::isBinaryFile(dir / "nul_on_last_byte_of_window.bin"));
    EXPECT_FALSE(utilities::isBinaryFile(dir / "nul_just_past_window.bin"));

    std::filesystem::remove_all(dir);
}

// A text-only repository gets no skip note at all: the note is information,
// not boilerplate.
TEST(BundlerTextOnlyTest, NoSkipNoteWhenNothingIsBinary)
{
    using namespace codebundler;

    const std::filesystem::path repo_path = std::filesystem::temp_directory_path() / "codebundler_textonly_test_repo";
    cleanup_test_git_repo(repo_path);
    setup_test_git_repo(repo_path, false); // no binary file in this repo

    const std::string bundleContent = bundle_repo(repo_path);
    cleanup_test_git_repo(repo_path);

    EXPECT_EQ(bundleContent.find(codebundler::SKIPPED_BINARY_PREFIX), std::string::npos);
    EXPECT_NE(bundleContent.find(codebundler::FILENAME_PREFIX + "file1.txt"), std::string::npos);
}

// The 8000-byte window is a fast path, not the whole rule. A binary whose NUL
// sits past the window (a tar or a PDF with a long text prologue) also has no
// business being inlined: the bundle is a line-oriented text format, so any NUL
// that reaches it means raw bytes are being carried as if they were text.
TEST(BundlerLateNulTest, SkipsFileWithNulPastTheSniffWindow)
{
    using namespace codebundler;

    const std::filesystem::path repo_path = std::filesystem::temp_directory_path() / "codebundler_late_nul_test_repo";
    cleanup_test_git_repo(repo_path);
    setup_repo_with_files(repo_path,
        { { "notes.txt", "ordinary text\n" },
            { "archives/late_nul.tar", std::string(9000, 'A') + std::string("\0binary junk\0", 14) + std::string(200, '\x7f') } });

    const std::string bundleContent = bundle_repo(repo_path);
    cleanup_test_git_repo(repo_path);

    EXPECT_EQ(bundleContent.find(codebundler::FILENAME_PREFIX + "archives/late_nul.tar"), std::string::npos);
    EXPECT_NE(bundleContent.find(codebundler::SKIPPED_BINARY_PREFIX + "archives/late_nul.tar"), std::string::npos);
    EXPECT_NE(bundleContent.find(codebundler::FILENAME_PREFIX + "notes.txt"), std::string::npos);

    // Nothing binary gets through at all: the bundle is NUL-free, whatever the
    // tracked files hold.
    EXPECT_EQ(bundleContent.find('\0'), std::string::npos);
}

//--------------------------------------------------------------------------
// Selection: --exclude, --include and .codebundlerignore
//--------------------------------------------------------------------------

// Excluding a directory leaves its files out and names the rule, so nothing is
// dropped silently - the same contract the binary note has.
TEST(BundlerSelectionTest, ExcludingADirectoryNamesTheRuleInTheNote)
{
    const std::filesystem::path repo_path = std::filesystem::temp_directory_path() / "codebundler_selection_exclude_repo";
    cleanup_test_git_repo(repo_path);
    setup_selection_repo(repo_path);

    codebundler::Options options;
    options.excludePatterns = { "tests" };
    const std::string bundleContent = bundle_repo_with(repo_path, options);
    cleanup_test_git_repo(repo_path);

    EXPECT_NE(bundleContent.find(codebundler::FILENAME_PREFIX + "src/main.cpp"), std::string::npos);
    EXPECT_NE(bundleContent.find(codebundler::FILENAME_PREFIX + "docs/NOTES.md"), std::string::npos);
    EXPECT_EQ(bundleContent.find("tests/test_main.cpp"), std::string::npos);
    EXPECT_EQ(bundleContent.find("tests/test_util.cpp"), std::string::npos);
    EXPECT_NE(bundleContent.find(codebundler::EXCLUDED_NOTE_PREFIX + "2 files (tests)"), std::string::npos);
}

// A single excluded file reads "1 file", not "1 file(s)".
TEST(BundlerSelectionTest, TheNoteCountsInWholeWords)
{
    const std::filesystem::path repo_path = std::filesystem::temp_directory_path() / "codebundler_selection_one_repo";
    cleanup_test_git_repo(repo_path);
    setup_selection_repo(repo_path);

    codebundler::Options options;
    options.excludePatterns = { "docs" };
    const std::string bundleContent = bundle_repo_with(repo_path, options);
    cleanup_test_git_repo(repo_path);

    EXPECT_NE(bundleContent.find(codebundler::EXCLUDED_NOTE_PREFIX + "1 file (docs)"), std::string::npos);
}

// Naming anything with --include switches the mode: a bundle is then only what a
// rule admits, which is the shape a size problem is solved with.
TEST(BundlerSelectionTest, NamingAnIncludeSwitchesToIncludeMode)
{
    const std::filesystem::path repo_path = std::filesystem::temp_directory_path() / "codebundler_selection_include_repo";
    cleanup_test_git_repo(repo_path);
    setup_selection_repo(repo_path);

    codebundler::Options options;
    options.includePatterns = { "src" };
    const std::string bundleContent = bundle_repo_with(repo_path, options);
    cleanup_test_git_repo(repo_path);

    EXPECT_NE(bundleContent.find(codebundler::FILENAME_PREFIX + "src/main.cpp"), std::string::npos);
    EXPECT_NE(bundleContent.find(codebundler::FILENAME_PREFIX + "src/util.cpp"), std::string::npos);
    EXPECT_EQ(bundleContent.find("docs/NOTES.md"), std::string::npos);
    EXPECT_EQ(bundleContent.find("tests/test_main.cpp"), std::string::npos);
    // What nothing named is counted, not silently dropped.
    EXPECT_NE(bundleContent.find(codebundler::EXCLUDED_NOTE_PREFIX + "3 files (" + codebundler::EXCLUDED_BY_DEFAULT + ")"),
        std::string::npos);
}

// An include is the more specific statement, so it wins over an exclude however
// the two were ordered on the command line.
TEST(BundlerSelectionTest, AnIncludeOverridesAnExclude)
{
    const std::filesystem::path repo_path = std::filesystem::temp_directory_path() / "codebundler_selection_override_repo";
    cleanup_test_git_repo(repo_path);
    setup_selection_repo(repo_path);

    codebundler::Options options;
    options.excludePatterns = { "*" }; // every file in the tree
    options.includePatterns = { "src/main.cpp" }; // ...except this one
    const std::string bundleContent = bundle_repo_with(repo_path, options);
    cleanup_test_git_repo(repo_path);

    EXPECT_NE(bundleContent.find(codebundler::FILENAME_PREFIX + "src/main.cpp"), std::string::npos);
    EXPECT_EQ(bundleContent.find("src/util.cpp"), std::string::npos);
    EXPECT_EQ(bundleContent.find("tests/test_main.cpp"), std::string::npos);
    EXPECT_NE(bundleContent.find(codebundler::EXCLUDED_NOTE_PREFIX + "4 files (*)"), std::string::npos);
}

// The repository's own file supplies rules without any flag: the exclusion set
// is a property of the project, not of one invocation.
TEST(BundlerSelectionTest, TheSelectionFileInTheRepositoryRootIsHonoured)
{
    const std::filesystem::path repo_path = std::filesystem::temp_directory_path() / "codebundler_selection_file_repo";
    cleanup_test_git_repo(repo_path);
    setup_selection_repo(repo_path);
    write_ignore_file(repo_path, "# what this tree never bundles\ndocs/\ntests/test_util.cpp\n");

    const std::string bundleContent = bundle_repo(repo_path);
    cleanup_test_git_repo(repo_path);

    // No entry for either, though the note legitimately names the second rule.
    EXPECT_EQ(bundleContent.find(codebundler::FILENAME_PREFIX + "docs/NOTES.md"), std::string::npos);
    EXPECT_EQ(bundleContent.find(codebundler::FILENAME_PREFIX + "tests/test_util.cpp"), std::string::npos);
    EXPECT_NE(bundleContent.find(codebundler::FILENAME_PREFIX + "tests/test_main.cpp"), std::string::npos);
    EXPECT_NE(bundleContent.find(codebundler::EXCLUDED_NOTE_PREFIX + "1 file (docs/)"), std::string::npos);
    EXPECT_NE(bundleContent.find(codebundler::EXCLUDED_NOTE_PREFIX + "1 file (tests/test_util.cpp)"), std::string::npos);
}

// The command line overrides the repository's file: an include re-admits what
// the file removed.
TEST(BundlerSelectionTest, TheCommandLineOverridesTheSelectionFile)
{
    const std::filesystem::path repo_path = std::filesystem::temp_directory_path() / "codebundler_selection_cli_repo";
    cleanup_test_git_repo(repo_path);
    setup_selection_repo(repo_path);
    write_ignore_file(repo_path, "docs/\n");

    codebundler::Options options;
    options.includePatterns = { "docs/NOTES.md" }; // the file's rule said no
    const std::string bundleContent = bundle_repo_with(repo_path, options);
    cleanup_test_git_repo(repo_path);

    EXPECT_NE(bundleContent.find(codebundler::FILENAME_PREFIX + "docs/NOTES.md"), std::string::npos);
    EXPECT_EQ(bundleContent.find("src/main.cpp"), std::string::npos); // include mode: not named, not bundled
}

// The last test the file used to list as missing: the notes must be comments to
// unbundle as well, or an excluded tree would come back with junk entries.
TEST(BundlerSelectionTest, TheNotesAreCommentsWhenUnbundled)
{
    using namespace codebundler;

    const std::filesystem::path repo_path = std::filesystem::temp_directory_path() / "codebundler_selection_roundtrip_repo";
    cleanup_test_git_repo(repo_path);
    setup_repo_with_files(repo_path, { { "src/main.cpp", "int main() { return 0; }\n" }, { "tests/test_main.cpp", "TEST(Main, Runs) {}\n" } });

    Options options;
    options.excludePatterns = { "tests" };
    const std::string bundleContent = bundle_repo_with(repo_path, options);

    const std::filesystem::path out_dir = std::filesystem::temp_directory_path() / "codebundler_selection_roundtrip_out";
    std::filesystem::remove_all(out_dir);

    Options unbundleOptions;
    Unbundler unbundler(unbundleOptions);
    std::istringstream bundleStream(bundleContent);
    ASSERT_NO_THROW(unbundler.unbundleFromStream(bundleStream, out_dir));

    EXPECT_TRUE(std::filesystem::exists(out_dir / "src" / "main.cpp"));
    EXPECT_FALSE(std::filesystem::exists(out_dir / "tests")); // excluded, and not restored
    for (const auto& entry : std::filesystem::directory_iterator(out_dir)) {
        EXPECT_NE(entry.path().filename().string().rfind("#", 0), 0u) << "a note became a file";
    }

    cleanup_test_git_repo(repo_path);
    std::filesystem::remove_all(out_dir);
}

//--------------------------------------------------------------------------
// --list: the size question answered without writing a bundle
//--------------------------------------------------------------------------

TEST(BundlerListTest, ListReportsSizesAndWritesNoBundle)
{
    const std::filesystem::path repo_path = std::filesystem::temp_directory_path() / "codebundler_list_repo";
    cleanup_test_git_repo(repo_path);
    setup_repo_with_files(repo_path, { { "a.txt", "12345\n" }, // 6 bytes as bundled
                                         { "b.txt", "ab\n" }, // 3 bytes
                                         { "tests/test_a.cpp", "TEST(A, Runs) {}\n" } });

    codebundler::Options options;
    options.listOnly = true;
    options.excludePatterns = { "tests" };

    const std::filesystem::path original_cwd = std::filesystem::current_path();
    std::filesystem::current_path(repo_path);
    std::stringstream report;
    try {
        codebundler::Bundler bundler(options);
        bundler.listToStream(report);
    } catch (...) {
        std::filesystem::current_path(original_cwd);
        cleanup_test_git_repo(repo_path);
        throw;
    }
    std::filesystem::current_path(original_cwd);
    cleanup_test_git_repo(repo_path);

    const std::string listing = report.str();
    EXPECT_NE(listing.find("a.txt"), std::string::npos);
    EXPECT_NE(listing.find("b.txt"), std::string::npos);
    EXPECT_EQ(listing.find("tests/test_a.cpp"), std::string::npos);
    EXPECT_NE(listing.find("Total: 2 files, 9 bytes of content"), std::string::npos);
    EXPECT_NE(listing.find("Excluded: 1 file (tests)"), std::string::npos);
    // A report, not a bundle: no separator and no entry headers in it.
    EXPECT_EQ(listing.find(options.separator), std::string::npos);
    EXPECT_EQ(listing.find(codebundler::FILENAME_PREFIX), std::string::npos);
}

// A binary file is counted, not inlined: the list must agree with what a bundle
// would actually carry.
TEST(BundlerListTest, ListCountsBinariesSeparately)
{
    const std::filesystem::path repo_path = std::filesystem::temp_directory_path() / "codebundler_list_binary_repo";
    cleanup_test_git_repo(repo_path);
    setup_test_git_repo(repo_path); // holds file1.txt and a binary subdir/file2.bin

    codebundler::Options options;
    options.listOnly = true;

    const std::filesystem::path original_cwd = std::filesystem::current_path();
    std::filesystem::current_path(repo_path);
    std::stringstream report;
    try {
        codebundler::Bundler bundler(options);
        bundler.listToStream(report);
    } catch (...) {
        std::filesystem::current_path(original_cwd);
        cleanup_test_git_repo(repo_path);
        throw;
    }
    std::filesystem::current_path(original_cwd);
    cleanup_test_git_repo(repo_path);

    const std::string listing = report.str();
    EXPECT_NE(listing.find("file1.txt"), std::string::npos);
    EXPECT_EQ(listing.find("subdir/file2.bin"), std::string::npos);
    EXPECT_NE(listing.find("Not inlined: 1 file, binary"), std::string::npos);
}

// Add more tests:
// - Bundling an empty repository
// - Bundling when git command fails (might need mocking or more complex setup)
// - Error handling for unreadable files
