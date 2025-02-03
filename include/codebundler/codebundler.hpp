#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <cstdio>
#include <memory>
#include <iostream>

namespace codebundler {

// Custom exception with context
class BundleError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

// RAII wrapper for process handles
class ProcessHandle {
    FILE* pipe_;
public:
    explicit ProcessHandle(const char* cmd) : pipe_(popen(cmd, "r")) {
        if (!pipe_) throw BundleError("Process creation failed: " + std::string(cmd));
    }
    ~ProcessHandle() { if (pipe_) pclose(pipe_); }
    
    FILE* get() const { return pipe_; }
    
    ProcessHandle(const ProcessHandle&) = delete;
    ProcessHandle& operator=(const ProcessHandle&) = delete;
};

// Validated separator
class Separator {
    std::string value_;

public:
    explicit Separator(std::string s) {
        if (s.empty()) {
            throw BundleError("Separator cannot be empty");
        }
        if (s.find('\n') != std::string::npos) {
            throw BundleError("Separator cannot contain newlines");
        }
        value_ = std::move(s);
    }

    const std::string& get() const { return value_; }
    
    bool matches(const std::string& line) const {
        return line == value_;
    }
};

class CodeBundler {
public:
    static constexpr const char* DEFAULT_SEPARATOR = "---------- BOUNDARY ----------";
    static constexpr const char* DEFAULT_DESCRIPTION = 
        "Description: This is a concatenation of several files. "
        "All files are separated by the boundary string you see above, "
        "followed by the name of the file.\n";

    // Stream-based bundle operation
    static void bundle(std::ostream& output,
                      const std::vector<std::string>& files,
                      const std::string& separator_str = DEFAULT_SEPARATOR,
                      const std::string& description = DEFAULT_DESCRIPTION) {
        // Validate separator
        Separator separator(separator_str);

        // Write initial separator and description
        output << separator.get() << '\n';
        output << description;

        // Process each file
        for (const auto& file : files) {
            output << separator.get() << '\n';
            output << "Filename: " << file << '\n';
            
            // Open and stream file content
            std::ifstream input(file);
            if (!input) {
                throw BundleError("Failed to open file: " + file);
            }
            input.seekg(0, std::ios::end);
            if (input.tellg() == 0) {
                throw BundleError("Empty file: " + file);
            }
            input.seekg(0, std::ios::beg);
            output << input.rdbuf();

            if (input.fail()) {
                throw BundleError("Input failed: " + file);
            }
            if (output.fail()) {
                throw BundleError("Output failed: " + file);
            }
        }

        // Write final separator
        output << separator.get() << '\n';
        
        if (!output) {
            throw BundleError("Failed to complete bundle operation");
        }
    }

    // File-based bundle operation (convenience wrapper)
    static void bundle(const std::filesystem::path& output_path,
                      const std::vector<std::string>& files,
                      const std::string& separator_str = DEFAULT_SEPARATOR,
                      const std::string& description = DEFAULT_DESCRIPTION) {
        std::ofstream output(output_path);
        if (!output) {
            throw BundleError("Failed to open output file: " + output_path.string());
        }
        bundle(output, files, separator_str, description);
    }

    // Stream-based unbundle operation
    static void unbundle(std::istream& input,
                        const std::filesystem::path& output_dir = "") {
        std::string line;
        
        // Read and validate first separator
        if (!std::getline(input, line)) {
            throw BundleError("Failed to read initial separator");
        }
        Separator separator(line);

        // Skip description until next separator
        while (std::getline(input, line) && !separator.matches(line)) {}

        // Process each file
        std::string current_file;
        std::stringstream content;
        
        while (std::getline(input, line)) {
            if (separator.matches(line)) {
                // Write previous file if exists
                if (!current_file.empty()) {
                    try {
                        std::filesystem::path filepath = output_dir / current_file;
                        std::filesystem::create_directories(filepath.parent_path());
                        
                        std::ofstream output(filepath);
                        if (!output) {
                            throw BundleError("Failed to create file: " + filepath.string());
                        }
                        output << content.str();
                        if (!output) {
                            throw BundleError("Failed to write to file: " + filepath.string());
                        }
                    } catch (const std::filesystem::filesystem_error& e) {
                        throw BundleError("Filesystem error: " + std::string(e.what()));
                    }
                }
                
                // Start new file
                if (!std::getline(input, line)) {
                    break;
                }
                
                if (line.substr(0, 10) != "Filename: ") {
                    throw BundleError("Expected filename, got: " + line);
                }
                
                current_file = line.substr(10);
                content.str("");
                content.clear();
                continue;
            }
            
            if (!current_file.empty()) {
                content << line << '\n';
            }
        }
        
        // Write final file if exists
        if (!current_file.empty()) {
            std::filesystem::path filepath = output_dir / current_file;
            std::filesystem::create_directories(filepath.parent_path());
            std::ofstream output(filepath);
            if (!output) {
                throw BundleError("Failed to create final file: " + filepath.string());
            }
            output << content.str();
        }
    }

    static bool verify(std::istream& bundle) {
        std::string separator;
        bool inDescription = false;
        std::string currentFile;
        std::stringstream content;
        
        std::string line;
        while (std::getline(bundle, line)) {
            if (separator.empty()) {
                separator = line;
                inDescription = true;
                continue;
            }
            
            if (line == separator) {
                if (inDescription) {
                    inDescription = false;
                } else if (!currentFile.empty()) {
                    // Verify current file before moving to next
                    std::ifstream orig(currentFile, std::ios::binary);
                    if (!orig) {
                        return false;
                    }
                    
                    std::string bundle_content = content.str();
                    std::string orig_content((std::istreambuf_iterator<char>(orig)),
                                          std::istreambuf_iterator<char>());
                    
                    if (bundle_content != orig_content) {
                        return false;
                    }
                    
                    currentFile.clear();
                    content.str("");
                    content.clear();
                }
                continue;
            }
            
            // Handle non-separator lines
            if (inDescription) {
                continue;  // Skip description content
            } else if (currentFile.empty()) {
                if (line.substr(0, 10) != "Filename: ") {
                    throw BundleError("Invalid filename line: " + line);
                }
                currentFile = line.substr(10);
            } else {
                content << line << '\n';
            }
        }
        
        // Handle final file if exists
        if (!currentFile.empty()) {
            std::ifstream orig(currentFile, std::ios::binary);
            if (!orig) {
                return false;
            }
            std::string bundle_content = content.str();
            std::string orig_content((std::istreambuf_iterator<char>(orig)),
                                   std::istreambuf_iterator<char>());
            return bundle_content == orig_content;
        }
        
        return true;
    }

    // File-based unbundle operation (convenience wrapper)
    static void unbundle(const std::filesystem::path& input_path,
                        const std::filesystem::path& output_dir = "") {
        std::ifstream input(input_path);
        if (!input) {
            throw BundleError("Failed to open input file: " + input_path.string());
        }
        unbundle(input, output_dir);
    }

    // File-based verify operation (convenience wrapper)
    static bool verify(const std::filesystem::path& bundle_path) {
        std::ifstream bundle(bundle_path);
        if (!bundle) {
            throw BundleError("Failed to open bundle file: " + bundle_path.string());
        }
        return verify(bundle);
    }
};

} // namespace codebundler
