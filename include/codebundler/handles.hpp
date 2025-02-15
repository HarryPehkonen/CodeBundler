// include/codebundler/handles.hpp
#pragma once

#include "errors.hpp"
#include <memory>
#include <fstream>
#include <filesystem>
#include <cstdio>

#ifdef _WIN32
#include <process.h>
#define popen _popen
#define pclose _pclose
#endif

namespace codebundler {

class FileHandle {
private:
    std::unique_ptr<std::fstream> file_;
    std::filesystem::path path_;

public:
    explicit FileHandle(const std::filesystem::path& path, 
                       std::ios_base::openmode mode = std::ios_base::in)
        : file_(std::make_unique<std::fstream>(path, mode))
        , path_(path) {
        if (!file_ || !*file_) {
            throw BundleError("Failed to open: " + path.string());
        }
    }

    std::fstream& get() { return *file_; }
    const std::fstream& get() const { return *file_; }
    const std::filesystem::path& path() const { return path_; }
};

class ProcessHandle {
private:
    struct PipeDeleter {
        void operator()(FILE* fp) { 
            if(fp) {
                #ifdef _WIN32
                _pclose(fp);
                #else
                pclose(fp);
                #endif
            }
        }
    };

    std::unique_ptr<FILE, PipeDeleter> pipe_;
    std::string command_;

public:
    explicit ProcessHandle(std::string cmd)
        : pipe_(popen(cmd.c_str(), "r"))
        , command_(std::move(cmd)) {
        if (!pipe_) {
            throw BundleError("Failed to execute: " + command_);
        }
    }

    FILE* get() const { return pipe_.get(); }
    const std::string& command() const { return command_; }

    // Prevent copying
    ProcessHandle(const ProcessHandle&) = delete;
    ProcessHandle& operator=(const ProcessHandle&) = delete;
};

} // namespace codebundler
