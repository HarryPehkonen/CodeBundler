#ifndef CODEBUNDLER_CONSTANTS_HPP
#define CODEBUNDLER_CONSTANTS_HPP

#include <string>

namespace codebundler {

// Constants - Using 'inline const' (C++17) to define them in the header without ODR issues
inline const std::string FILENAME_PREFIX = "Filename: ";
inline const std::string CHECKSUM_PREFIX = "Checksum: ";

} // namespace codebundler

#endif // CODEBUNDLER_CONSTANTS_HPP
