#include "bundler.hpp"
#include "bundleparser.hpp"
#include "options.hpp"
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
    --separator <sep>          Specify a custom separator string (default: "========== BOUNDARY ==========").
    --description <desc>       Add an optional description to the bundle header.

  unbundle [input_file] [output_dir] Unbundle files from archive. Reads from stdin if no input_file.
                                     Extracts to current directory if no output_dir.
                                     (Separator is detected automatically from the first line).
    --no-verify                Disable SHA256 checksum verification during unbundling.
  verify [input_file]          Verify checksums in the bundle without extracting. Reads from stdin if no input_file.
                                     (Separator is detected automatically from the first line).

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
    std::string separator = "========== BOUNDARY =========="; // Default separator (only for bundle)
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
                if (args.command == "unbundle" || args.command == "verify") {
                    throw codebundler::ArgumentParserException("--separator is not used for unbundle or verify (it's auto-detected).");
                }
            } else {
                throw codebundler::ArgumentParserException("--separator requires an argument.");
            }
        } else if (token == "--no-verify") {
            // Allow --no-verify only for unbundle command
            if (args.command != "unbundle") {
                 throw codebundler::ArgumentParserException("--no-verify is only applicable to the 'unbundle' command.");
            }
            args.noVerify = true;
        } else if (token == "--description") {
             // Allow --description only for bundle command
            if (args.command != "bundle") {
                 throw codebundler::ArgumentParserException("--description is only applicable to the 'bundle' command.");
            }
            if (++currentArg < tokens.size()) {
                args.description = tokens[currentArg];
            } else {
                throw codebundler::ArgumentParserException("--description requires an argument.");
            }
        } else if (token == "-h" || token == "--help") {
            args.showHelp = true;
            return args; // Stop parsing if help is requested
        } else if (token.rfind("--", 0) == 0) {
            // Unknown option starting with --
            throw codebundler::ArgumentParserException("Unknown option: " + token);
        } else {
            // Assume positional arguments based on command
            if (args.command == "bundle") {
                if (args.outputFile.empty()) {
                    args.outputFile = token; // First positional arg is output file
                } else {
                    throw codebundler::ArgumentParserException("Unexpected positional argument for bundle: " + token);
                }
            } else if (args.command == "unbundle" || args.command == "verify") {
                if (args.inputFile.empty()) {
                    args.inputFile = token; // First positional arg is input file
                } else if (args.command == "unbundle" && args.outputDir == ".") { // Only set outputDir if not already set for unbundle
                    args.outputDir = token; // Second positional arg is output dir
                } else if (args.command == "verify" && !args.inputFile.empty()) {
                     throw codebundler::ArgumentParserException("Unexpected positional argument for verify: " + token);
                }
                 else {
                    // Covers case where outputDir was already set for unbundle, or too many args for verify
                    throw codebundler::ArgumentParserException("Unexpected positional argument for " + args.command + ": " + token);
                }
            } else {
                // Handles case where the command itself is unknown or arg appears before valid command
                 throw codebundler::ArgumentParserException("Unknown command or misplaced argument: " + token);
            }
        }
        currentArg++;
    }

    // --- Post-parsing validation ---
    if (args.command != "bundle" && args.command != "unbundle" && args.command != "verify") {
        // This check might be redundant if the positional arg logic catches unknown commands, but good for clarity
        throw codebundler::ArgumentParserException("Invalid command: " + args.command + ". Must be 'bundle', 'unbundle', or 'verify'.");
    }
    // Validation for --no-verify and --description already handled inline during parsing.
    // Separator validation also handled inline for unbundle/verify.
    // It remains relevant for bundle, using the default if not provided.

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
            codebundler::Bundler bundler(args.separator); // Use provided or default separator
            if (args.outputFile.empty()) {
                // Bundle to standard output
                std::cerr << "Bundling to standard output..." << std::endl;
                bundler.bundleToStream(std::cout, args.description);
                 std::cerr << "Bundling to standard output completed." << std::endl;
            } else {
                // Bundle to a file
                std::cerr << "Bundling to file: " << args.outputFile << "..." << std::endl;
                bundler.bundleToFile(args.outputFile, args.description);
                 std::cerr << "Bundling to file completed." << std::endl;
            }
            // std::cerr << "Bundling completed successfully." << std::endl; // Use cerr for status messages not part of bundle output

        } else if (args.command == "unbundle") {
            codebundler::Options options;
            // Separator is auto-detected by Unbundler, no longer set here
            options.trialRun = !args.noVerify; // Note the inversion

            codebundler::Unbundler unbundler(options);
            std::filesystem::path outputDir = args.outputDir; // Convert string to path

            if (args.inputFile.empty()) {
                // Unbundle from standard input
                std::cerr << "Unbundling from standard input to directory: " << outputDir.string() << "..." << std::endl;
                unbundler.unbundleFromStream(std::cin, outputDir);
                 std::cerr << "Unbundling from standard input completed." << std::endl;
            } else {
                // Unbundle from a file
                 std::cerr << "Unbundling from file: " << args.inputFile << " to directory: " << outputDir.string() << "..." << std::endl;
                unbundler.unbundleFromFile(args.inputFile, outputDir);
                 std::cerr << "Unbundling from file completed." << std::endl;
            }
           // std::cerr << "Unbundling completed successfully." << std::endl;

        }

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

    return 0; // Success (only reached for bundle command now)
}
