#include "bundler.hpp"
#include "exceptions.hpp"
#include "unbundler.hpp"
#include "utilities.hpp" // May need utilities here too
#include <filesystem> // Requires C++17
#include <iostream>
#include <string>
#include <vector>

namespace { // Use an anonymous namespace for local helpers

void printUsage()
{
    std::cerr << R"(Usage: codebundler <command> [options] [args...]

Commands:
  bundle [output_file]         Bundle tracked files. Writes to stdout if no output_file.
    --separator <sep>          Specify a custom separator string (default: "--- // ---").
    --description <desc>       Add a description to the bundle header.

  unbundle [input_file] [output_dir] Unbundle files from archive. Reads from stdin if no input_file.
                                     Extracts to current directory if no output_dir.
    --separator <sep>          Specify the separator used in the bundle (default: "--- // ---").
    --no-verify                Disable SHA256 checksum verification during unbundling.

  verify [input_file]          Verify checksums in the bundle without extracting. Reads from stdin if no input_file.
    --separator <sep>          Specify the separator used in the bundle (default: "--- // ---").

Options:
  -h, --help                   Show this help message.
)";
}

// Simple argument parsing helper
struct Arguments {
    std::string command;
    std::string inputFile;
    std::string outputFile;
    std::string outputDir = "."; // Default to current directory for unbundle
    std::string separator = "========== BOUNDARY =========="; // Default separator
    std::string description;
    bool noVerify = false;
    bool showHelp = false;
};

// VERY basic argument parser. Consider a library (like CLI11) for more complex needs.
Arguments parseArguments(int argc, char* argv[])
{
    Arguments args;
    std::vector<std::string> tokens(argv + 1, argv + argc); // Skip executable name

    if (tokens.empty()) {
        args.showHelp = true;
        return args;
    }

    args.command = tokens[0];
    if (args.command == "-h" || args.command == "--help") {
        args.showHelp = true;
        return args;
    }

    size_t currentArg = 1;
    while (currentArg < tokens.size()) {
        const std::string& token = tokens[currentArg];

        if (token == "--separator") {
            if (++currentArg < tokens.size()) {
                args.separator = tokens[currentArg];
            } else {
                throw codebundler::ArgumentParserException("--separator requires an argument.");
            }
        } else if (token == "--no-verify") {
            args.noVerify = true;
        } else if (token == "--description") {
            if (++currentArg < tokens.size()) {
                args.description = tokens[currentArg];
            } else {
                throw codebundler::ArgumentParserException("--description requires an argument.");
            }
        } else if (token == "-h" || token == "--help") {
            args.showHelp = true;
            return args; // Stop parsing if help is requested
        } else if (token.rfind("--", 0) == 0) {
            throw codebundler::ArgumentParserException("Unknown option: " + token);
        } else {
            // Assume positional arguments
            if (args.command == "bundle") {
                if (args.outputFile.empty()) {
                    args.outputFile = token; // First positional arg is output file
                } else {
                    throw codebundler::ArgumentParserException("Unexpected positional argument for bundle: " + token);
                }
            } else if (args.command == "unbundle" || args.command == "verify") {
                if (args.inputFile.empty()) {
                    args.inputFile = token; // First positional arg is input file
                } else if (args.command == "unbundle" && args.outputDir == ".") { // Only set outputDir if not already set
                    args.outputDir = token; // Second positional arg is output dir
                } else {
                    throw codebundler::ArgumentParserException("Unexpected positional argument for " + args.command + ": " + token);
                }
            } else {
                throw codebundler::ArgumentParserException("Unknown command or misplaced argument: " + token);
            }
        }
        currentArg++;
    }

    // Post-parsing validation
    if (args.command != "bundle" && args.command != "unbundle" && args.command != "verify") {
        throw codebundler::ArgumentParserException("Invalid command: " + args.command);
    }
    if (args.noVerify && args.command != "unbundle") {
        throw codebundler::ArgumentParserException("--no-verify is only applicable to the 'unbundle' command.");
    }
    if (!args.description.empty() && args.command != "bundle") {
        throw codebundler::ArgumentParserException("--description is only applicable to the 'bundle' command.");
    }

    return args;
}

} // anonymous namespace

int main(int argc, char* argv[])
{
    try {
        Arguments args = parseArguments(argc, argv);

        if (args.showHelp) {
            printUsage();
            return 0;
        }

        if (args.command == "bundle") {
            codebundler::Bundler bundler(args.separator);
            if (args.outputFile.empty()) {
                // Bundle to standard output
                bundler.bundleToStream(std::cout, args.description);
            } else {
                // Bundle to a file
                bundler.bundleToFile(args.outputFile, args.description);
            }
            std::cerr << "Bundling completed successfully." << std::endl; // Use cerr for status messages not part of bundle output

        } else if (args.command == "unbundle") {
            codebundler::Unbundler::Options options;
            options.separator = args.separator;
            options.verifyChecksums = !args.noVerify; // Note the inversion

            codebundler::Unbundler unbundler(options);
            std::filesystem::path outputDir = args.outputDir; // Convert string to path

            if (args.inputFile.empty()) {
                // Unbundle from standard input
                unbundler.unbundleFromStream(std::cin, outputDir);
            } else {
                // Unbundle from a file
                unbundler.unbundleFromFile(args.inputFile, outputDir);
            }
            std::cerr << "Unbundling completed successfully." << std::endl;

        } else if (args.command == "verify") {
            codebundler::Unbundler::Options options;
            options.separator = args.separator;
            options.verifyChecksums = true; // Verification is the purpose

            codebundler::Unbundler unbundler(options);
            bool success = false;
            if (args.inputFile.empty()) {
                // Verify from standard input
                success = unbundler.verifyBundleStream(std::cin);
            } else {
                // Verify from a file
                success = unbundler.verifyBundleFile(args.inputFile);
            }

            if (success) {
                std::cerr << "Bundle verification successful." << std::endl;
                return 0; // Explicit success return
            } else {
                // Error message already printed by verifyBundleStream/File on failure (mismatch)
                std::cerr << "Bundle verification failed." << std::endl;
                return 1; // Explicit failure return
            }
        }
        // No else needed because parseArguments should have thrown if command was invalid

    } catch (const codebundler::ArgumentParserException& e) {
        std::cerr << "Argument Error: " << e.what() << std::endl
                  << std::endl;
        printUsage();
        return 1;
    } catch (const codebundler::CodeBundlerException& e) {
        // Catch specific bundler exceptions (IO, Git, Format, Checksum)
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        // Catch any other standard exceptions
        std::cerr << "An unexpected error occurred: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        // Catch any non-standard exceptions
        std::cerr << "An unknown error occurred." << std::endl;
        return 1;
    }

    return 0; // Success
}
