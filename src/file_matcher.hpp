// src/file_matcher.hpp

#pragma once

#include <string>
#include <filesystem>

class FileMatcherInterface {
public:
    virtual ~FileMatcherInterface() = default;
    [[nodiscard]] virtual bool matches(const std::filesystem::path& path) const = 0;
};

class ExtensionMatcher : public FileMatcherInterface {
public:
    explicit ExtensionMatcher(std::string extension);
    [[nodiscard]] bool matches(const std::filesystem::path& path) const override;

private:
    std::string extension_;
};

class ExactNameMatcher : public FileMatcherInterface {
public:
    explicit ExactNameMatcher(std::string name);
    [[nodiscard]] bool matches(const std::filesystem::path& path) const override;

private:
    std::string name_;
};
