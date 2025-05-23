#ifndef CODEBUNDLER_UNBUNDLER_HPP
#define CODEBUNDLER_UNBUNDLER_HPP

#include "options.hpp"
#include <filesystem> // Requires C++17
#include <iosfwd> // Forward declaration for std::istream
#include <string>
#include <vector>

namespace codebundler {

/**
 * @brief Extracts files from a CodeBundle archive.
 */
class Unbundler {
public:
    /**
     * @brief Constructs an Unbundler object.
     * @param options Configuration options for unbundling.
     */
    explicit Unbundler(Options options);

    /**
     * @brief Unbundles files from the provided input stream into a target directory.
     * @param inputStream The stream containing the bundle content.
     * @param outputDirectory The directory where files will be extracted. Defaults to current directory.
     * @throws BundleFormatException If the bundle format is invalid or separator cannot be detected.
     * @throws ChecksumMismatchException If checksum verification is enabled and fails.
     * @throws FileIOException If files cannot be written to the output directory or stream errors occur.
     * @throws std::runtime_error For other unexpected errors.
     */
    void unbundleFromStream(std::istream& inputStream, const std::filesystem::path& outputDirectory = ".");

    /**
     * @brief Unbundles files from a specified bundle file into a target directory.
     * @param inputFilePath The path to the bundle file.
     * @param outputDirectory The directory where files will be extracted. Defaults to current directory.
     * @throws BundleFormatException If the bundle format is invalid or separator cannot be detected.
     * @throws ChecksumMismatchException If checksum verification is enabled and fails.
     * @throws FileIOException If the bundle file cannot be read or files cannot be written.
     * @throws std::runtime_error For other unexpected errors.
     */
    void unbundleFromFile(const std::string& inputFilePath, const std::filesystem::path& outputDirectory = ".");

    /**
     * @brief Verifies the integrity of a bundle by checking checksums without extracting files.
     * @param inputStream The stream containing the bundle content.
     * @return True if all checksums match, false otherwise.
     * @throws BundleFormatException If the bundle format is invalid or separator cannot be detected.
     * @throws FileIOException If the bundle stream cannot be read properly.
     */
    bool verifyBundleStream(std::istream& inputStream);

    /**
     * @brief Verifies the integrity of a bundle file by checking checksums without extracting files.
     * @param inputFilePath The path to the bundle file.
     * @return True if all checksums match, false otherwise.
     * @throws BundleFormatException If the bundle format is invalid or separator cannot be detected.
     * @throws FileIOException If the bundle file cannot be read properly.
     */
    bool verifyBundleFile(const std::string& inputFilePath);

private:
    Options m_options;

    enum class ParserState {
        EXPECT_SEPARATOR_OR_HEADER, // Expecting optional header lines or the separator after the header
        EXPECT_FILENAME,
        EXPECT_CHECKSUM,
        READ_CONTENT
        /*
        READ_SEPARATOR,
        EXPECT_FILENAME_OR_COMMENT,
        EXPECT_FILENAME,
        EXPECT_CHECKSUM_OR_CONTENT,
        EXPECT_CONTENT_OR_SEPARATOR
        */
    };

    /**
     * @brief Internal parsing and extraction/verification logic. Separator is detected from the stream.
     * @param inputStream The stream to read from.
     * @param outputDirectory The directory to extract to (used only if verifyOnly is false).
     * @param verifyOnly If true, only verify checksums; do not write files.
     * @return True if verification passed (only relevant if verifyOnly is true), otherwise void.
     * @throws Various exceptions based on errors encountered.
     */
    bool processBundle(std::istream& inputStream, const std::filesystem::path& outputDirectory);
};

} // namespace codebundler

#endif // CODEBUNDLER_UNBUNDLER_HPP
