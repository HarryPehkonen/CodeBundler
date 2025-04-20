#ifndef CODEBUNDLER_UTILITIES_HPP
#define CODEBUNDLER_UTILITIES_HPP

#include <filesystem> // Requires C++17
#include <string>
#include <vector>

namespace codebundler {
namespace utilities {

    /**
     * @brief Reads the entire content of a file into a string.
     * @param filepath The path to the file.
     * @return The content of the file as a string.
     * @throws FileIOException If the file cannot be opened or read.
     */
    std::vector<std::string> readFileLines(const std::filesystem::path& filepath);

    std::string readFileContent(const std::filesystem::path& filepath);

    /**
     * @brief Checks if a file contains a specific delimiter.
     * @param lines The lines of the file as a vector of strings.
     * @param delimiter The delimiter to check for.
     * @return true if the file contains the delimiter, false otherwise.
     */
    bool fileContainsDelimiter(const std::vector<std::string>& lines, const std::string& delimiter);

    /**
     * @brief Converts a vector of strings to a single string, joining
     * them with newlines.
     * @param lines The vector of strings to convert.
     * @return The joined string.
     */
    std::string linesToString(const std::vector<std::string>& lines);

    /**
     * @brief Writes content to a file, creating directories if necessary.
     * @param filepath The path to the file to write.
     * @param content The string content to write.
     * @throws FileIOException If the file cannot be opened or written to, or if directories cannot be created.
     */
    void writeFileContent(const std::filesystem::path& filepath, const std::string& content);

    /**
     * @brief Executes a shell command and captures its standard output.
     * @param command The command to execute.
     * @return A pair containing the exit code and the captured standard output.
     * @throws std::runtime_error If the command cannot be executed (e.g., popen fails).
     */
    std::pair<int, std::string> executeCommand(const std::string& command);

    /**
     * @brief Retrieves a list of files tracked by Git using `git ls-files`.
     * @return A vector of strings, each representing a tracked file path relative to the repository root.
     * @throws GitCommandException If the `git ls-files` command fails or returns a non-zero exit code.
     * @throws std::runtime_error If the Git command cannot be executed.
     */
    std::vector<std::string> getGitTrackedFiles();

    /**
     * @brief Calculates the SHA-256 hash of a string.
     * @param content The string content to hash.
     * @return The SHA-256 hash represented as a hexadecimal string.
     */
    std::string calculateSHA256(const std::string& content);

    /**
     * @brief Trims leading and trailing whitespace from a string.
     * @param str The string to trim.
     * @return The trimmed string.
     */
    std::string trim(const std::string& str);

    /**
     * @brief Checks if a string starts with a specific prefix.
     * @param str The string to check.
     * @param prefix The prefix to look for.
     * @return True if the string starts with the prefix, false otherwise.
     */
    bool startsWith(const std::string& str, const std::string& prefix);

} // namespace utilities
} // namespace codebundler

#endif // CODEBUNDLER_UTILITIES_HPP
