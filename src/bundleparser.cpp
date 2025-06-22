#include "bundleparser.hpp"
#include "constants.hpp"
#include "exceptions.hpp"
#include "options.hpp"

#include <cctype> // For std::isspace in trim
#include <filesystem> // For std::filesystem::create_directories, std::filesystem::path
#include <fstream> // For std::ofstream
#include <iostream> // For std::cout, std::cerr
#include <sstream> // For std::stringstream
#include <stdexcept> // For std::runtime_error

// Constructor
BundleParser::BundleParser(const codebundler::Options& options, Hasher hasher, std::filesystem::path outputPath)
    : options_(options)
    , hasher_(std::move(hasher))
    , outputPath_(outputPath)
{
    auto builder = fsm_.get_builder();
    
    // READ SEPARATOR -> EXPECT FILENAME OR COMMENT (always transition)
    builder.from("READ SEPARATOR")
        .action([this](const InputType& input) { rememberSeparator(input); })
        .to("EXPECT FILENAME OR COMMENT");
    
    // EXPECT FILENAME OR COMMENT transitions
    builder.from("EXPECT FILENAME OR COMMENT")
        .predicate([this](const InputType& input) { return isFilename(input); })
        .action([this](const InputType& input) { rememberFilename(input); })
        .to("EXPECT CHECKSUM OR CONTENT");
    
    builder.from("EXPECT FILENAME OR COMMENT")
        .predicate([this](const InputType& input) { return isChecksum(input); })
        .action([this](const InputType& input) { errorMissingFilename(input); })
        .to("DONE");
    
    builder.from("EXPECT FILENAME OR COMMENT")
        .action([this](const InputType& input) { skip(input); })
        .to("IN COMMENT"); // If not filename or checksum, assume comment start
    
    // IN COMMENT transitions
    builder.from("IN COMMENT")
        .predicate([this](const InputType& input) { return isSeparator(input); })
        .action([this](const InputType& input) { skip(input); })
        .to("EXPECT FILENAME");
    
    builder.from("IN COMMENT")
        .predicate([this](const InputType& input) { return isEOF(input); })
        .action([this](const InputType& input) { done(input); })
        .to("DONE");
    
    builder.from("IN COMMENT")
        .action([this](const InputType& input) { skip(input); })
        .to("IN COMMENT"); // Continue skipping comment lines
    
    // EXPECT CHECKSUM OR CONTENT transitions
    builder.from("EXPECT CHECKSUM OR CONTENT")
        .predicate([this](const InputType& input) { return isChecksum(input); })
        .action([this](const InputType& input) { rememberChecksum(input); })
        .to("IN CONTENT");
    
    builder.from("EXPECT CHECKSUM OR CONTENT")
        .predicate([this](const InputType& input) { return isSeparator(input); })
        .action([this](const InputType& input) { saveFile(input); })
        .to("EXPECT FILENAME");
    
    builder.from("EXPECT CHECKSUM OR CONTENT")
        .predicate([this](const InputType& input) { return isEOF(input); })
        .action([this](const InputType& input) { errorBadFormat(input); })
        .to("DONE");
    
    builder.from("EXPECT CHECKSUM OR CONTENT")
        .action([this](const InputType& input) { rememberContentLine(input); })
        .to("IN CONTENT"); // If not checksum/sep/EOF, assume content
    
    // IN CONTENT transitions
    builder.from("IN CONTENT")
        .predicate([this](const InputType& input) { return isSeparator(input); })
        .action([this](const InputType& input) { saveFile(input); })
        .to("EXPECT FILENAME");
    
    builder.from("IN CONTENT")
        .predicate([this](const InputType& input) { return isEOF(input); })
        .action([this](const InputType& input) { saveFile(input); })
        .to("DONE");
    
    builder.from("IN CONTENT")
        .action([this](const InputType& input) { rememberContentLine(input); })
        .to("IN CONTENT"); // Continue reading content lines
    
    // EXPECT FILENAME transitions
    builder.from("EXPECT FILENAME")
        .predicate([this](const InputType& input) { return isFilename(input); })
        .action([this](const InputType& input) { rememberFilename(input); })
        .to("EXPECT CHECKSUM OR CONTENT");
    
    builder.from("EXPECT FILENAME")
        .predicate([this](const InputType& input) { return isEOF(input); })
        .action([this](const InputType& input) { done(input); })
        .to("DONE");
    
    builder.from("EXPECT FILENAME")
        .action([this](const InputType& input) { skip(input); })
        .to("IN COMMENT"); // If not filename/EOF after separator, treat as comment
    
    // Set initial state
    fsm_.setInitialState("READ SEPARATOR");
}

std::string BundleParser::trim(const std::string& str)
{
    const std::string whitespace = " \t\n\r\f\v";
    size_t first = str.find_first_not_of(whitespace);
    if (first == std::string::npos) {
        return ""; // String contains only whitespace
    }
    size_t last = str.find_last_not_of(whitespace);
    return str.substr(first, (last - first + 1));
}

// predicates
bool BundleParser::isSeparator(const InputType& input) const
{
    bool result = input && !separator_.empty() && input.value().find(separator_, 0) == 0;
    options_.verbose > 2 && std::cerr << "predicate: isSeparator ('" << (input ? input.value() : "EOF") << "' vs '" << separator_ << "') -> " << (result ? "true" : "false") << std::endl;
    return result;
}

bool BundleParser::isFilename(const InputType& input) const
{
    bool result = input && input.value().find(codebundler::FILENAME_PREFIX, 0) == 0;
    options_.verbose > 2 && std::cerr << "predicate: isFilename ('" << (input ? input.value() : "EOF") << "') -> " << (result ? "true" : "false") << std::endl;
    return result;
}

bool BundleParser::isChecksum(const InputType& input) const
{
    bool result = input && input.value().find(codebundler::CHECKSUM_PREFIX, 0) == 0;
    options_.verbose > 2 && std::cerr << "predicate: isChecksum ('" << (input ? input.value() : "EOF") << "') -> " << (result ? "true" : "false") << std::endl;
    return result;
}

bool BundleParser::isEOF(const InputType& input) const
{
    bool result = !input.has_value();
    options_.verbose > 2 && std::cerr << "predicate: isEOF -> " << (result ? "true" : "false") << std::endl;
    return result;
}

// actions
void BundleParser::rememberSeparator(const InputType& input)
{
    if (input) {
        separator_ = trim(input.value());
        options_.verbose > 2 && std::cerr << "action: rememberSeparator -> separator set to '" << separator_ << "'" << std::endl;
    } else {
        options_.verbose > 2 && std::cerr << "action: rememberSeparator (skipped on EOF)" << std::endl;
    }
}

void BundleParser::rememberFilename(const InputType& input)
{
    if (input) {
        filename_ = trim(input.value().substr(codebundler::FILENAME_PREFIX.length()));
        options_.verbose > 2 && std::cerr << "action: rememberFilename -> filename set to '" << filename_ << "'" << std::endl;
    } else {
        options_.verbose > 2 && std::cerr << "action: rememberFilename (skipped on EOF)" << std::endl;
    }
}

void BundleParser::rememberChecksum(const InputType& input)
{
    if (input) {
        checksum_ = trim(input.value().substr(codebundler::CHECKSUM_PREFIX.length()));
        options_.verbose > 2 && std::cerr << "action: rememberChecksum -> checksum set to '" << checksum_ << "'" << std::endl;
    } else {
        options_.verbose > 2 && std::cerr << "action: rememberChecksum (skipped on EOF)" << std::endl;
    }
}

void BundleParser::rememberContentLine(const InputType& input)
{
    if (input) {
        // Don't trim content lines. preserve whitespace within the line
        lines_.push_back(input.value());
        options_.verbose > 2 && std::cerr << "action: rememberContentLine -> added '" << input.value() << "'" << std::endl;
    } else {
        options_.verbose > 2 && std::cerr << "action: rememberContentLine (skipped on EOF)" << std::endl;
    }
}

void BundleParser::saveFile(const InputType& /*input*/)
{
    options_.verbose > 2 && std::cerr << "action: saveFile" << std::endl;

    if (options_.verbose > 2) {
        std::cerr << "  Attempting to save file: '" << filename_ << "'\n"
                  << "  With Checksum: '" << checksum_ << "'\n"
                  << "  Output Path: '" << outputPath_ << "'\n"
                  << "  Line " << lineCount_ << std::endl;
    }

    if (filename_.empty()) {
        options_.verbose > 2 && std::cerr << "Error: Cannot save file with empty filename." << std::endl;
        throw codebundler::BundleFormatException("Empty filename.");
    }

    std::filesystem::path filepath = outputPath_ / filename_;

    // we'll get the file contents regardless
    std::stringstream fileContentStream;
    for (size_t i = 0; i < lines_.size(); ++i) {
        fileContentStream << lines_[i] << "\n";
    }
    std::string fileContent = fileContentStream.str(); // Reconstruct content

    // things to check:
    //   - have hasher?   (h)
    //   - have checksum? (c)
    //   - calculated checksum matches? (m)
    //   - supposed to verify?          (v)
    //
    //   h c m v save?
    //   0 x x 0 yes
    //   0 x x 1 no
    //   1 0 x 0 yes
    //   1 0 x 1 no
    //   1 1 x 0 yes
    //   1 1 0 1 no
    //   1 1 1 0 yes
    //   1 1 1 1 yes

    // find all the bad scenarios and report them if verbosity
    // is set
    bool toSave = true;
    std::string calculatedChecksum;
    if (!hasher_) {

        // 0 x x x
        if (options_.verify) {

            // 0 x x 1
            // no hasher but verifying
            toSave = false;
            options_.verbose > 2 && std::cerr << "No hasher but supposed to verify" << std::endl;
            throw codebundler::CodeBundlerException("No hasher but supposed to verify");
        }
    } else {

        // 1 x x x
        if (checksum_.empty()) {

            // 1 0 x x
            if (options_.verify) {

                // 1 0 x 1
                // hasher and verifying but no checksum
                options_.verbose > 2 && std::cerr << "No checksum.  Line " << lineCount_ << std::endl;
                toSave = false;
                throw codebundler::ChecksumMismatchException(filename_, checksum_, calculatedChecksum);
            }
        } else {

            // 1 1 x x
            calculatedChecksum = hasher_(fileContent);
            bool match = (calculatedChecksum == checksum_);

            if (!match) {

                // 1 1 0 x
                if (options_.verify) {

                    // 1 1 0 1
                    // verifying but checksum doesn't match
                    toSave = false;
                    if (options_.verbose > 2) {
                        std::cerr << "Verifying but checksum mismatch\n"
                                     "  Expected:   "
                                  << checksum_ << "\n"
                                                         "  Calculated: "
                                  << calculatedChecksum << std::endl;
                    }
                    throw codebundler::ChecksumMismatchException(filename_, checksum_, calculatedChecksum);
                }
            }
        }
    }

    options_.verbose > 2 && std::cerr << "  Saving file: '" << filepath << "'" << std::endl;

    // Create parent directories if they don't exist
    if (!options_.trialRun && filepath.has_parent_path()) {
        options_.verbose > 1 && std::cerr << "  Creating directories: " << filepath.parent_path() << std::endl;
        std::filesystem::create_directories(filepath.parent_path());
    }

    // Open file in binary mode to write content correctly
    std::ofstream fileStream;
    fileStream.exceptions(std::ios::badbit | std::ios::failbit); // Throw exceptions on failure
    options_.verbose > 2 && std::cerr << "  Opening file for writing: " << filepath << std::endl;
    if (options_.trialRun) {
        options_.verbose > 2 && std::cerr << "  Trial run: file not actually written." << std::endl;
    } else {
        fileStream.open(filepath, std::ios::out | std::ios::trunc);

        fileStream.write(fileContent.data(), fileContent.size());
        fileStream.close(); // Close explicitly after write
        options_.verbose > 2 && std::cerr << "  File saved successfully: '" << filename_ << "'" << std::endl;
    }

    // Reset state for the next file
    filename_.clear();
    checksum_.clear();
    lines_.clear();
}

void BundleParser::skip(const InputType& input)
{
    if (options_.verbose > 2) {
        if (input) {
            std::cerr << "action: skip -> Skipping line: '" << input.value() << "'" << std::endl;
        } else {
            std::cerr << "action: skip (on EOF)" << std::endl;
        }
    }
    // No actual state change needed for skipping
}

void BundleParser::done(const InputType& /*input*/)
{
    options_.verbose > 2 && std::cerr << "action: done" << std::endl;
    // Final actions could happen here if needed, e.g., saving the last file if EOF acts as implicit separator
    // The current state machine seems to handle saveFile *before* transitioning based on EOF
}

void BundleParser::errorMissingFilename(const InputType& /*input*/)
{
    options_.verbose > 2 && std::cerr << "action: errorMissingFilename" << std::endl;
    throw codebundler::BundleFormatException("Missing filename.");
}

void BundleParser::errorBadFormat(const InputType& /*input*/)
{
    options_.verbose > 2 && std::cerr << "action: errorBadFormat" << std::endl;
    throw codebundler::BundleFormatException("Bad format.");
}

// --- Public Method Definition ---
bool BundleParser::parse(const InputType& input)
{
    lineCount_ += 1;

    if (options_.verbose > 1) {
        std::cerr << "Parsing input: " << (input ? "'" + input.value() + "'" : "EOF")
                  << " in state: " << fsm_.getCurrentState() << std::endl;
    }

    // Process the event through FSM and check if a transition was found
    bool transitioned = fsm_.process(input);
    
    if (!transitioned) {
        // No valid transition found
        options_.verbose > 2 && std::cerr << "Error: No valid transition found from state " 
                                          << fsm_.getCurrentState() 
                                          << " for input: " << (input ? "'" + input.value() + "'" : "EOF") 
                                          << " line " << lineCount_ << std::endl;
        throw std::runtime_error("Invalid state or input encountered during parsing");
    }

    // Check if we're done
    return fsm_.getCurrentState() == "DONE";
}
