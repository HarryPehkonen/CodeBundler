#ifndef CODEBUNDLER_BUNDLER_HPP
#define CODEBUNDLER_BUNDLER_HPP

#include "options.hpp"
#include <iosfwd> // Forward declaration for std::ostream
#include <string>
#include <vector>

namespace codebundler {

/**
 * @brief Creates a CodeBundle archive from files tracked by Git.
 */
class Bundler {
public:
    /**
     * @brief Constructs a Bundler object.
     * @param separator The boundary marker to use between files in the bundle.
     */
    explicit Bundler(const Options& options);

    /**
     * @brief Bundles files tracked by `git ls-files` into the provided output stream.
     * @param outputStream The stream to write the bundle content to.
     * @param description An optional description to include in the bundle header.
     * @throws GitCommandException If `git ls-files` fails.
     * @throws FileIOException If any tracked file cannot be read.
     * @throws std::runtime_error For other unexpected errors.
     */
    void bundleToStream(std::ostream& outputStream, const std::string& description = "");

    /**
     * @brief Bundles files tracked by `git ls-files` into a specified file.
     * @param outputFilePath The path to the bundle file to create.
     * @param description An optional description to include in the bundle header.
     * @throws GitCommandException If `git ls-files` fails.
     * @throws FileIOException If any tracked file cannot be read or the output file cannot be written.
     * @throws std::runtime_error For other unexpected errors.
     */
    void bundleToFile(const std::string& outputFilePath, const std::string& description = "");

private:
    Options m_options;

    /** @brief What the bundler did with one tracked path. */
    enum class EntryOutcome {
        written, // an entry went into the bundle
        skippedDirectory, // git tracked a directory (gitlink); nothing to inline
        skippedBinary // binary file: left out, named in the skip note
    };

    /**
     * @brief Writes the bundle header to the output stream.
     * @param outputStream The stream to write to.
     * @param description The optional description text.
     */
    void writeHeader(std::ostream& outputStream, const std::string& description);

    /**
     * @brief Writes a single file entry (header and content) to the output stream.
     * @param outputStream The stream to write to.
     * @param filePath The path of the file to include.
     * @return What happened to the path (see EntryOutcome).
     * @throws FileIOException If the file cannot be read.
     */
    EntryOutcome writeFileEntry(std::ostream& outputStream, const std::string& filePath);

    /**
     * @brief Writes one comment line per binary file left out of the bundle.
     * Written after the last entry; the unbundler treats these lines as
     * comments, so the note does not affect what gets restored.
     * @param outputStream The stream to write to.
     * @param skippedBinaries The paths that were skipped.
     */
    void writeSkippedNote(std::ostream& outputStream, const std::vector<std::string>& skippedBinaries);
};

} // namespace codebundler

#endif // CODEBUNDLER_BUNDLER_HPP
