// src/file_processor.cpp

#include "file_processor.hpp"
#include <algorithm>
#include <iostream>
#include <variant>

FileProcessor::FileProcessor(bool verbose) : verbose_(verbose) {}

void FileProcessor::addMatcher(std::unique_ptr<FileMatcherInterface> matcher) {
    matchers_.push_back(std::move(matcher));
}

bool FileProcessor::shouldProcessFile(const std::filesystem::path& path) const {
    return std::any_of(matchers_.begin(), matchers_.end(),
                       [&path](const auto& matcher) { return matcher->matches(path); });
}

std::vector<std::filesystem::path> FileProcessor::getMatchingFiles(const std::filesystem::path& directory) const {
    std::vector<std::filesystem::path> matching_files;

    std::variant<std::filesystem::directory_iterator,
                 std::filesystem::recursive_directory_iterator> iterator;

    // don't recursively search from the root directory.  do it from all
    // the other specified directories.
    if (directory == "./") {
        iterator = std::filesystem::directory_iterator(directory);
    } else {
        iterator = std::filesystem::recursive_directory_iterator(directory);
    }

    // iterator is a funky variant type, so we wat to use std::visit
    std::visit([&matching_files, this](auto&& it) {
        for (const auto& entry : it) {
            verbose_ && std::cerr << "Processing " << entry.path() << std::endl;
            if (entry.is_regular_file() && shouldProcessFile(entry.path())) {
                verbose_ && std::cerr << "Matched " << entry.path() << std::endl;
                matching_files.push_back(entry.path());
            }
        }
    }, iterator);

    return matching_files;
}
