// src/file_extractor.cpp

#include "file_extractor.hpp"
#include "constants.hpp"
#include "utils.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

void FileExtractor::extractFiles(const std::string& input_filename) const {
    std::ifstream input_file(input_filename);
    if (!input_file) {
        throw std::runtime_error("Unable to open input file: " + input_filename);
    }

    std::string line;
    std::string current_file;
    std::ostringstream current_content;
    bool expect_empty_line = false;

    auto writeFile = [this](const std::string& file, const std::string& content) {
        if (!file.empty()) {
            createDirectoryIfNotExists(std::filesystem::path(file).parent_path());
            std::ofstream output_file(file);
            if (!output_file) {
                throw std::runtime_error("Unable to create output file: " + file);
            }
            output_file << content;
        }
    };

    while (std::getline(input_file, line)) {
        if (line == BOUNDARY_STRING) {
            writeFile(current_file, current_content.str());
            current_file.clear();
            current_content.str("");
            current_content.clear();
            expect_empty_line = false;
        } else if (line.starts_with("Path: ")) {
            current_file = line.substr(6);
            expect_empty_line = true;
        } else if (expect_empty_line) {
            if (!line.empty()) {
                throw std::runtime_error("Expected empty line after Path, but got: " + line);
            }
            expect_empty_line = false;
        } else {
            current_content << line << '\n';
        }
    }

    // Handle the last file
    writeFile(current_file, current_content.str());
}

void FileExtractor::createDirectoryIfNotExists(const std::filesystem::path& path) const {
    if (!std::filesystem::exists(path)) {
        std::filesystem::create_directories(path);
    }
}
