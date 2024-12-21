// src/main.cpp

#include "file_processor.hpp"
#include "file_combiner.hpp"
#include "file_extractor.hpp"
#include "constants.hpp"
#include <iostream>
#include <string>
#include <filesystem>

void showExample() {
    std::cout << EXAMPLE << std::endl;
}

void showUsage(std::string_view my_name) {
    std::cerr << "Usage:\n";
    std::cerr << my_name << " [combine|extract] [<filename>] [-v|--verbose]\n";
    std::cerr << my_name << " example\n";
    std::cerr << std::endl;
    std::cerr << "If <filename> is not provided, the program will read from stdin and write to stdout.\n";
}

int main(int argc, char* argv[]) {

    std::string my_name;
    std::string mode;
    std::string filename;
    bool verbose = false;

    for (int i = 0; i < argc; i++) {
        std::string arg = argv[i];
        if (i == 0) {
            my_name = arg;
        } else if (i == 1) {
            mode = arg;
        } else if (i == 2) {
            filename = arg;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else {
            std::cerr << "Ignoring extra argument: " << arg << std::endl;
        }
    }

    try {
        if (mode == "combine") {
            verbose && std::cerr << "Combining files..." << std::endl;
            FileProcessor processor(verbose);
            std::vector<std::filesystem::path> directories = {
                TOP_DIR,
                SRC_DIR,
                TESTS_DIR,
                CMAKE_DIR,
                EXAMPLES_DIR,
                INCLUDE_DIR,
                GITHUB_DIR
            };
            processor.addMatcher(std::make_unique<ExtensionMatcher>(".cpp"));
            processor.addMatcher(std::make_unique<ExtensionMatcher>(".hpp"));
            processor.addMatcher(std::make_unique<ExtensionMatcher>(".h"));
            processor.addMatcher(std::make_unique<ExactNameMatcher>("CMakeLists.txt"));
            processor.addMatcher(std::make_unique<ExtensionMatcher>(".cmake"));
            processor.addMatcher(std::make_unique<ExtensionMatcher>(".cmake.in"));
            processor.addMatcher(std::make_unique<ExactNameMatcher>("ci.yml"));
            processor.addMatcher(std::make_unique<ExactNameMatcher>("README.md"));
            processor.addMatcher(std::make_unique<ExactNameMatcher>("LICENSE"));

            if (verbose) {
                for (const auto& directory : directories) {
                    std::cerr << "Directory: " << directory << std::endl;
                }
            }

            FileCombiner combiner(processor, verbose);
            if (filename.empty()) {
                combiner.combineFiles(directories, std::cout);
                std::cerr << "Files combined successfully to stdout" << std::endl;
            } else {
                combiner.combineFiles(directories, filename);
                std::cout << "Files combined successfully into " << filename << std::endl;
            }
        } else if (mode == "extract") {
            FileExtractor extractor(verbose);
            if (filename.empty()) {
                extractor.extractFiles(std::cin);
                std::cerr << "Files extracted successfully from stdin" << std::endl;
            } else {
                extractor.extractFiles(filename);
                std::cout << "Files extracted successfully from " << filename << std::endl;
            }
        } else if (mode == "example") {
            showExample();
            return 0;
        } else {
            showUsage(my_name);
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
