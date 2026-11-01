#include "bundler.hpp"
#include "constants.hpp"
#include "exceptions.hpp"
#include "utilities.hpp"
#include <filesystem> // For path manipulation
#include <fstream>
#include <iostream> // For std::cerr, std::cout

namespace codebundler {

//--------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------
Bundler::Bundler(const Options& options)
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

    std::vector<std::string> skippedBinaries;
    for (const auto& filePath : filesToBundle) {
        m_options.verbose > 0 && std::cerr << "Bundling: " << filePath << std::endl;
        try {
            if (writeFileEntry(outputStream, filePath) == EntryOutcome::skippedBinary) {
                skippedBinaries.push_back(filePath);
            }
        } catch (const FileIOException& e) {
            std::cerr << "Error processing file '" << filePath << "': " << e.what() << ". Aborting." << std::endl;
            throw; // Re-throw to signal failure
        }
    }
    writeSkippedNote(outputStream, skippedBinaries);
    if (!skippedBinaries.empty()) {
        m_options.verbose > 0 && std::cerr << "Skipped " << skippedBinaries.size() << " binary file(s); they are named at the end of the bundle." << std::endl;
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
Bundler::EntryOutcome Bundler::writeFileEntry(std::ostream& outputStream, const std::string& filePath)
{
    // Use filesystem::path for potentially better path handling, but keep string for git output
    std::filesystem::path fsPath(filePath);

    // Skip directories (e.g., gitlinks/submodules tracked by git ls-files)
    if (std::filesystem::is_directory(fsPath)) {
        if (m_options.verbose > 0) {
            std::cerr << "Skipping directory: " << filePath << std::endl;
        }
        return EntryOutcome::skippedDirectory;
    }

    // Skip binary files: they do not survive a line-based, separator-delimited
    // format (the content is mangled on the way in and a restored file is not
    // byte-identical), and arbitrary bytes can look like bundle structure.
    if (utilities::isBinaryFile(fsPath)) {
        if (m_options.verbose > 0) {
            std::cerr << "Skipping binary file: " << filePath << std::endl;
        }
        return EntryOutcome::skippedBinary;
    }

    std::vector<std::string> fileLines = utilities::readFileLines(fsPath);
    std::string fileContent = utilities::linesToString(fileLines);

    // The 8000-byte window is only a fast path: a binary can carry a long text
    // prologue (a tar, a PDF, a container format) and put its first NUL past the
    // window. Since the whole file is in hand by now, check all of it - one NUL
    // is enough to make this "not text", and the format here is line-oriented
    // text. This is deliberately stricter than git, which only sniffs the window.
    if (fileContent.find('\0') != std::string::npos) {
        if (m_options.verbose > 0) {
            std::cerr << "Skipping binary file (NUL past the " << codebundler::BINARY_SNIFF_BYTES
                      << "-byte sniff window): " << filePath << std::endl;
        }
        return EntryOutcome::skippedBinary;
    }

    if (utilities::fileContainsDelimiter(fileLines, m_options.separator)) {
        throw CodeBundlerException("File contains the bundle separator, which is not allowed.");
    }
    std::string checksum = utilities::calculateSHA256(fileContent);

    // Normalize path separators for consistency in the bundle? Optional.
    // std::string normalizedPath = fsPath.generic_string(); // Use forward slashes

    outputStream << codebundler::FILENAME_PREFIX << filePath << "\n"; // Keep original path from git
    outputStream << codebundler::CHECKSUM_PREFIX << checksum << "\n";
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

    return EntryOutcome::written;
}

/**
 * @brief Writes the "left out" note: one comment line per binary file.
 */
void Bundler::writeSkippedNote(std::ostream& outputStream, const std::vector<std::string>& skippedBinaries)
{
    if (skippedBinaries.empty()) {
        return; // nothing was left out, so there is nothing to explain
    }
    for (const auto& filePath : skippedBinaries) {
        outputStream << codebundler::SKIPPED_BINARY_PREFIX << filePath << "\n";
    }
    if (!outputStream) {
        throw FileIOException("Stream error occurred while writing the skipped-binary note", codebundler::SKIPPED_BINARY_PREFIX);
    }
}

} // namespace codebundler
