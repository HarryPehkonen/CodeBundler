#ifndef CODEBUNDLER_EXCEPTIONS_HPP
#define CODEBUNDLER_EXCEPTIONS_HPP

#include <stdexcept>
#include <string>

namespace codebundler {

/**
 * @brief Base class for all CodeBundler specific exceptions.
 */
class CodeBundlerException : public std::runtime_error {
public:
    /**
     * @brief Constructs a CodeBundlerException.
     * @param message The error message.
     */
    explicit CodeBundlerException(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

/**
 * @brief Exception thrown for errors related to file I/O operations.
 */
class FileIOException : public CodeBundlerException {
public:
    /**
     * @brief Constructs a FileIOException.
     * @param message The error message.
     * @param path The path of the file involved in the error (optional).
     */
    FileIOException(const std::string& message, const std::string& path = "")
        : CodeBundlerException(path.empty() ? message : message + ": " + path)
    {
    }
};

/**
 * @brief Exception thrown for errors during Git command execution.
 */
class GitCommandException : public CodeBundlerException {
public:
    /**
     * @brief Constructs a GitCommandException.
     * @param message The error message (e.g., command output or failure reason).
     * @param command The Git command that failed (optional).
     */
    GitCommandException(const std::string& message, const std::string& command = "")
        : CodeBundlerException(command.empty() ? message : "Git command failed: '" + command + "' - " + message)
    {
    }
};

/**
 * @brief Exception thrown for errors related to the bundle file format.
 */
class BundleFormatException : public CodeBundlerException {
public:
    /**
     * @brief Constructs a BundleFormatException.
     * @param message The error message describing the format violation.
     */
    explicit BundleFormatException(const std::string& message)
        : CodeBundlerException("Bundle format error: " + message)
    {
    }
};

/**
 * @brief Exception thrown for checksum verification failures.
 */
class ChecksumMismatchException : public CodeBundlerException {
public:
    /**
     * @brief Constructs a ChecksumMismatchException.
     * @param filename The name of the file with the checksum mismatch.
     * @param expected The expected checksum.
     * @param actual The actual calculated checksum.
     */
    ChecksumMismatchException(const std::string& filename, const std::string& expected, const std::string& actual)
        : CodeBundlerException("Checksum mismatch for file '" + filename + "'. Expected: " + expected + ", Actual: " + actual)
    {
    }
};

/**
 * @brief Exception thrown for invalid command line arguments.
 */
class ArgumentParserException : public CodeBundlerException {
public:
    /**
     * @brief Constructs an ArgumentParserException.
     * @param message The error message describing the argument issue.
     */
    explicit ArgumentParserException(const std::string& message)
        : CodeBundlerException("Invalid arguments: " + message)
    {
    }
};

} // namespace codebundler

#endif // CODEBUNDLER_EXCEPTIONS_HPP
