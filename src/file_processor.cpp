// src/file_processor.cpp

#include "file_processor.hpp"
#include <algorithm>  // Add this line to include <algorithm>

void FileProcessor::addMatcher(std::unique_ptr<FileMatcherInterface> matcher) {
    matchers_.push_back(std::move(matcher));
}

bool FileProcessor::shouldProcessFile(const std::filesystem::path& path) const {
    return std::any_of(matchers_.begin(), matchers_.end(),
                       [&path](const auto& matcher) { return matcher->matches(path); });
}

std::vector<std::filesystem::path> FileProcessor::getMatchingFiles(const std::filesystem::path& directory) const {
    std::vector<std::filesystem::path> matching_files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        if (entry.is_regular_file() && shouldProcessFile(entry.path())) {
            matching_files.push_back(entry.path());
        }
    }
    return matching_files;
}
