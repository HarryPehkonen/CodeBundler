#ifndef CODEBUNDLER_OPTIONS_HPP
#define CODEBUNDLER_OPTIONS_HPP

#include <string>

namespace codebundler {

struct Options {

    // by default:  strict and quiet
    bool trialRun = false; // do we actually write the files?
    bool verify = true; // do mismatching checksums matter?
    int verbose = 0; // 0 - silent, 1 - errors, 2 - warnings, 3 - info,
                     // 4 - debug
    std::string separator = "========= BOUNDARY ==========";
};

}
#endif // CODEBUNDLER_OPTIONS_HPP
