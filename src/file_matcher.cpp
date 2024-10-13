// src/file_matcher.cpp

#include "file_matcher.hpp"

ExtensionMatcher::ExtensionMatcher(std::string extension) : extension_(std::move(extension)) {}

bool ExtensionMatcher::matches(const std::filesystem::path& path) const {
    return path.extension() == extension_;
}

ExactNameMatcher::ExactNameMatcher(std::string name) : name_(std::move(name)) {}

bool ExactNameMatcher::matches(const std::filesystem::path& path) const {
    return path.filename() == name_;
}
