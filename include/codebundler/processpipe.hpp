#ifndef CODEBUNDLER_PROCESSPIPE_HPP
#define CODEBUNDLER_PROCESSPIPE_HPP

#include <string>
#include <vector>
#include <stdexcept>
#include <cstdio>   // for popen, pclose, FILE
#include <cerrno>   // for errno

#ifdef _WIN32
#include <process.h> // for _popen, _pclose
#define popen _popen
#define pclose _pclose
#endif

namespace codebundler {

class ProcessPipe {
private:
    FILE* pipe_;
    std::string command_;

    ProcessPipe(const ProcessPipe&) = delete;
    ProcessPipe& operator=(const ProcessPipe&) = delete;

public:
    explicit ProcessPipe(const std::string& command) 
        : pipe_(popen((command + " 2>&1").c_str(), "r"))
        , command_(command) {
        if (!pipe_) {
            throw std::runtime_error("Failed to execute: " + command);
        }
    }

    ~ProcessPipe() {
        if (pipe_) {
            pclose(pipe_);
        }
    }

std::vector<std::string> readLines() {
    std::vector<std::string> lines;
    char buffer[4096];

    bool has_output = false;
    while (fgets(buffer, sizeof(buffer), pipe_)) {
        has_output = true;
        lines.emplace_back(buffer);
        if (!lines.back().empty() && lines.back().back() == '\n') {
            lines.back().pop_back();
        }
    }

    int status = pclose(pipe_);
    pipe_ = nullptr;

    if (status != 0 || !has_output) {
        throw BundleError("Process failed: " + command_);
    }

    return lines;
}
};

} // namespace codebundler

#endif // CODEBUNDLER_PROCESSPIPE_HPP
