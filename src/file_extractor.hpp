// src/file_extractor.hpp

#pragma once

#include <string>
#include <filesystem>

class FileExtractor {
public:
    void extractFiles(const std::string& input_filename) const;

private:
    void createDirectoryIfNotExists(const std::filesystem::path& path) const;
};
