#include "bundler.hpp"
#include "constants.hpp"
#include "exceptions.hpp"
#include "utilities.hpp"
#include <algorithm> // std::find_if
#include <filesystem> // For path manipulation
#include <fstream>
#include <iomanip> // std::setw
#include <iostream> // For std::cerr, std::cout
#include <vector>

namespace codebundler {

namespace {

    /** @brief "1 file" / "2 files", so a count never reads "1 file(s)". */
    std::string fileCount(int count)
    {
        return std::to_string(count) + (count == 1 ? " file" : " files");
    }

} // namespace

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
 * @brief Bundles the selected files into the provided output stream.
 */
void Bundler::bundleToStream(std::ostream& outputStream, const std::string& description)
{
    m_options.verbose > 0 && std::cerr << "Gathering files tracked by Git..." << std::endl;
    const std::vector<std::string> trackedFiles = utilities::getGitTrackedFiles();
    m_options.verbose > 0 && std::cerr << "Found " << trackedFiles.size() << " files." << std::endl;

    const SelectionResult selection = selectFiles(trackedFiles);
    m_options.verbose > 0 && std::cerr << "Selected " << selection.files.size() << " files to bundle." << std::endl;

    if (selection.files.empty()) {
        m_options.verbose > 0 && std::cerr << "Warning: no file was selected. Bundle will be empty." << std::endl;
    }

    writeHeader(outputStream, description);

    std::vector<std::string> skippedBinaries;
    for (const auto& filePath : selection.files) {
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
    writeSelectionNote(outputStream, selection, codebundler::EXCLUDED_NOTE_PREFIX);
    if (!skippedBinaries.empty()) {
        m_options.verbose > 0 && std::cerr << "Skipped " << skippedBinaries.size() << " binary file(s); they are named at the end of the bundle." << std::endl;
    }
    m_options.verbose > 0 && std::cerr << "Bundle creation finished." << std::endl;
}

/**
 * @brief Bundles the selected files into a specified file.
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

/**
 * @brief Reports what a bundle would carry, and writes nothing.
 *
 * The size question is the whole reason this exists: a bundle is made to be
 * pasted somewhere, and the answer to "will it fit" is a list with byte counts.
 * Every rule a real bundle applies is applied here, so the totals are what a
 * bundle would hold rather than an estimate from file sizes on disk.
 */
void Bundler::listToStream(std::ostream& outputStream)
{
    m_options.verbose > 0 && std::cerr << "Gathering files tracked by Git..." << std::endl;
    const std::vector<std::string> trackedFiles = utilities::getGitTrackedFiles();
    const SelectionResult selection = selectFiles(trackedFiles);

    std::size_t listed = 0;
    std::size_t contentBytes = 0;
    int binaryFiles = 0;
    int refusedFiles = 0;

    for (const auto& filePath : selection.files) {
        const std::filesystem::path fsPath(filePath);
        if (std::filesystem::is_directory(fsPath)) {
            continue; // a gitlink: a bundle has never carried one
        }
        if (utilities::isBinaryFile(fsPath)) {
            ++binaryFiles;
            m_options.verbose > 2 && std::cerr << "Counted binary (not inlined): " << filePath << std::endl;
            continue;
        }
        const std::vector<std::string> fileLines = utilities::readFileLines(fsPath);
        const std::string fileContent = utilities::linesToString(fileLines);
        if (fileContent.find('\0') != std::string::npos) {
            ++binaryFiles;
            m_options.verbose > 2 && std::cerr << "Counted binary (NUL past the sniff window): " << filePath << std::endl;
            continue;
        }
        if (utilities::fileContainsDelimiter(fileLines, m_options.separator)) {
            ++refusedFiles;
            m_options.verbose > 2 && std::cerr << "Refused (contains the separator): " << filePath << std::endl;
            continue;
        }
        const std::size_t size = fileContent.size();
        ++listed;
        contentBytes += size;
        outputStream << std::setw(9) << size << "  " << filePath << "\n";
    }

    outputStream << "\nTotal: " << fileCount(static_cast<int>(listed)) << ", " << contentBytes
                 << " bytes of content (about " << (contentBytes / 4) << " tokens at 4 bytes/token)\n";
    writeSelectionNote(outputStream, selection, "Excluded: ");
    if (binaryFiles > 0) {
        outputStream << "Not inlined: " << fileCount(binaryFiles) << ", binary (a bundle names them instead)\n";
    }
    if (refusedFiles > 0) {
        outputStream << "Refused: " << fileCount(refusedFiles) << " containing the separator\n";
    }
    if (!outputStream) {
        throw FileIOException("Stream error occurred while writing the file list", "list");
    }
}

//--------------------------------------------------------------------------
// Private Methods
//--------------------------------------------------------------------------

/**
 * @brief Builds the selection filter: the repository's own file first, then the
 * command line, so the command line overrides it.
 */
FileFilter Bundler::buildFilter() const
{
    std::vector<FilterRule> rules;

    for (const auto& line : utilities::readIgnoreFile()) {
        FilterRule rule;
        if (FileFilter::parseRule(line, codebundler::IGNORE_FILE_NAME, rule)) {
            rules.push_back(rule);
        }
    }

    // Excludes before includes: an include is the more specific statement and
    // the last matching rule wins, so an include re-admits what an exclude
    // removed - whichever order the two were typed in.
    for (const auto& pattern : m_options.excludePatterns) {
        FilterRule rule;
        if (FileFilter::parseRule(pattern, "--exclude", rule)) {
            rule.include = false; // the flag, not a leading '!', decides the direction
            rules.push_back(rule);
        }
    }
    for (const auto& pattern : m_options.includePatterns) {
        FilterRule rule;
        if (FileFilter::parseRule(pattern, "--include", rule)) {
            rule.include = true;
            rules.push_back(rule);
        }
    }

    // Naming anything to include switches the bundle to include mode.
    const bool includeMode = !m_options.includePatterns.empty();
    m_options.verbose > 2 && std::cerr << "Selection: " << rules.size() << " rule(s), mode: " << (includeMode ? "include (only what a rule names)" : "exclude (everything tracked otherwise)") << std::endl;
    return FileFilter(std::move(rules), includeMode);
}

/**
 * @brief Applies the filter to the tracked paths, keeping the bookkeeping the
 * note needs.
 */
Bundler::SelectionResult Bundler::selectFiles(const std::vector<std::string>& trackedFiles) const
{
    const FileFilter filter = buildFilter();
    SelectionResult selection;

    for (const auto& filePath : trackedFiles) {
        const FilterRule* rule = filter.decidingRule(filePath);
        if (rule == nullptr) {
            if (filter.whitelist()) {
                ++selection.excludedByDefault; // include mode: nothing named it
                m_options.verbose > 2 && std::cerr << "Not named by any include rule: " << filePath << std::endl;
                continue;
            }
            selection.files.push_back(filePath);
            continue;
        }
        if (rule->include) {
            selection.files.push_back(filePath);
            continue;
        }

        m_options.verbose > 2 && std::cerr << "Excluded by '" << rule->text << "': " << filePath << std::endl;
        auto known = std::find_if(selection.excludedByRule.begin(), selection.excludedByRule.end(),
            [&rule](const std::pair<std::string, int>& entry) { return entry.first == rule->text; });
        if (known == selection.excludedByRule.end()) {
            selection.excludedByRule.emplace_back(rule->text, 1);
        } else {
            ++known->second;
        }
    }

    return selection;
}

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

/**
 * @brief Reports what the selection rules left out, one line per rule.
 */
void Bundler::writeSelectionNote(std::ostream& outputStream, const SelectionResult& selection, const std::string& lead) const
{
    for (const auto& [ruleText, count] : selection.excludedByRule) {
        outputStream << lead << fileCount(count) << " (" << ruleText << ")\n";
    }
    if (selection.excludedByDefault > 0) {
        outputStream << lead << fileCount(selection.excludedByDefault) << " ("
                     << codebundler::EXCLUDED_BY_DEFAULT << ")\n";
    }
    if (!outputStream) {
        throw FileIOException("Stream error occurred while writing the selection note", lead);
    }
}

} // namespace codebundler
