#ifndef CODEBUNDLER_CONSTANTS_HPP
#define CODEBUNDLER_CONSTANTS_HPP

#include <string>

namespace codebundler {

// Constants - Using 'inline const' (C++17) to define them in the header without ODR issues
inline const std::string FILENAME_PREFIX = "Filename: ";
inline const std::string CHECKSUM_PREFIX = "Checksum: ";
// Written once per file the bundler refused to inline (binary files). The
// unbundler reads these as comments, so the line is bookkeeping, not data.
inline const std::string SKIPPED_BINARY_PREFIX = "# Skipped binary file: ";

} // namespace codebundler

#endif // CODEBUNDLER_CONSTANTS_HPP
