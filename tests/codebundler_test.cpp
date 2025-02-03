#include <gtest/gtest.h>
#include "codebundler/codebundler.hpp"
#include "codebundler/processpipe.hpp"
#include <sstream>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class CodeBundlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Verify clean state
        ASSERT_FALSE(fs::exists("test_files")) << "test_files directory already exists";
        ASSERT_FALSE(fs::exists("test_unbundle")) << "test_unbundle directory already exists";
        
        // Create directory and verify
        ASSERT_TRUE(fs::create_directories("test_files/subdir")) << "Failed to create test directories";
        ASSERT_TRUE(fs::exists("test_files/subdir")) << "Directory creation verification failed";
        
        // Create and verify each file
        std::vector<std::pair<std::string, std::string>> test_data = {
            {"test_files/file1.txt", "Content of file 1\nSecond line\n"},
            {"test_files/file2.txt", "Content of file 2\n"},
            {"test_files/subdir/file3.txt", "Content in subdirectory\n"}
        };
        
        for (const auto& [path, content] : test_data) {
            std::ofstream file(path);
            ASSERT_TRUE(file.is_open()) << "Failed to create " << path;
            file << content;
            file.close();
            ASSERT_TRUE(fs::exists(path)) << "File creation verification failed: " << path;
            
            // Verify content
            std::string read_content = readFile(path);
            ASSERT_EQ(read_content, content) << "Content verification failed for " << path;
        }

        test_files = {"test_files/file1.txt",
                     "test_files/file2.txt",
                     "test_files/subdir/file3.txt"};
    }

    void TearDown() override {
        // Clean up test files and directories
        fs::remove_all("test_files");
        fs::remove_all("test_unbundle");
        fs::remove("test_bundle.txt");
    }

    std::string readFile(const std::string& path) {
        std::ifstream file(path);
        EXPECT_TRUE(file.is_open()) << "Failed to open: " << path 
                                   << " errno: " << errno;
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        EXPECT_FALSE(file.fail()) << "Read failed for: " << path;
        return content;
    }

    std::vector<std::string> test_files;
};

TEST_F(CodeBundlerTest, BundleToStringStream) {
    std::stringstream output;
    EXPECT_NO_THROW(codebundler::CodeBundler::bundle(output, test_files));
    
    std::string result = output.str();
    EXPECT_TRUE(result.find("Filename: ") != std::string::npos);
    EXPECT_TRUE(result.find("Content of file 1") != std::string::npos);
    EXPECT_TRUE(result.find("Content of file 2") != std::string::npos);
    EXPECT_TRUE(result.find("Content in subdirectory") != std::string::npos);
}

TEST_F(CodeBundlerTest, UnbundleFromStringStream) {
    std::stringstream bundle_writer;
    ASSERT_NO_THROW(codebundler::CodeBundler::bundle(bundle_writer, test_files));
    
    // Diagnostic: Print bundle content
    std::string bundle_content = bundle_writer.str();
    std::cout << "\nBundle content:\n---START---\n" << bundle_content << "\n---END---\n";
    
    // Create new reader stream
    std::stringstream bundle_reader(bundle_content);
    
    // Diagnostic: Track unbundle progress
    bool unbundle_result = false;
    try {
        codebundler::CodeBundler::unbundle(bundle_reader, "test_unbundle");
        unbundle_result = true;
    } catch (const std::exception& e) {
        std::cout << "Exception during unbundle: " << e.what() << "\n";
    }
    std::cout << "Unbundle completed: " << (unbundle_result ? "success" : "failure") << "\n";
    
    // Diagnostic: Check directory state
    std::cout << "Directory exists: " << fs::exists("test_unbundle/test_files") << "\n";
    std::cout << "File should be at: " << fs::absolute("test_unbundle/test_files/file1.txt") << "\n";
}

TEST_F(CodeBundlerTest, BundleToFile) {
    EXPECT_NO_THROW(codebundler::CodeBundler::bundle("test_bundle.txt", test_files));
    EXPECT_TRUE(fs::exists("test_bundle.txt"));
    
    std::string content = readFile("test_bundle.txt");
    EXPECT_TRUE(content.find("Content of file 1") != std::string::npos);
}

TEST_F(CodeBundlerTest, UnbundleFromFile) {
    // First create a bundle file
    ASSERT_NO_THROW(codebundler::CodeBundler::bundle("test_bundle.txt", test_files));
    
    // Diagnostic: Read and display bundle file content
    {
        std::ifstream check_content("test_bundle.txt", std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(check_content)),
                          std::istreambuf_iterator<char>());
        std::cout << "\nFile content:\n---START---\n" << content << "\n---END---\n";
    }
    
    // Then unbundle it with explicit binary mode
    bool unbundle_result = false;
    try {
        std::ifstream input("test_bundle.txt", std::ios::binary);
        codebundler::CodeBundler::unbundle(input, "test_unbundle");
        unbundle_result = true;
    } catch (const std::exception& e) {
        std::cout << "Exception during unbundle: " << e.what() << "\n";
    }
    std::cout << "Unbundle completed: " << (unbundle_result ? "success" : "failure") << "\n";
    
    // Diagnostic: Check file state
    std::cout << "Output path exists: " << fs::exists("test_unbundle/test_files/file1.txt") << "\n";
    if (fs::exists("test_unbundle/test_files/file1.txt")) {
        std::ifstream check_output("test_unbundle/test_files/file1.txt");
        std::string content((std::istreambuf_iterator<char>(check_output)),
                          std::istreambuf_iterator<char>());
        std::cout << "Output content:\n" << content << "\n";
    }
}

TEST_F(CodeBundlerTest, BundleToInvalidPath) {
    EXPECT_THROW(codebundler::CodeBundler::bundle("/nonexistent/path/bundle.txt", test_files),
                 codebundler::BundleError);
}

TEST_F(CodeBundlerTest, UnbundleFromInvalidFile) {
    EXPECT_THROW(codebundler::CodeBundler::unbundle("nonexistent.txt"),
                 codebundler::BundleError);
}

TEST_F(CodeBundlerTest, UnbundleToInvalidPath) {
    // First create a valid bundle
    ASSERT_NO_THROW(codebundler::CodeBundler::bundle("test_bundle.txt", test_files));
    
    // Then try to unbundle to an invalid path
    EXPECT_THROW(codebundler::CodeBundler::unbundle("test_bundle.txt", "/nonexistent/path"),
                 codebundler::BundleError);
}

TEST_F(CodeBundlerTest, BundleWithCustomSeparator) {
    std::stringstream output;
    std::string custom_separator = "===CUSTOM===";
    EXPECT_NO_THROW(codebundler::CodeBundler::bundle(output, test_files, custom_separator));
    
    std::string result = output.str();
    EXPECT_TRUE(result.find(custom_separator) != std::string::npos);
}

TEST_F(CodeBundlerTest, HandleFailedStreamOperations) {
    // Test with a stream that fails on write
    struct FailingStream : std::ostream {
        struct FailingBuf : std::stringbuf {
            int overflow(int) override { return std::char_traits<char>::eof(); }
        } buf;
        FailingStream() : std::ostream(&buf) {}
    } failing_stream;
    
    EXPECT_THROW(codebundler::CodeBundler::bundle(failing_stream, test_files),
                 codebundler::BundleError);
}

TEST_F(CodeBundlerTest, VerifyModifiedBundle) {
    // Create initial bundle
    std::stringstream bundle;
    ASSERT_NO_THROW(codebundler::CodeBundler::bundle(bundle, test_files));
    
    // Modify and create verification stream
    std::string modified_content = bundle.str();
    std::cout << "\nOriginal bundle:\n" << modified_content << "\n";
    
    modified_content.replace(modified_content.find("Content of file 1"),
                           strlen("Content of file 1"),
                           "Modified content");
    std::cout << "\nModified bundle:\n" << modified_content << "\n";
    
    // Compare original and modified content explicitly
    std::cout << "\nExplicit content comparison:\n";
    {
        std::ifstream orig("test_files/file1.txt");
        std::string orig_content((std::istreambuf_iterator<char>(orig)),
                               std::istreambuf_iterator<char>());
        std::cout << "Original file content: '" << orig_content << "'\n";
        
        size_t content_pos = modified_content.find("Modified content");
        size_t end_pos = modified_content.find("---------- BOUNDARY ----------", content_pos);
        std::string mod_content = modified_content.substr(content_pos, end_pos - content_pos);
        std::cout << "Modified section: '" << mod_content << "'\n";
    }
    
    std::stringstream verify_stream(modified_content);
    EXPECT_FALSE(codebundler::CodeBundler::verify(verify_stream));
}

TEST_F(CodeBundlerTest, VerifyEmptyFileList) {
    std::vector<std::string> empty_files;
    std::stringstream output;
    EXPECT_NO_THROW(codebundler::CodeBundler::bundle(output, empty_files));
}

TEST_F(CodeBundlerTest, BundleWithInvalidSeparator) {
    std::stringstream output;
    EXPECT_THROW(codebundler::CodeBundler::bundle(output, test_files, ""),
                 codebundler::BundleError);
    EXPECT_THROW(codebundler::CodeBundler::bundle(output, test_files, "sep\nwith\nnewline"),
                 codebundler::BundleError);
}

TEST(ProcessPipeTest, HandleInvalidCommand) {
    try {
        codebundler::ProcessPipe pipe("nonexistentcommand");
        auto lines = pipe.readLines();  // Force execution
        ADD_FAILURE() << "Expected exception, got " << lines.size() << " lines";
        for (const auto& line : lines) {
            std::cout << "Output line: '" << line << "'" << std::endl;
        }
    } catch (const std::runtime_error& e) {
        std::cout << "Exception message: " << e.what() << std::endl;
        SUCCEED();
    }
}

TEST(ProcessPipeTest, ExecuteValidCommand) {
    codebundler::ProcessPipe pipe("echo test");
    auto lines = pipe.readLines();
    ASSERT_EQ(lines.size(), 1);
    ASSERT_EQ(lines[0], "test");
}

TEST(ProcessPipeTest, HandleMultilineOutput) {
    codebundler::ProcessPipe pipe("printf 'line1\\nline2\\nline3'");
    auto lines = pipe.readLines();
    ASSERT_EQ(lines.size(), 3);
    ASSERT_EQ(lines[0], "line1");
    ASSERT_EQ(lines[1], "line2");
    ASSERT_EQ(lines[2], "line3");
}
