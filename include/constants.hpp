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
// Written once per selection rule that left files out, with the number of files
// it removed: "# Excluded: 157 files (tests/)". Same contract as the binary
// note - a comment line, so unbundle ignores it.
inline const std::string EXCLUDED_NOTE_PREFIX = "# Excluded: ";
// The phrase used in that note when include mode itself (not a rule) left files
// out, i.e. nothing named them.
inline const std::string EXCLUDED_BY_DEFAULT = "no --include rule matched";
// The per-repository selection file, read from the Git top level.
inline const std::string IGNORE_FILE_NAME = ".codebundlerignore";
// How far the cheap binary sniff looks, in bytes. 8000 is git's own window: past
// it a file may still be binary (the bundler re-checks the whole content).
inline constexpr std::size_t BINARY_SNIFF_BYTES = 8000;

} // namespace codebundler

#endif // CODEBUNDLER_CONSTANTS_HPP
