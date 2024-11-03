// src/file_extractor.hpp

#pragma once

#include <string>
#include <filesystem>
#include <iostream>

class FileExtractor {
public:
    explicit FileExtractor(bool verbose = false);
    void extractFiles(const std::string& input_filename) const;
    void extractFiles(std::istream& input, std::ostream* output = nullptr) const;

private:
    void createDirectoryIfNotExists(const std::filesystem::path& path) const;
    const bool verbose_;
};
