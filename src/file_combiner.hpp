// src/file_combiner.hpp

#pragma once

#include "file_processor.hpp"
#include <string>
#include <vector>
#include <filesystem>

class FileCombiner {
public:
    explicit FileCombiner(const FileProcessor& processor);
    void combineFiles(const std::vector<std::filesystem::path>& directories, const std::string& output_filename) const;

private:
    const FileProcessor& processor_;
};