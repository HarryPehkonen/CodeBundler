// src/file_combiner.cpp

#include "file_combiner.hpp"
#include "constants.hpp"
#include "utils.hpp"
#include <fstream>
#include <stdexcept>
#include <iostream>

FileCombiner::FileCombiner(const FileProcessor& processor, const bool verbose) : processor_(processor), verbose_(verbose) {}

void FileCombiner::combineFiles(const std::vector<std::filesystem::path>& directories, const std::string& output_filename) const {
    std::ofstream output_file(output_filename);
    if (!output_file) {
        throw std::runtime_error("Unable to open output file: " + output_filename);
    }
    combineFiles(directories, output_file);
}

void FileCombiner::combineFiles(const std::vector<std::filesystem::path>& directories, std::ostream& output) const {
    for (const auto& directory : directories) {
        if (!std::filesystem::exists(directory)) {

            // silently ignore directories that don't exist
            continue;
        }

        verbose_ && std::cerr << "Processing directory: " << directory << std::endl;

        for (const auto& file_path : processor_.getMatchingFiles(directory)) {
            verbose_ && std::cerr << "Processing file:  " << file_path.relative_path().string() << std::endl;
            output << BOUNDARY_STRING << '\n';
            output << "Path: " << file_path.relative_path().string() << '\n' << '\n';
            
            std::ifstream input_file(file_path);
            if (!input_file) {
                throw std::runtime_error("Unable to open input file: " + file_path.string());
            }
            
            // read the text from the file
            std::string txt((std::istreambuf_iterator<char>(input_file)), std::istreambuf_iterator<char>());
            output << txt;
            output << '\n';

            verbose_ && std::cerr << "Processed file: " << file_path.relative_path().string() << " at " << txt.length() << std::endl;
        }
    }

    output << BOUNDARY_STRING << '\n';
}
