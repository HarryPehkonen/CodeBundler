#include "codebundler/codebundler.hpp"
#include "codebundler/processpipe.hpp"
#include <vector>
#include <string>
#include <iostream>
#include <memory>

// Helper function to get git-tracked files
std::vector<std::string> get_git_files() {
    try {
        codebundler::ProcessPipe pipe("git ls-files");
        return pipe.readLines();
    } catch (const std::runtime_error& e) {
        throw codebundler::BundleError("Failed to execute git ls-files: " + std::string(e.what()));
    }
}

void print_usage(const char* program) {
    std::cerr << "Usage: " << program << " [command] [options]\n"
              << "Commands:\n"
              << "  bundle [output_file]     Bundle files (default: output to stdout)\n"
              << "  unbundle [input_file] [output_dir]\n"
              << "                           Unbundle files (default: read from stdin)\n"
              << "  verify [bundle_file]     Verify bundle integrity\n";
}

int main(int argc, char* argv[]) {
    try {
        if (argc < 2) {
            print_usage(argv[0]);
            return 1;
        }

        std::string command = argv[1];

        if (command == "bundle") {
            // Handle bundle command
            std::unique_ptr<std::ofstream> output_file;
            std::ostream* output = &std::cout;  // Default to stdout

            if (argc >= 3) {
                // Open output file if specified
                output_file = std::make_unique<std::ofstream>(argv[2]);
                if (!*output_file) {
                    throw codebundler::BundleError("Cannot open output file: " + std::string(argv[2]));
                }
                output = output_file.get();
            }

            // Get list of files from git
            std::vector<std::string> files;
            try {
                files = get_git_files();
            } catch (const codebundler::BundleError& e) {
                std::cerr << "Warning: Git repository not found. No files to bundle.\n";
                return 1;
            }

            // Perform bundling
            codebundler::CodeBundler::bundle(*output, files);
        }
        else if (command == "unbundle") {
            // Handle unbundle command
            std::unique_ptr<std::ifstream> input_file;
            std::istream* input = &std::cin;  // Default to stdin

            if (argc >= 3) {
                // Open input file if specified
                input_file = std::make_unique<std::ifstream>(argv[2]);
                if (!*input_file) {
                    throw codebundler::BundleError("Cannot open input file: " + std::string(argv[2]));
                }
                input = input_file.get();
            }

            // Handle output directory
            std::string output_dir;
            if (argc >= 4) {
                output_dir = argv[3];
            }

            // Perform unbundling
            codebundler::CodeBundler::unbundle(*input, output_dir);
        }
        else if (command == "verify") {
            if (argc < 3) {
                throw codebundler::BundleError("Verify command requires a bundle file");
            }

            // Open bundle file for verification
            std::ifstream bundle_file(argv[2]);
            if (!bundle_file) {
                throw codebundler::BundleError("Cannot open bundle file: " + std::string(argv[2]));
            }

            // Perform verification
            bool is_valid = codebundler::CodeBundler::verify(bundle_file);
            
            if (is_valid) {
                std::cout << "Bundle verification successful\n";
                return 0;
            } else {
                std::cout << "Bundle verification failed\n";
                return 1;
            }
        }
        else {
            throw codebundler::BundleError("Unknown command: " + command);
        }

        return 0;
    }
    catch (const codebundler::BundleError& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << "\n";
        return 1;
    }
}
