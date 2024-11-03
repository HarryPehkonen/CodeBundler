// src/file_processor.hpp

#pragma once

#include "file_matcher.hpp"
#include <vector>
#include <memory>
#include <filesystem>

class FileProcessor {
public:
    explicit FileProcessor(bool verbose = false);
    void addMatcher(std::unique_ptr<FileMatcherInterface> matcher);
    [[nodiscard]] bool shouldProcessFile(const std::filesystem::path& path) const;
    [[nodiscard]] std::vector<std::filesystem::path> getMatchingFiles(const std::filesystem::path& directory) const;

private:
    std::vector<std::unique_ptr<FileMatcherInterface>> matchers_;
    const bool verbose_;
};
