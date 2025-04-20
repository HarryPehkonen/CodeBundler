#include "bundleparser.hpp"
#include "options.hpp"
#include "exceptions.hpp"

#include <cctype>     // For std::isspace in trim
#include <iostream>   // For std::cout, std::cerr
#include <fstream>    // For std::ofstream
#include <sstream>    // For std::stringstream
#include <stdexcept>  // For std::runtime_error
#include <filesystem> // For std::filesystem::create_directories, std::filesystem::path

// Constructor
BundleParser::BundleParser(const codebundler::Options& options, Hasher hasher, std::filesystem::path outputPath) :
    options_(options),
    hasher_(std::move(hasher)),
    outputPath_(outputPath)
{ }

std::string BundleParser::trim(const std::string& str) {
    const std::string whitespace = " \t\n\r\f\v";
    size_t first = str.find_first_not_of(whitespace);
    if (first == std::string::npos) {
        return ""; // String contains only whitespace
    }
    size_t last = str.find_last_not_of(whitespace);
    return str.substr(first, (last - first + 1));
}

// predicates
const std::function<bool(const InputType&, BundleParser&)> BundleParser::always =
    [](const InputType&, BundleParser& parser) {
    if (parser.options_.verbose > 0) std::cout << "predicate: always -> true" << std::endl;
    return true;
};

const std::function<bool(const InputType&, BundleParser&)> BundleParser::isSeparator =
    [](const InputType& input, BundleParser& parser) {
    bool result = input && !parser.separator_.empty() && input.value().find(parser.separator_, 0) == 0;
    if (parser.options_.verbose > 0) std::cout << "predicate: isSeparator ('" << (input ? input.value() : "EOF") << "' vs '" << parser.separator_ << "') -> " << (result ? "true" : "false") << std::endl;
    return result;
};

const std::function<bool(const InputType&, BundleParser&)> BundleParser::isFilename =
    [](const InputType& input, BundleParser& parser) {
    bool result = input && input.value().find(FILENAME_PREFIX, 0) == 0;
    if (parser.options_.verbose > 0) std::cout << "predicate: isFilename ('" << (input ? input.value() : "EOF") << "') -> " << (result ? "true" : "false") << std::endl;
    return result;
};

const std::function<bool(const InputType&, BundleParser&)> BundleParser::isChecksum =
    [](const InputType& input, BundleParser& parser) {
    bool result = input && input.value().find(CHECKSUM_PREFIX, 0) == 0;
     if (parser.options_.verbose > 0) std::cout << "predicate: isChecksum ('" << (input ? input.value() : "EOF") << "') -> " << (result ? "true" : "false") << std::endl;
    return result;
};

const std::function<bool(const InputType&, BundleParser&)> BundleParser::isEOF =
    [](const InputType& input, BundleParser& parser) {
    bool result = !input.has_value();
    if (parser.options_.verbose > 0) std::cout << "predicate: isEOF -> " << (result ? "true" : "false") << std::endl;
    return result;
};

// actions
const std::function<void(const InputType&, BundleParser&)> BundleParser::rememberSeparator =
    [](const InputType& input, BundleParser& parser) {
    if (input) {
        parser.separator_ = parser.trim(input.value());
        if (parser.options_.verbose > 0) std::cout << "action: rememberSeparator -> separator set to '" << parser.separator_ << "'" << std::endl;
    } else {
         if (parser.options_.verbose > 0) std::cout << "action: rememberSeparator (skipped on EOF)" << std::endl;
    }
};

const std::function<void(const InputType&, BundleParser&)> BundleParser::rememberFilename =
    [](const InputType& input, BundleParser& parser) {
    if (input) {
        parser.filename_ = parser.trim(input.value().substr(FILENAME_PREFIX.length()));
        if (parser.options_.verbose > 0) std::cout << "action: rememberFilename -> filename set to '" << parser.filename_ << "'" << std::endl;
    } else {
        if (parser.options_.verbose > 0) std::cout << "action: rememberFilename (skipped on EOF)" << std::endl;
    }
};

const std::function<void(const InputType&, BundleParser&)> BundleParser::rememberChecksum =
    [](const InputType& input, BundleParser& parser) {
    if (input) {
        parser.checksum_ = parser.trim(input.value().substr(CHECKSUM_PREFIX.length()));
        if (parser.options_.verbose > 0) std::cout << "action: rememberChecksum -> checksum set to '" << parser.checksum_ << "'" << std::endl;
    } else {
        if (parser.options_.verbose > 0) std::cout << "action: rememberChecksum (skipped on EOF)" << std::endl;
    }
};

const std::function<void(const InputType&, BundleParser&)> BundleParser::rememberContentLine =
    [](const InputType& input, BundleParser& parser) {
    if (input) {
        // Don't trim content lines.  preserve whitespace within the line
        parser.lines_.push_back(input.value());
        if (parser.options_.verbose > 1) std::cout << "action: rememberContentLine -> added '" << input.value() << "'" << std::endl;
        else if (parser.options_.verbose > 0) std::cout << "action: rememberContentLine" << std::endl;

    } else {
        if (parser.options_.verbose > 0) std::cout << "action: rememberContentLine (skipped on EOF)" << std::endl;
    }
};

const std::function<void(const InputType&, BundleParser&)> BundleParser::saveFile =
    [](const InputType& /*input*/, BundleParser& parser) {
    if (parser.options_.verbose > 0) std::cout << "action: saveFile" << std::endl;

    if (parser.options_.verbose > 0) {
         std::cout << "  Attempting to save file: '" << parser.filename_ << "'" << std::endl;
         std::cout << "  With Checksum: '" << parser.checksum_ << "'" << std::endl;
         std::cout << "  Output Path: '" << parser.outputPath_ << "'" << std::endl;
         std::cout << "  Line " << parser.lineCount_ << std::endl;
    }

    if (parser.filename_.empty()) {
         std::cerr << "Error: Cannot save file with empty filename." << std::endl;
         throw codebundler::BundleFormatException("Empty filename.");
    }


    std::filesystem::path filepath = parser.outputPath_ / parser.filename_;

    // we'll get the file contents regardless
    std::stringstream fileContentStream;
    for (size_t i = 0; i < parser.lines_.size(); ++i) {
        fileContentStream << parser.lines_[i] << "\n";
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
    if (!parser.hasher_) {

        // 0 x x x
        if (parser.options_.verify) {

            // 0 x x 1
            // no hasher but verifying
            toSave = false;
            parser.options_.verbose > 0 && std::cerr << "No hasher but supposed to verify" << std::endl;
            throw codebundler::CodeBundlerException("No hasher but supposed to verify");
        }
    } else {

        // 1 x x x
        if (parser.checksum_.empty()) {

            // 1 0 x x
            if (parser.options_.verify) {

                // 1 0 x 1
                // hasher and verifying but no checksum
                parser.options_.verbose > 0 && std::cerr << "No checksum.  Line " << parser.lineCount_ << std::endl;
                toSave = false;
                throw codebundler::ChecksumMismatchException(parser.filename_, parser.checksum_, calculatedChecksum);
            }
        } else {

            // 1 1 x x
            calculatedChecksum = parser.hasher_(fileContent);
            bool match = (calculatedChecksum == parser.checksum_);

            if (!match) {

                // 1 1 0 x
                if (parser.options_.verify) {

                    // 1 1 0 1
                    // verifying but checksum doesn't match
                    toSave = false;
                    if (parser.options_.verbose > 0) {
                        std::cerr << "Verifying but checksum mismatch" << std::endl;
                        std::cerr << "  Expected:   " << parser.checksum_ << std::endl;
                        std::cerr << "  Calculated: " << calculatedChecksum << std::endl;
                    }
                    throw codebundler::ChecksumMismatchException(parser.filename_, parser.checksum_, calculatedChecksum);
                }
            }
        }
    }

    parser.options_.verbose > 0 && std::cout << "  Saving file: '" << filepath << "'" << std::endl;

    // Create parent directories if they don't exist
    if (!parser.options_.trialRun && filepath.has_parent_path()) {
         if (parser.options_.verbose > 1) std::cout << "  Creating directories: " << filepath.parent_path() << std::endl;
         std::filesystem::create_directories(filepath.parent_path());
    }

    // Open file in binary mode to write content correctly
    std::ofstream fileStream;
    fileStream.exceptions(std::ios::badbit | std::ios::failbit); // Throw exceptions on failure
    if (parser.options_.verbose > 0) {
        std::cout << "  Opening file for writing: " << filepath << std::endl;
    }
    if (parser.options_.trialRun) {
        parser.options_.verbose > 0 && std::cout << "  Trial run: file not actually written." << std::endl;
    } else {
        fileStream.open(filepath, std::ios::out | std::ios::binary | std::ios::trunc);

        fileStream.write(fileContent.data(), fileContent.size());
        fileStream.close(); // Close explicitly after write
        if (parser.options_.verbose > 0) std::cout << "  File saved successfully: '" << parser.filename_ << "'" << std::endl;
    }

    // Reset state for the next file
    parser.filename_.clear();
    parser.checksum_.clear();
    parser.lines_.clear();
};

const std::function<void(const InputType&, BundleParser&)> BundleParser::skip =
    [](const InputType& input, BundleParser& parser) {
    if (parser.options_.verbose > 0) {
        if (input) {
             std::cout << "action: skip -> Skipping line: '" << input.value() << "'" << std::endl;
        } else {
             std::cout << "action: skip (on EOF)" << std::endl;
        }
    }
    // No actual state change needed for skipping
};

const std::function<void(const InputType&, BundleParser&)> BundleParser::done =
    [](const InputType& /*input*/, BundleParser& parser) {
    if (parser.options_.verbose > 0) std::cout << "action: done" << std::endl;
    // Final actions could happen here if needed, e.g., saving the last file if EOF acts as implicit separator
    // The current state machine seems to handle saveFile *before* transitioning based on EOF
};

const std::function<void(const InputType&, BundleParser&)> BundleParser::errorMissingFilename =
    [](const InputType& /*input*/, BundleParser& parser) {
    if (parser.options_.verbose > 0) std::cout << "action: errorMissingFilename" << std::endl;
    throw codebundler::BundleFormatException("Missing filename.");
};

const std::function<void(const InputType&, BundleParser&)> BundleParser::errorBadFormat =
    [](const InputType& /*input*/, BundleParser& parser) {
    if (parser.options_.verbose > 0) std::cout << "action: errorBadFormat" << std::endl;
    throw codebundler::BundleFormatException("Bad format.");
};

// ---------------------------------------------------------------------
const std::vector<Transition> BundleParser::transitions = {
    // currentState                       predicate               action                 nextState
    { ParserState::READ_SEPARATOR,          BundleParser::always,   BundleParser::rememberSeparator, ParserState::EXPECT_FILENAME_OR_COMMENT },

    { ParserState::EXPECT_FILENAME_OR_COMMENT, BundleParser::isFilename, BundleParser::rememberFilename, ParserState::EXPECT_CHECKSUM_OR_CONTENT },
    { ParserState::EXPECT_FILENAME_OR_COMMENT, BundleParser::isChecksum, BundleParser::errorMissingFilename, ParserState::DONE },
    { ParserState::EXPECT_FILENAME_OR_COMMENT, BundleParser::always,   BundleParser::skip,            ParserState::IN_COMMENT }, // If not filename, assume comment start

    { ParserState::IN_COMMENT,              BundleParser::isSeparator, BundleParser::skip,           ParserState::EXPECT_FILENAME }, // End comment on separator
    { ParserState::IN_COMMENT,              BundleParser::isEOF,       BundleParser::done,           ParserState::DONE }, // EOF ends comment block and parsing
    { ParserState::IN_COMMENT,              BundleParser::always,      BundleParser::skip,           ParserState::IN_COMMENT }, // Continue skipping comment lines

    { ParserState::EXPECT_CHECKSUM_OR_CONTENT, BundleParser::isChecksum, BundleParser::rememberChecksum, ParserState::IN_CONTENT },
    { ParserState::EXPECT_CHECKSUM_OR_CONTENT, BundleParser::isSeparator, BundleParser::saveFile,      ParserState::EXPECT_FILENAME }, // Empty content block, save (likely nothing), expect next file
    { ParserState::EXPECT_CHECKSUM_OR_CONTENT, BundleParser::isEOF,       BundleParser::errorBadFormat,      ParserState::DONE }, // End of file after filename (empty content)
    { ParserState::EXPECT_CHECKSUM_OR_CONTENT, BundleParser::always,      BundleParser::rememberContentLine, ParserState::IN_CONTENT }, // If not checksum/sep/EOF, assume content

    { ParserState::IN_CONTENT,              BundleParser::isSeparator, BundleParser::saveFile,       ParserState::EXPECT_FILENAME }, // End content block on separator
    { ParserState::IN_CONTENT,              BundleParser::isEOF,       BundleParser::saveFile,       ParserState::DONE }, // End content block on EOF
    { ParserState::IN_CONTENT,              BundleParser::always,      BundleParser::rememberContentLine, ParserState::IN_CONTENT }, // Continue reading content lines

    // Renamed state for clarity below
    { ParserState::EXPECT_FILENAME,         BundleParser::isFilename,  BundleParser::rememberFilename, ParserState::EXPECT_CHECKSUM_OR_CONTENT },
    { ParserState::EXPECT_FILENAME,         BundleParser::isEOF,       BundleParser::done,            ParserState::DONE }, // Expected filename but got EOF -> Done
    { ParserState::EXPECT_FILENAME,         BundleParser::always,      BundleParser::skip,            ParserState::IN_COMMENT } // If not filename/EOF after separator, treat as unexpected start of comment? Or error? Assuming comment/skip.

    // DONE state has no transitions out
};


// --- Public Method Definition ---
bool BundleParser::parse(const InputType& input) {
    
    lineCount_ += 1;

    if (options_.verbose > 1) {
        std::cout << "Parsing input: " << (input ? "'" + input.value() + "'" : "EOF")
                  << " in state: " << static_cast<int>(state_) << std::endl;
    }

    for (const auto& transition : transitions) {
        if (transition.currentState == state_) {
            // Evaluate predicate first
            bool predicateResult = transition.predicate(input, *this);
            // Verbose output moved into predicate lambdas

            if (predicateResult) {
                // Execute action
                transition.action(input, *this);
                // Verbose output moved into action lambdas

                ParserState previousState = state_;
                state_ = transition.nextState;

                if (options_.verbose > 0) {
                    std::cout << "Transition: State " << static_cast<int>(previousState)
                              << " -> State " << static_cast<int>(state_) << std::endl;
                }
                return state_ == ParserState::DONE; // Return true if parsing is finished
            }
            // else: predicate was false, try next transition for this state
        }
    }

    // If no transition matched for the current state and input
    std::cerr << "Error: No valid transition found from state "
              << static_cast<int>(state_)
              << " for input: "
              << (input ? "'" + input.value() + "'" : "EOF")
              << " line " << lineCount_
              << std::endl;
    // You might want to set state to an error state or throw
    throw std::runtime_error("Invalid state or input encountered during parsing");

    // return state_ == ParserState::DONE; // Or return false indicating error/not done
}
