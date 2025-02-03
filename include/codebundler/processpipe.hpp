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
        std::string current_line;

        while (fgets(buffer, sizeof(buffer), pipe_) != nullptr) {
            current_line += buffer;
            size_t pos = 0;
            size_t next;
            
            while ((next = current_line.find('\n', pos)) != std::string::npos) {
                lines.push_back(current_line.substr(pos, next - pos));
                pos = next + 1;
            }
            
            current_line = current_line.substr(pos);
        }

        if (!current_line.empty()) {
            lines.push_back(std::move(current_line));
        }

        int status = pclose(pipe_);
        pipe_ = nullptr;  // Prevent double-close in destructor

        #ifdef _WIN32
        if (status != 0) {
            throw std::runtime_error("Process failed with status " + 
                std::to_string(status) + ": " + command_);
        }
        #else
        if (status == -1) {
            throw std::runtime_error("Failed to get exit status for: " + command_);
        }
        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            if (exit_status != 0) {
                throw std::runtime_error("Process failed with status " + 
                    std::to_string(exit_status) + ": " + command_);
            }
        } else {
            throw std::runtime_error("Process terminated abnormally: " + command_);
        }
        #endif

        return lines;
    }
};

} // namespace codebundler

#endif // CODEBUNDLER_PROCESSPIPE_HPP
