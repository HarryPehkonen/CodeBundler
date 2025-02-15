// include/codebundler/codebundler.hpp
#pragma once

#include "handles.hpp"
#include "errors.hpp"
#include "options.hpp"
#include <vector>
#include <string>
#include <filesystem>
#include <memory>
#include <sstream>
#include <iostream>
#include <optional>
#include <algorithm>
#include <fstream>
#include <functional>
#include <iterator>

#include <cassert>

namespace codebundler {

void print_directory_tree(const std::filesystem::path&, std::string);

void print_directory_tree(const std::filesystem::path& path, std::string indent = "") {
    try {
        if (!std::filesystem::exists(path)) {
            std::cout << "Path does not exist: " << path << std::endl;
            return;
        }

        std::cout << indent << path.filename().string() << std::endl;
        
        if (std::filesystem::is_directory(path)) {
            indent += "    ";
            std::vector<std::filesystem::path> sorted_paths;
            
            // Collect and sort directory entries
            for (const auto& entry : std::filesystem::directory_iterator(path)) {
                sorted_paths.push_back(entry.path());
            }
            std::sort(sorted_paths.begin(), sorted_paths.end());
            
            // Recursively print each entry
            for (const auto& entry : sorted_paths) {
                print_directory_tree(entry, indent);
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error accessing path: " << e.what() << std::endl;
    }
}

class CodeBundler {
private:
    class BundleSession {
        std::vector<std::unique_ptr<FileHandle>> open_files_;

    public:
        void addFile(const std::filesystem::path& path) {
            auto file = std::make_unique<FileHandle>(path);
            open_files_.push_back(std::move(file));
        }

        const std::vector<std::unique_ptr<FileHandle>>& files() const {
            return open_files_;
        }
    };

    static void writeBundle(std::ostream& output,
                          const BundleSession& session,
                          const Options& options) {
        output << options.separator() << '\n'
               << options.description();

        for (const auto& file : session.files()) {
            output << options.separator() << '\n'
                   << "Filename: " << file->path().string() << '\n';

            // Stream the entire file
            output << file->get().rdbuf();

            if (!output) {
                throw BundleError("Failed to write file: " + file->path().string());
            }
        }

        output << options.separator() << '\n';

        if (!output) {
            throw BundleError("Failed to complete bundle operation");
        }
    }

public:
    static void bundle(std::ostream& output,
                      const std::vector<std::string>& filenames,
                      const Options& options = {}) {
        if (!output) {
            throw BundleError("Invalid output stream");
        }

        BundleSession session;
        for (const auto& filename : filenames) {
            session.addFile(filename);
        }

        writeBundle(output, session, options);
    }

    static void unbundle(std::istream& input,
                 const std::filesystem::path& output_dir) {
        if (!input) {
            throw BundleError("Invalid input stream");
        }

        // Create base output directory if it doesn't exist
        std::filesystem::create_directories(output_dir);
        if (!std::filesystem::is_directory(output_dir)) {
            throw BundleError("Unable to create output directory");
        }

        std::string line;
        std::unique_ptr<Options> options; // Validates the separator
        std::optional<std::string> filename = std::nullopt;
        std::ofstream output; // Output file stream

        // Skip description until next separator
        bool gonePastDescription = false;

        // schema:
        //   separator
        //   description*
        //   separator
        //   filename
        //   content*   > *
        //   separator
        //
        // the description continues until the second separator.
        // the content continues until the next separator, and may be
        // empty.
        // the filename-content-separator sequence may repeat at least
        // zero times.
        while (std::getline(input, line)) {

            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            // the first line must be the separator.  have we
            // initialized options with the separator yet?
            if (!options) {
                std::string separator = line;
                options = std::make_unique<Options>(separator);
                continue;
            }

            // a separator ends each section
            if (line == options->separator()) {

                // if we already went past the description, we must be
                // at the end of a file.  not *the* file, but *a* file.
                if (gonePastDescription) {

                    // reset everything because we are starting a new
                    // file
                    if (output) {
                        output.close();
                    }
                    filename = std::nullopt;
                    continue;
                }

                // we must have gone past the description now
                gonePastDescription = true;
                continue;
            }

            // skip until we get to that separator
            if (!gonePastDescription) {
                continue;
            }

            // are we getting the filename?
            if (!filename) {
                if (line.substr(0, 10) != "Filename: ") {
                    throw BundleError("Expected filename, got: '" + line + "'");
                }

                filename = line.substr(10);
                std::filesystem::path output_file = output_dir / *filename;
                if (!std::filesystem::create_directories(output_file.parent_path())) {
                    // don't worry about it.  if the directory already
                    // exists, create_directories will return false.
                }
                output.open(output_file);
                if (!output) {
                    throw BundleError("Failed to open output file: " + output_file.string());
                }
                continue;
            }

            // must be contents of the current file
            output << line << '\n';
        }
    }

    static bool verify(std::istream& bundle) {
        std::stringstream bundle_copy;

        // Check for errors before clearing the stream
        if (bundle.fail() || bundle.bad()) {
            throw BundleError("Bundle input stream with ERROR given to verify.");
        }
        bundle.seekg(0); // Reset original bundle position
        bundle_copy << bundle.rdbuf(); // Make a copy of the bundle

        if (bundle_copy.fail() || bundle_copy.bad()) {
            throw BundleError("Bundle copy stream with ERROR created in verify.");
        }
        bundle_copy.seekg(0); // Reset copy position

        // Create temporary directory for verification
        auto temp_unbundling_dir = std::filesystem::temp_directory_path() / 
                       "codebundler_verify_XXXXXX";
        assert(!std::filesystem::exists(temp_unbundling_dir));
        bool verified = true;

        try {
            std::filesystem::create_directories(temp_unbundling_dir);
            assert(std::filesystem::exists(temp_unbundling_dir));
            assert(std::filesystem::is_directory(temp_unbundling_dir));

            // Use RAII to clean up temp directory
            struct TempDirCleaner {
                std::filesystem::path path;
                ~TempDirCleaner() { std::filesystem::remove_all(path); }
            } cleaner{temp_unbundling_dir};

            // Unbundle to temp directory
            unbundle(bundle_copy, temp_unbundling_dir);

            int line_count = 0;
            bundle_copy.clear();
            bundle_copy.seekg(0);
            std::string line;
            while (std::getline(bundle_copy, line)) {
                ++line_count;
                if (line.substr(0, 10) == "Filename: ") {
                    std::string filename = line.substr(10);
                    auto temp_file = temp_unbundling_dir / filename;

                    if (!std::filesystem::exists(temp_file)) {

                        // the file is actually missing in the unbundled
                        // directory
                        std::cerr << "File not found: " << temp_file << std::endl;
                        verified = false;
                        break;
                    } 
                    if (!compareFiles(filename, temp_file)) {

                        // the files are just different
                        verified = false;
                        break;
                    }
                }
            }

        }
        catch (const std::exception&) {
            std::cerr << "Exception caught during verification" << std::endl;
            verified = false;
            assert(!std::filesystem::exists(temp_unbundling_dir));
            return verified;
        }

        // temp directory should have been cleaned up by now
        assert(!std::filesystem::exists(temp_unbundling_dir));

        return verified;
    }

private:
    static bool compareFiles(const std::filesystem::path& original,
                             const std::filesystem::path& copy) {

        // Check file sizes first
        if (std::filesystem::file_size(original) != std::filesystem::file_size(copy)) {
            return false;
        }

        std::ifstream f1(original, std::ios::binary);
        std::ifstream f2(copy, std::ios::binary);

        if (!f1 || !f2) return false;

        bool result = std::equal(
            std::istreambuf_iterator<char>(f1),
            std::istreambuf_iterator<char>(),
            std::istreambuf_iterator<char>(f2)
        );

        return result;
    }

};

} // namespace codebundler
