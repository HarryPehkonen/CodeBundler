#ifndef CODEBUNDLER_BUNDLER_HPP
#define CODEBUNDLER_BUNDLER_HPP

#include "filefilter.hpp"
#include "options.hpp"
#include <iosfwd> // Forward declaration for std::ostream
#include <string>
#include <utility> // std::pair
#include <vector>

namespace codebundler {

/**
 * @brief Creates a CodeBundle archive from files tracked by Git.
 */
class Bundler {
public:
    /**
     * @brief Constructs a Bundler object.
     * @param options The parsed options: separator, verbosity and the selection
     *        patterns that decide which tracked files go in.
     */
    explicit Bundler(const Options& options);

    /**
     * @brief Bundles the selected files into the provided output stream.
     * @param outputStream The stream to write the bundle content to.
     * @param description An optional description to include in the bundle header.
     * @throws GitCommandException If `git ls-files` fails.
     * @throws FileIOException If any selected file cannot be read.
     * @throws std::runtime_error For other unexpected errors.
     */
    void bundleToStream(std::ostream& outputStream, const std::string& description = "");

    /**
     * @brief Bundles the selected files into a specified file.
     * @param outputFilePath The path to the bundle file to create.
     * @param description An optional description to include in the bundle header.
     * @throws GitCommandException If `git ls-files` fails.
     * @throws FileIOException If any selected file cannot be read or the output file cannot be written.
     * @throws std::runtime_error For other unexpected errors.
     */
    void bundleToFile(const std::string& outputFilePath, const std::string& description = "");

    /**
     * @brief Reports what a bundle would carry, one line per file with its size
     * in bytes, and writes no bundle at all.
     *
     * This is the size question a bundle exists to answer - will the artifact
     * fit wherever it is going - so the same selection rules, the same binary
     * rule and the same refusal to carry a file containing the separator are
     * applied here, and the totals say what the bundle would hold.
     *
     * @param outputStream The stream to write the report to.
     */
    void listToStream(std::ostream& outputStream);

private:
    Options m_options;

    /** @brief What the bundler did with one tracked path. */
    enum class EntryOutcome {
        written, // an entry went into the bundle
        skippedDirectory, // git tracked a directory (gitlink); nothing to inline
        skippedBinary // binary file: left out, named in the skip note
    };

    /** @brief What the selection rules did with the tracked paths. */
    struct SelectionResult {
        std::vector<std::string> files; ///< kept, in the order git reported them
        std::vector<std::pair<std::string, int>> excludedByRule; ///< rule as written -> files left out
        int excludedByDefault = 0; ///< left out because include mode admits only what a rule names
    };

    /**
     * @brief Builds the filter from the repository's selection file and the
     * command line, in that order, so the command line overrides the file.
     */
    FileFilter buildFilter() const;

    /** @brief Applies the filter to the tracked paths. */
    SelectionResult selectFiles(const std::vector<std::string>& trackedFiles) const;

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

    /**
     * @brief Reports the files the selection rules left out, one line per rule.
     *
     * Counts, not names: a rule such as `tests/` removes hundreds of files, and
     * the bundle is meant to stay readable. The per-file detail is on standard
     * error at verbosity 3.
     *
     * @param outputStream The stream to write to.
     * @param selection The selection the files came from.
     * @param lead What starts each line: the note prefix inside a bundle, the
     *        plain phrase in a *list* report.
     */
    void writeSelectionNote(std::ostream& outputStream, const SelectionResult& selection, const std::string& lead) const;
};

} // namespace codebundler

#endif // CODEBUNDLER_BUNDLER_HPP
