#ifndef CODEBUNDLER_CONSTANTS_HPP
#define CODEBUNDLER_CONSTANTS_HPP

#include <cstddef>
#include <string>

namespace codebundler {

// Constants - Using 'inline const' (C++17) to define them in the header without ODR issues
inline const std::string FILENAME_PREFIX = "Filename: ";
inline const std::string CHECKSUM_PREFIX = "Checksum: ";
// Written once per file the bundler refused to inline (binary files). The
// unbundler reads these as comments, so the line is bookkeeping, not data.
inline const std::string SKIPPED_BINARY_PREFIX = "# Skipped binary file: ";
// How far the cheap binary sniff looks, in bytes. 8000 is git's own window: past
// it a file may still be binary (the bundler re-checks the whole content).
inline constexpr std::size_t BINARY_SNIFF_BYTES = 8000;

} // namespace codebundler

#endif // CODEBUNDLER_CONSTANTS_HPP
