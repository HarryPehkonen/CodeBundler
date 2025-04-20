#include "unbundler.hpp"
#include "bundleparser.hpp"
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
{ } // Separator is now detected, no need to validate here.

//--------------------------------------------------------------------------
// Public Methods
//--------------------------------------------------------------------------

/**
 * @brief Unbundles files from the provided input stream into a target directory.
 */
void Unbundler::unbundleFromStream(std::istream& inputStream, const std::filesystem::path& outputDirectory)
{
    std::cout << "Starting unbundle process..." << std::endl;

    /* do we actually need this?  i don't think so
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
    */

    processBundle(inputStream, outputDirectory);
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
 * @brief Internal parsing and extraction/verification logic. Separator is detected from the stream.
 */
bool Unbundler::processBundle(std::istream& inputStream, const std::filesystem::path& outputDirectory)
{
    int verbose = 5;
    BundleParser::Hasher hasher = utilities::calculateSHA256;
    BundleParser parser(m_options, hasher, outputDirectory);
    std::string line;
    bool done = false;

    while (std::getline(inputStream, line)) {
        std::cout << line << std::endl;
        done = parser.parse(std::make_optional(line));
    }

    if (done) {
        std::cerr << "Parser indicated 'done', but shouldn't be yet." << std::endl;
    }

    if (!done) {

        // signal EOF
        done = parser.parse(std::nullopt);
    }

    if (!done) {
        std::cerr << "Error: Parsing failed!" << std::endl;
        return 1;
    } else {
        std::cout << "Parsing completed successfully!" << std::endl;
    }

    return true; // If we reached here without throwing, verification passed or wasn't applicable
}

} // namespace codebundler
