// include/codebundler/options.hpp
#pragma once

#include "errors.hpp"
#include <string>

namespace codebundler {

class Options {
    std::string separator_;
    std::string description_;

public:
    Options(std::string sep = "---------- BOUNDARY ----------",
            std::string desc = "Description: This is a concatenation of several files.\n")
        : separator_(validateSeparator(std::move(sep)))
        , description_(std::move(desc)) {}

    const std::string& separator() const { return separator_; }
    const std::string& description() const { return description_; }

private:
    static std::string validateSeparator(std::string sep) {
        if (sep.empty() || sep.find('\n') != std::string::npos) {
            throw BundleError("Invalid separator");
        }
        return sep;
    }
};

} // namespace codebundler
