#include "unbundler.hpp"
#include "exceptions.hpp"
#include "utilities.hpp"
#include <filesystem> // Requires C++17
#include <fstream>
#include <iostream> // For std::cout, std::cerr
#include <sstream>

namespace codebundler {

//--------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------
Unbundler::Unbundler(Options options)
    : m_options(std::move(options))
{
    if (m_options.separator.empty()) {
        throw std::invalid_argument("Unbundler separator cannot be empty.");
    }
}

//--------------------------------------------------------------------------
// Public Methods
//--------------------------------------------------------------------------

/**
 * @brief Unbundles files from the provided input stream into a target directory.
 */
void Unbundler::unbundleFromStream(std::istream& inputStream, const std::filesystem::path& outputDirectory)
{
    std::cout << "Starting unbundle process..." << std::endl;
    if (!std::filesystem::exists(outputDirectory)) {
        std::cout << "Output directory does not exist, attempting to create: " << outputDirectory << std::endl;
        std::error_code ec;
        std::filesystem::create_directories(outputDirectory, ec);
        if (ec) {
            throw FileIOException("Failed to create output directory: " + ec.message(), outputDirectory.string());
        }
    } else if (!std::filesystem::is_directory(outputDirectory)) {
        throw FileIOException("Output path exists but is not a directory", outputDirectory.string());
    }

    processBundle(inputStream, outputDirectory, false); // false = not verifyOnly
    std::cout << "Unbundle process finished." << std::endl;
}

/**
 * @brief Unbundles files from a specified bundle file into a target directory.
 */
void Unbundler::unbundleFromFile(const std::string& inputFilePath, const std::filesystem::path& outputDirectory)
{
    std::ifstream inputFileStream(inputFilePath, std::ios::binary);
    if (!inputFileStream) {
        throw FileIOException("Failed to open input bundle file for reading", inputFilePath);
    }
    std::cout << "Reading bundle from: " << inputFilePath << std::endl;
    unbundleFromStream(inputFileStream, outputDirectory);
}

/**
 * @brief Verifies the integrity of a bundle by checking checksums without extracting files.
 */
bool Unbundler::verifyBundleStream(std::istream& inputStream)
{
    std::cout << "Starting bundle verification..." << std::endl;
    // Pass a dummy path, it won't be used in verifyOnly mode
    bool success = processBundle(inputStream, ".", true); // true = verifyOnly
    if (success) {
        std::cout << "Bundle verification successful: All checksums match." << std::endl;
    } else {
        // Specific error already thrown by processBundle if a mismatch occurred
        std::cerr << "Bundle verification failed." << std::endl;
    }
    return success;
}

/**
 * @brief Verifies the integrity of a bundle file by checking checksums without extracting files.
 */
bool Unbundler::verifyBundleFile(const std::string& inputFilePath)
{
    std::ifstream inputFileStream(inputFilePath, std::ios::binary);
    if (!inputFileStream) {
        throw FileIOException("Failed to open input bundle file for verification", inputFilePath);
    }
    std::cout << "Verifying bundle file: " << inputFilePath << std::endl;
    return verifyBundleStream(inputFileStream);
}

//--------------------------------------------------------------------------
// Private Methods
//--------------------------------------------------------------------------

/**
 * @brief Internal parsing and extraction/verification logic.
 */
bool Unbundler::processBundle(std::istream& inputStream, const std::filesystem::path& outputDirectory, bool verifyOnly)
{
    std::string line;
    const std::string FILENAME_PREFIX = "Filename: ";
    std::string currentFilename;
    const std::string CHECKSUM_PREFIX = "Checksum: SHA256:";
    std::string expectedChecksum;
    std::stringstream fileContentStream;
    ParserState state = ParserState::EXPECT_SEPARATOR_OR_HEADER;
    bool firstSeparatorFound = false;
    int fileCount = 0;
    bool headerProcessed = false; // Track if we've passed the optional header section
    bool checksumFoundForCurrentFile = false;

    // Helper lambda to process a completed file entry
    auto processFileEntry = [&]() {
        if (!currentFilename.empty()) {
            fileCount++;
            std::string actualContent = fileContentStream.str();
            // Reset stream for next file
            fileContentStream.str("");
            fileContentStream.clear();

            std::cout << (verifyOnly ? "Verifying: " : "Extracting: ") << currentFilename;

            if (m_options.verifyChecksums) {
                if (!checksumFoundForCurrentFile) {
                    // Checksum was missing for this file
                    std::cout << " - WARNING: Checksum missing." << std::endl;
                    // If not verifying, proceed without check. If verifying, this might be considered a failure depending on strictness.
                } else {
                    std::string actualChecksum = utilities::calculateSHA256(actualContent);
                    if (actualChecksum == expectedChecksum) {
                        std::cout << " - Checksum OK." << std::endl;
                    } else {
                        std::cout << " - CHECKSUM MISMATCH!" << std::endl;
                        // Throw immediately if checksum fails during verification or extraction
                        throw ChecksumMismatchException(currentFilename, expectedChecksum, actualChecksum);
                    }
                }
            } else {
                std::cout << " (Checksum verification skipped)." << std::endl;
            }

            if (!verifyOnly) {
                std::filesystem::path outputPath = outputDirectory / currentFilename;
                try {
                    utilities::writeFileContent(outputPath, actualContent);
                } catch (const FileIOException& e) {
                    std::cerr << "\nError writing file '" << outputPath.string() << "': " << e.what() << std::endl;
                    throw; // Re-throw write errors
                }
            }

            // Reset for next file
            currentFilename.clear();
            expectedChecksum.clear();
            checksumFoundForCurrentFile = false;
        }
    };

    while (std::getline(inputStream, line)) {
        // Check for stream errors after reading line
        if (inputStream.fail() && !inputStream.eof()) {
            throw FileIOException("Stream error while reading bundle");
        }

        if (line == m_options.separator) {
            if (state == ParserState::READ_CONTENT) {
                // End of a file entry - process it
                processFileEntry();
                state = ParserState::EXPECT_FILENAME; // Ready for the next file's header
            } else if (state == ParserState::EXPECT_SEPARATOR_OR_HEADER) {
                // This is either the VERY first separator, or the one after the header section.
                if (!firstSeparatorFound) {
                    // This is the very first separator. Now we expect header lines or the next separator.
                    firstSeparatorFound = true;
                    // Stay in EXPECT_SEPARATOR_OR_HEADER, but behavior changes now that firstSeparatorFound=true
                } else {
                    // This is the separator AFTER the header section. Now expect the first Filename.
                    state = ParserState::EXPECT_FILENAME;
                }
            } else if (state == ParserState::EXPECT_FILENAME) {
                // Two separators in a row? (e.g., after finishing a file). Valid, just wait for next Filename.
                continue;
            } else if (state == ParserState::EXPECT_CHECKSUM) {
                // Separator immediately after Checksum line? Implies empty file. Process it.
                processFileEntry(); // Content will be empty stringstream
                state = ParserState::EXPECT_FILENAME; // Ready for next file.
            } else {
                // Should not happen with correct state transitions
                throw BundleFormatException("Unexpected separator encountered in state.");
            }
        } else if (state == ParserState::EXPECT_SEPARATOR_OR_HEADER) {
            if (!firstSeparatorFound) {
                // We haven't seen the first separator yet, but got content. Invalid format.
                throw BundleFormatException("Bundle does not start with the expected separator. Found: " + line);
            } else {
                // We are between the first separator and the one marking the end of the header.
                // This line MUST be either a header line (like Description) or the start of the first file.
                if (utilities::startsWith(line, FILENAME_PREFIX)) {
                    // Found the first file entry directly after the initial separator (no other header lines)
                    currentFilename = utilities::trim(line.substr(FILENAME_PREFIX.length()));
                    if (currentFilename.empty()) {
                        throw BundleFormatException("Filename line found, but filename is empty.");
                    }
                    state = ParserState::EXPECT_CHECKSUM; // Transition to expecting checksum
                } else {
                    // Assume this is an optional header line (e.g., "Description: ...").
                    // Just consume/ignore it and wait for the next separator or the first Filename line.
                    continue;
                }
            }
        } else if (state == ParserState::EXPECT_FILENAME) {
            // We expect a Filename line because the previous line was a separator ending a file entry or the header.
            if (utilities::startsWith(line, FILENAME_PREFIX)) {
                currentFilename = utilities::trim(line.substr(FILENAME_PREFIX.length()));
                if (currentFilename.empty()) {
                    throw BundleFormatException("Filename line found, but filename is empty.");
                }
                state = ParserState::EXPECT_CHECKSUM;
            } else {
                // This is where the original error occurred. If we are in EXPECT_FILENAME state,
                // non-filename lines are an error.
                throw BundleFormatException("Expected 'Filename: ' line, but got: " + line);
            }
        } else if (state == ParserState::EXPECT_CHECKSUM) {
            // Logic for handling Checksum line or start of content (remains the same)
            if (utilities::startsWith(line, CHECKSUM_PREFIX)) {
                expectedChecksum = utilities::trim(line.substr(CHECKSUM_PREFIX.length()));
                if (expectedChecksum.empty() || expectedChecksum.length() != 64) {
                    throw BundleFormatException("Invalid SHA256 checksum format found: " + line);
                }
                checksumFoundForCurrentFile = true;
                state = ParserState::READ_CONTENT;
            } else {
                // Allow content to start immediately if Checksum line is missing
                checksumFoundForCurrentFile = false;
                state = ParserState::READ_CONTENT;
                fileContentStream << line << '\n'; // This line is the first line of content
            }
        } else if (state == ParserState::READ_CONTENT) {
            // Append line to current file content
            fileContentStream << line << '\n';
        }
    } // End while loop

    // Process the last file entry if the bundle didn't end with a separator
    if (state == ParserState::READ_CONTENT) {
        processFileEntry();
    } else if (!currentFilename.empty()) {
        // We had a filename/checksum but no content followed by EOF?
        std::cerr << "Warning: Bundle ended unexpectedly after metadata for file: " << currentFilename << std::endl;
        processFileEntry(); // Process whatever content was gathered (likely none)
    } else if (state != ParserState::EXPECT_FILENAME && fileCount == 0 && headerProcessed) {
        // If we expected a filename after the header but hit EOF, the bundle might be empty or malformed.
        // Only warn if we actually processed a header section.
        std::cerr << "Warning: Bundle appears to be empty or malformed after header." << std::endl;
    } else if (state == ParserState::EXPECT_SEPARATOR_OR_HEADER && !headerProcessed) {
        // Stream was empty or didn't even contain the initial separator
        throw BundleFormatException("Bundle is empty or does not start with the expected separator.");
    }

    // Check for stream errors one last time
    if (inputStream.bad()) {
        throw FileIOException("Stream encountered a fatal error during processing.");
    }

    return true; // If we reached here without throwing, verification passed (or wasn't applicable)
}

} // namespace codebundler
