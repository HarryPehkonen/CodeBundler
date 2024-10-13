// src/constants.hpp

#pragma once

#include <string>
#include <filesystem>

const std::string BOUNDARY_STRING = "------ BOUNDARY_STRING_XYZZY ------";
const std::filesystem::path SRC_DIR = "./src/";
const std::filesystem::path TESTS_DIR = "./tests/";
const std::string EXAMPLE = R"(
We're using a custom file format for bundling multiple source files. Here's an example of the format:
------ BOUNDARY_STRING_XYZZY ------
Path: src/main.cpp

#include <iostream>

int main() {
    std::cout << "Hello, World!" << std::endl;
    return 0;
}
------ BOUNDARY_STRING_XYZZY ------
Path: include/header.hpp

#pragma once

void someFunction();
------ BOUNDARY_STRING_XYZZY ------

The format follows these rules:

Each file starts with a boundary string ('------ BOUNDARY_STRING_XYZZY ------').
The next line contains 'Path: ' followed by the file path.
There's an empty line after the Path line.
The file contents follow, exactly as they appear in the original file.
The format repeats for each file, ending with a final boundary string.
)";
