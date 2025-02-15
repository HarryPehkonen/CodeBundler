// include/codebundler/git_loader.hpp
#pragma once

#include "handles.hpp"
#include "errors.hpp"
#include <vector>
#include <string>

namespace codebundler {

class GitFileLoader {
public:
    static std::vector<std::string> getTrackedFiles() {
        ProcessHandle git("git ls-files");
        auto result = readAllLines(git.get());

        if (result.empty()) {
            throw BundleError("No files found in git repository");
        }

        return result;
    }

private:
    static std::vector<std::string> readAllLines(FILE* pipe) {
        std::vector<std::string> lines;
        char buffer[4096];

        while (fgets(buffer, sizeof(buffer), pipe)) {
            lines.emplace_back(buffer);
            if (!lines.back().empty() && lines.back().back() == '\n') {
                lines.back().pop_back();
            }
        }

        if (ferror(pipe)) {
            throw BundleError("Error reading from git process");
        }

        return lines;
    }
};

} // namespace codebundler
