#ifndef BUNDLEPARSER_HPP
#define BUNDLEPARSER_HPP

#include "options.hpp"
#include "FSMgine/FSMgine.hpp"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

// Forward declaration (though not strictly necessary inside its own header, good practice if other headers might include this selectively)
class BundleParser;

// Type Aliases
using InputType = std::optional<std::string>;
inline const InputType INPUT_EOF = std::nullopt; // Define INPUT_EOF associated with InputType

class BundleParser {
public:
    // Public type alias for Hasher
    using Hasher = std::function<std::string(const std::string&)>;

    // --- Constructor ---
    BundleParser(const codebundler::Options& options, Hasher hasher, std::filesystem::path outputPath = ".");

    // --- Public Methods ---
    bool haveContent() const
    {
        return !lines_.empty();
    }
    bool parse(const InputType& input);

private:
    // FSM for state management
    fsmgine::FSM<InputType> fsm_;
    
    int lineCount_ = 0;
    codebundler::Options options_;
    Hasher hasher_;
    std::string separator_; // Determined at runtime
    std::string filename_;
    std::string checksum_;
    std::filesystem::path outputPath_;
    std::vector<std::string> lines_;

    // --- Private Helper Methods ---
    std::string trim(const std::string& str);

    // --- Predicates (converted from static to member functions) ---
    bool isAlways(const InputType& input) const { return true; }
    bool isSeparator(const InputType& input) const;
    bool isFilename(const InputType& input) const;
    bool isChecksum(const InputType& input) const;
    bool isEOF(const InputType& input) const;

    // --- Actions (converted from static to member functions) ---
    void rememberSeparator(const InputType& input);
    void rememberFilename(const InputType& input);
    void rememberChecksum(const InputType& input);
    void rememberContentLine(const InputType& input);
    void saveFile(const InputType& input);
    void skip(const InputType& input);
    void errorMissingFilename(const InputType& input);
    void errorBadFormat(const InputType& input);
    void done(const InputType& input);
};

#endif // BUNDLEPARSER_HPP
