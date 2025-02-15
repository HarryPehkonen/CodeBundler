// src/main.cpp
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include "codebundler/errors.hpp"
#include "codebundler/codebundler.hpp"
#include "codebundler/git_loader.hpp"
#include <iostream>
#include <memory>

using namespace codebundler;

void printUsage(const char* program);

void printUsage(const char* program) {
    std::cerr << "Usage: " << program << " [command] [options]\n"
              << "Commands:\n"
              << "  bundle [output_file]     Bundle files (default: stdout)\n"
              << "  unbundle [input_file] [output_dir]\n"
              << "                           Unbundle files (default: stdin)\n"
              << "  verify [bundle_file]     Verify bundle integrity\n";
}

int main(int argc, char* argv[]) try {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    const std::string_view command{argv[1]};

    if (command == "bundle") {
        std::unique_ptr<std::ofstream> output_file;
        std::ostream* output = &std::cout;

        if (argc > 2) {
            output_file = std::make_unique<std::ofstream>(argv[2]);
            if (!*output_file) {
                throw BundleError("Failed to open output file");
            }
            output = output_file.get();
        }

        const auto files = GitFileLoader::getTrackedFiles();
        CodeBundler::bundle(*output, files);
    }
    else if (command == "unbundle") {
        std::unique_ptr<std::ifstream> input_file;
        std::istream* input = &std::cin;

        if (argc > 2) {
            input_file = std::make_unique<std::ifstream>(argv[2]);
            if (!*input_file) {
                throw BundleError("Failed to open input file");
            }
            input = input_file.get();
        }

        std::filesystem::path output_dir = argc > 3 ? argv[3] : "";
        CodeBundler::unbundle(*input, output_dir);
    }
    else if (command == "verify") {
        if (argc < 3) {
            throw BundleError("verify requires input file");
        }

        std::ifstream input(argv[2]);
        if (!input) {
            throw BundleError("Failed to open bundle file");
        }

        return CodeBundler::verify(input) ? 0 : 1;
    }
    else {
        throw BundleError("Unknown command: " + std::string(command));
    }

    return 0;
}
catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << '\n';
    return 1;
}
