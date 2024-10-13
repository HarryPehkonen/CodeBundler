// src/file_combiner.cpp

#include "file_combiner.hpp"
#include "constants.hpp"
#include "utils.hpp"
#include <fstream>
#include <stdexcept>

FileCombiner::FileCombiner(const FileProcessor& processor) : processor_(processor) {}

void FileCombiner::combineFiles(const std::vector<std::filesystem::path>& directories, const std::string& output_filename) const {
    std::ofstream output_file(output_filename);
    if (!output_file) {
        throw std::runtime_error("Unable to open output file: " + output_filename);
    }

    for (const auto& directory : directories) {
        for (const auto& file_path : processor_.getMatchingFiles(directory)) {
            output_file << BOUNDARY_STRING << '\n';
            output_file << "Path: " << file_path.relative_path().string() << '\n' << '\n';
            
            std::ifstream input_file(file_path);
            if (!input_file) {
                throw std::runtime_error("Unable to open input file: " + file_path.string());
            }
            
            output_file << input_file.rdbuf();
            output_file << '\n';
        }
    }

    output_file << BOUNDARY_STRING << '\n';
}
