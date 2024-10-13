// src/constants.hpp

#pragma once

#include <string>
#include <filesystem>

const std::string BOUNDARY_STRING = "------ BOUNDARY_STRING_XYZZY ------";
const std::filesystem::path SRC_DIR = "./src/";
const std::filesystem::path TESTS_DIR = "./tests/";
const std::string EXAMPLE = R"(
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
)";
