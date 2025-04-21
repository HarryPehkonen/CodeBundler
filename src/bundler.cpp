#include "bundler.hpp"
#include "exceptions.hpp"
#include "utilities.hpp"
#include <filesystem> // For path manipulation
#include <fstream>
#include <iostream> // For std::cerr, std::cout

namespace codebundler {

//--------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------
Bundler::Bundler(Options options)
    : m_options(options)
{
    if (m_options.separator.empty()) {
        // Prevent empty separator which would break parsing
        throw std::invalid_argument("Bundler separator cannot be empty.");
    }
}

//--------------------------------------------------------------------------
// Public Methods
//--------------------------------------------------------------------------

/**
 * @brief Bundles files tracked by `git ls-files` into the provided output stream.
 */
void Bundler::bundleToStream(std::ostream& outputStream, const std::string& description)
{
    m_options.verbose > 0 && std::cerr << "Gathering files tracked by Git..." << std::endl;
    std::vector<std::string> filesToBundle = utilities::getGitTrackedFiles();
    m_options.verbose > 0 && std::cerr << "Found " << filesToBundle.size() << " files." << std::endl;

    if (filesToBundle.empty()) {
        m_options.verbose > 0 && std::cerr << "Warning: No files found by 'git ls-files'. Bundle will be empty." << std::endl;
    }

    writeHeader(outputStream, description);

    for (const auto& filePath : filesToBundle) {
        m_options.verbose > 0 && std::cerr << "Bundling: " << filePath << std::endl;
        try {
            writeFileEntry(outputStream, filePath);
        } catch (const FileIOException& e) {
            std::cerr << "Error processing file '" << filePath << "': " << e.what() << ". Aborting." << std::endl;
            throw; // Re-throw to signal failure
        }
    }
    m_options.verbose > 0 && std::cerr << "Bundle creation finished." << std::endl;
}

/**
 * @brief Bundles files tracked by `git ls-files` into a specified file.
 */
void Bundler::bundleToFile(const std::string& outputFilePath, const std::string& description)
{
    std::ofstream outputFileStream(outputFilePath, std::ios::binary | std::ios::trunc);
    if (!outputFileStream) {
        throw FileIOException("Failed to open output bundle file for writing", outputFilePath);
    }
    m_options.verbose > 0 && std::cerr << "Writing bundle to: " << outputFilePath << std::endl;
    bundleToStream(outputFileStream, description);
}

//--------------------------------------------------------------------------
// Private Methods
//--------------------------------------------------------------------------

/**
 * @brief Writes the bundle header to the output stream.
 */
void Bundler::writeHeader(std::ostream& outputStream, const std::string& description)
{
    outputStream << m_options.separator << "\n";
    if (!description.empty()) {
        outputStream << "Description: " << description << "\n";
        outputStream << m_options.separator << "\n";
    }
    // Add more header info if needed (e.g., timestamp, tool version)
}

/**
 * @brief Writes a single file entry (header and content) to the output stream.
 */
void Bundler::writeFileEntry(std::ostream& outputStream, const std::string& filePath)
{
    // Use filesystem::path for potentially better path handling, but keep string for git output
    std::filesystem::path fsPath(filePath);
    std::vector<std::string> fileLines = utilities::readFileLines(fsPath);
    if (utilities::fileContainsDelimiter(fileLines, m_options.separator)) {
        throw CodeBundlerException("File contains the bundle separator, which is not allowed.");
    }
    std::string fileContent = utilities::linesToString(fileLines);
    std::string checksum = utilities::calculateSHA256(fileContent);

    // Normalize path separators for consistency in the bundle? Optional.
    // std::string normalizedPath = fsPath.generic_string(); // Use forward slashes

    outputStream << "Filename: " << filePath << "\n"; // Keep original path from git
    outputStream << "Checksum: SHA256:" << checksum << "\n";
    outputStream << fileContent; // Write content directly
    // Ensure a newline separates content from the next separator if content doesn't end with one
    if (!fileContent.empty() && fileContent.back() != '\n') {
        outputStream << "\n";
    }
    outputStream << m_options.separator << "\n";

    // Check for stream errors after writing
    if (!outputStream) {
        throw FileIOException("Stream error occurred while writing entry for file", filePath);
    }
}

} // namespace codebundler
