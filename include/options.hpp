#ifndef CODEBUNDLER_OPTIONS_HPP
#define CODEBUNDLER_OPTIONS_HPP

#include <string>
#include <vector>

namespace codebundler {

struct Options {

    // by default:  strict and quiet
    bool trialRun = false; // do we actually write the files?
    bool verify = true; // do mismatching checksums matter?
    int verbose = 0; // 0 - silent, 1 - errors, 2 - warnings, 3 - info,
                     // 4 - debug
    std::string separator = "========= BOUNDARY ==========";

    // --- Selection (bundle only) ---
    // Patterns are matched against the path as it appears in the bundle; see
    // the PATTERNS section of codebundler(1). Naming even one include pattern
    // switches the bundle to include mode: only what a rule admits is bundled,
    // instead of everything tracked except what a rule removes.
    std::vector<std::string> includePatterns; // --include: named things to keep
    std::vector<std::string> excludePatterns; // --exclude: named things to leave out
    // --list: report what would be bundled (sizes included) and write nothing.
    bool listOnly = false;
};

}
#endif // CODEBUNDLER_OPTIONS_HPP
