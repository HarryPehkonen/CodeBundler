#include "utilities.hpp"
#include "exceptions.hpp"
#include <array> // For buffer in executeCommand
#include <cstdio> // For popen, pclose
#include <fstream>
#include <iostream> // For cerr
#include <memory> // For unique_ptr with custom deleter
#include <picosha2.h> // Include PicoSHA2 header
#include <sstream>
#include <system_error> // For filesystem errors

namespace codebundler {
namespace utilities {

    /**
     * @brief Reads a file line by line into a vector of strings.
     * Newline characters are removed from the end of each line.
     * Uses RAII for file handling. Opens in text mode.
     * @param filepath The path to the file.
     * @return A vector where each element is a line from the file.
     * @throws FileIOException If the file cannot be opened or a read error occurs.
     */
    std::vector<std::string> readFileLines(const std::filesystem::path& filepath)
    {
        // Open in text mode (default for ifstream)
        std::ifstream fileStream(filepath);
        if (!fileStream) {
            throw FileIOException("Failed to open file for reading lines", filepath.string());
        }

        std::vector<std::string> lines;
        std::string currentLine;

        // Read lines one by one
        while (std::getline(fileStream, currentLine)) {
            // std::getline already removes the newline character(s)
            lines.push_back(currentLine);
        }

        // Check for errors *after* the loop (excluding EOF)
        // getline sets failbit on EOF *if* nothing was read, but eofbit is the primary indicator.
        // badbit indicates a more serious I/O error.
        if (fileStream.bad()) {
            throw FileIOException("Failed during reading lines from file", filepath.string());
        }
        // No need to check failbit specifically unless you need to distinguish
        // between reaching EOF successfully vs. a format error (unlikely here).
        // The loop condition already handles normal EOF.

        return lines;
    }

    std::string readFileContent(const std::filesystem::path& filepath)
    {
        std::vector<std::string> lines = readFileLines(filepath);
        return linesToString(lines);
    }
    /**
     * @brief Checks if a file contains a specific delimiter.
     * @param lines The lines of the file as a vector of strings.
     * @param delimiter The delimiter to check for.
     * @return true if the file contains the delimiter, false otherwise.
     */
    bool fileContainsDelimiter(const std::vector<std::string>& lines, const std::string& delimiter)
    {
        for (const auto& line : lines) {
            if (line == delimiter) {
                return true; // Delimiter found
            }
        }
        return false; // Delimiter not found
    }

    /**
     * @brief Converts a vector of strings to a single string, joining them with newlines.
     * @param lines The vector of strings to convert.
     * @return The joined string.
     */
    std::string linesToString(const std::vector<std::string>& lines)
    {
        std::ostringstream oss;
        for (const auto& line : lines) {
            oss << line << '\n'; // Append newline after each line
        }
        return oss.str();
    }

    /**
     * @brief Writes content to a file, creating directories if necessary.
     * Uses RAII for file handling.
     * @param filepath The path to the file to write.
     * @param content The string content to write.
     * @throws FileIOException If the file cannot be opened or written to, or if directories cannot be created.
     */
    void writeFileContent(const std::filesystem::path& filepath, const std::string& content)
    {
        // Create parent directories if they don't exist
        if (filepath.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(filepath.parent_path(), ec);
            if (ec) {
                throw FileIOException("Failed to create directories for file: " + ec.message(), filepath.string());
            }
        }

        // Open file in binary mode to write content correctly
        std::ofstream fileStream(filepath, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!fileStream) {
            throw FileIOException("Failed to open file for writing", filepath.string());
        }

        fileStream.write(content.data(), content.size());

        if (fileStream.fail()) {
            throw FileIOException("Failed to write content to file", filepath.string());
        }

        // ofstream destructor handles closing
    }

    /**
     * @brief Executes a shell command and captures its standard output.
     * Uses popen/pclose for process management.
     * @param command The command to execute.
     * @return A pair containing the exit code and the captured standard output.
     * @throws std::runtime_error If the command cannot be executed (e.g., popen fails).
     */
    std::pair<int, std::string> executeCommand(const std::string& command)
    {
        // Custom deleter for FILE* from popen
        auto pipe_deleter = [](FILE* p) { if (p) pclose(p); };
        using unique_pipe_ptr = std::unique_ptr<FILE, decltype(pipe_deleter)>;

#ifdef _WIN32
        // Use _popen on Windows
        unique_pipe_ptr pipe(_popen(command.c_str(), "r"), pipe_deleter);
#else
        // Use popen on POSIX systems
        unique_pipe_ptr pipe(popen(command.c_str(), "r"), pipe_deleter);
#endif

        if (!pipe) {
            throw std::runtime_error("Failed to execute command: popen() failed.");
        }

        std::stringstream output;
        std::array<char, 256> buffer;
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            output << buffer.data();
        }

// pclose returns the exit status of the command
#ifdef _WIN32
        int exit_code = _pclose(pipe.release()); // Release ownership before _pclose
#else
        int exit_code = pclose(pipe.release()); // Release ownership before pclose
#endif

// On POSIX, WEXITSTATUS extracts the actual exit code
#ifndef _WIN32
        if (WIFEXITED(exit_code)) {
            exit_code = WEXITSTATUS(exit_code);
        } else {
            // Handle cases where the process terminated abnormally (e.g., signal)
            exit_code = -1; // Indicate abnormal termination
        }
#endif
        // Note: Windows _pclose directly returns the exit code, or -1 on error.

        return { exit_code, output.str() };
    }

    /**
     * @brief Retrieves a list of files tracked by Git using `git ls-files`.
     * @return A vector of strings, each representing a tracked file path relative to the repository root.
     * @throws GitCommandException If the `git ls-files` command fails or returns a non-zero exit code.
     * @throws std::runtime_error If the Git command cannot be executed.
     */
    std::vector<std::string> getGitTrackedFiles()
    {
        const std::string command = "git ls-files";
        auto [exit_code, output] = executeCommand(command);

        if (exit_code != 0) {
            throw GitCommandException("`git ls-files` exited with code " + std::to_string(exit_code) + ". Output: " + output, command);
        }

        std::vector<std::string> files;
        std::stringstream ss(output);
        std::string line;
        while (std::getline(ss, line)) {
            // Trim whitespace, especially trailing newline/carriage return
            line = trim(line);
            if (!line.empty()) {
                files.push_back(line);
            }
        }
        return files;
    }

    /**
     * @brief Calculates the SHA-256 hash of a string.
     * @param content The string content to hash.
     * @return The SHA-256 hash represented as a hexadecimal string.
     */
    std::string calculateSHA256(const std::string& content)
    {
        return picosha2::hash256_hex_string(content);
    }

    /**
     * @brief Trims leading and trailing whitespace from a string.
     * @param str The string to trim.
     * @return The trimmed string.
     */
    std::string trim(const std::string& str)
    {
        const std::string whitespace = " \t\n\r\f\v";
        size_t first = str.find_first_not_of(whitespace);
        if (first == std::string::npos) {
            return ""; // String contains only whitespace
        }
        size_t last = str.find_last_not_of(whitespace);
        return str.substr(first, (last - first + 1));
    }

    /**
     * @brief Checks if a string starts with a specific prefix.
     * @param str The string to check.
     * @param prefix The prefix to look for.
     * @return True if the string starts with the prefix, false otherwise.
     */
    bool startsWith(const std::string& str, const std::string& prefix)
    {
        return str.rfind(prefix, 0) == 0;
    }

} // namespace utilities
} // namespace codebundler
