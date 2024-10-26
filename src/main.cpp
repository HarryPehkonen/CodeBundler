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
    std::cerr << my_name << " [combine|extract] [<filename>]\n";
    std::cerr << my_name << " example\n";
    std::cerr << std::endl;
    std::cerr << "If <filename> is not provided, the program will read from stdin and write to stdout.\n";
}

int main(int argc, char* argv[]) {
    std::string my_name = argv[0];
    std::string mode = argc >= 2 ? argv[1] : "";
    std::string filename = argc >= 3 ? argv[2] : "";

    try {
        if (mode == "combine") {
            FileProcessor processor;
            processor.addMatcher(std::make_unique<ExtensionMatcher>(".cpp"));
            processor.addMatcher(std::make_unique<ExtensionMatcher>(".hpp"));
            processor.addMatcher(std::make_unique<ExactNameMatcher>("CMakeLists.txt"));

            FileCombiner combiner(processor);
            if (filename.empty()) {
                combiner.combineFiles({SRC_DIR, TESTS_DIR}, std::cout);
                std::cerr << "Files combined successfully to stdout" << std::endl;
            } else {
                combiner.combineFiles({SRC_DIR, TESTS_DIR}, filename);
                std::cout << "Files combined successfully into " << filename << std::endl;
            }
        } else if (mode == "extract") {
            FileExtractor extractor;
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
            showUsage(std::string_view(argv[0]));
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
