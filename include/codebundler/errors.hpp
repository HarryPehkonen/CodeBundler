// include/codebundler/errors.hpp
#pragma once

#include <stdexcept>
#include <string>

namespace codebundler {

class BundleError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

} // namespace codebundler
