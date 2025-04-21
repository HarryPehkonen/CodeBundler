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
{
} // Separator is now detected, no need to validate here.

//--------------------------------------------------------------------------
// Public Methods
//--------------------------------------------------------------------------

/**
 * @brief Unbundles files from the provided input stream into a target directory.
 */
void Unbundler::unbundleFromStream(std::istream& inputStream, const std::filesystem::path& outputDirectory)
{
    m_options.verbose > 0 && std::cerr << "Starting unbundle process..." << std::endl;

    processBundle(inputStream, outputDirectory);
    m_options.verbose > 0 && std::cerr << "Unbundle process finished." << std::endl;
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
    m_options.verbose > 0 && std::cerr << "Reading bundle from: " << inputFilePath << std::endl;
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
        m_options.verbose > 3 && std::cerr << line << std::endl;
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
        m_options.verbose > 0 && std::cerr << "Parsing completed successfully!" << std::endl;
    }

    return true; // If we reached here without throwing, verification passed or wasn't applicable
}

} // namespace codebundler
