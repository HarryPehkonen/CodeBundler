#ifndef BUNDLEPARSER_HPP
#define BUNDLEPARSER_HPP

#include "options.hpp"

#include <functional>
#include <vector>
#include <string>
#include <optional>
#include <filesystem>

// Forward declaration (though not strictly necessary inside its own header, good practice if other headers might include this selectively)
class BundleParser;

// Constants - Using 'inline const' (C++17) to define them in the header without ODR issues
inline const std::string FILENAME_PREFIX = "Filename: ";
inline const std::string CHECKSUM_PREFIX = "Checksum: SHA256:";

// Type Aliases
using InputType = std::optional<std::string>;
inline const InputType INPUT_EOF = std::nullopt; // Define INPUT_EOF associated with InputType

// Enum for Parser State
enum class ParserState {
    READ_SEPARATOR,
    EXPECT_FILENAME_OR_COMMENT,
    EXPECT_FILENAME,
    IN_COMMENT,
    EXPECT_CHECKSUM_OR_CONTENT,
    IN_CONTENT,
    DONE
};

// Transition Struct Definition (needed for the 'transitions' member declaration)
// Made public nested type for clarity or could be defined outside the class before BundleParser
struct Transition {
    ParserState currentState;
    std::function<bool(const InputType&, BundleParser&)> predicate;
    std::function<void(const InputType&, BundleParser&)> action;
    ParserState nextState;
};


class BundleParser {
public:
    // Public type alias for Hasher
    using Hasher = std::function<std::string(const std::string&)>;

    // --- Constructor ---
    BundleParser(const codebundler::Options& options, Hasher hasher, std::filesystem::path outputPath = ".");

    // --- Public Methods ---
    bool haveContent() const {
        return !lines_.empty();
    }
    bool parse(const InputType& input);

private:
    int lineCount_ = 0;
    codebundler::Options options_;
    Hasher hasher_;
    std::string separator_; // Determined at runtime
    std::string filename_;
    std::string checksum_;
    std::filesystem::path outputPath_;
    std::vector<std::string> lines_;
    ParserState state_ = ParserState::READ_SEPARATOR; // In-class initializer

    // --- Private Helper Methods ---
    // Declaration only
    std::string trim(const std::string& str);

    // --- Static Predicates & Actions (Declarations) ---
    // These are implementation details tied to the state machine logic.
    // Declaring them static makes sense if they don't depend on instance state
    // other than what's passed in (the BundleParser&).
    // We declare the function variables here.
    static const std::function<bool(const InputType&, BundleParser&)> always;
    static const std::function<bool(const InputType&, BundleParser&)> isSeparator;
    static const std::function<bool(const InputType&, BundleParser&)> isFilename;
    static const std::function<bool(const InputType&, BundleParser&)> isChecksum;
    static const std::function<bool(const InputType&, BundleParser&)> isEOF;

    static const std::function<void(const InputType&, BundleParser&)> rememberSeparator;
    static const std::function<void(const InputType&, BundleParser&)> rememberFilename;
    static const std::function<void(const InputType&, BundleParser&)> rememberChecksum;
    static const std::function<void(const InputType&, BundleParser&)> rememberContentLine;
    static const std::function<void(const InputType&, BundleParser&)> saveFile;
    static const std::function<void(const InputType&, BundleParser&)> skip;
    static const std::function<void(const InputType&, BundleParser&)> errorMissingFilename;
    static const std::function<void(const InputType&, BundleParser&)> errorBadFormat;
    static const std::function<void(const InputType&, BundleParser&)> done;

    // --- State Machine Transitions (Declaration) ---
    // Declared static const because the transitions are fixed for the class.
    // The actual definition/initialization goes in the .cpp file.
    static const std::vector<Transition> transitions;
};

#endif // BUNDLEPARSER_HPP
