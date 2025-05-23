# CodeBundler
Bundles code tracked by Git into a single file for easier sharing, especially with AI assistants. Includes checksums for integrity verification.

If a bundle was created by an LLM, it most likely will not contain a valid checksum.  In this case, the `--no-verify` option can be used to unbundle without checksum verification.

## Features and Limitation

*   Handles ASCII and UTF-8.
*   No support for binary files.
*   Always ensures files end with a newline.
*   Concatenates files listed by `git ls-files` into a single bundle.
*   Uses a customizable boundary marker.
*   Includes a checksum for each file within the bundle.
*   Uses PicoSHA2 for hashing.
*   Checksums are not guaranteed to match the original file
    content, as the bundle format may modify the file content (e.g.,
    adding a newline at the end).
*   Can unbundle files, verifying checksums during extraction.
*   Can verify the integrity of a bundle file without extracting.
*   Can unbundle without checksum verification if it's missing, modified,
    or wrong (file contents changed without updating checksum).
*   Designed for use in Git repositories.
*   Supports custom separators.
*   All external libraries will be checked for local presence, and
    downloaded if not found.

## Usage

```bash
# Bundle all tracked files into bundle.txt
codebundler bundle bundle.txt

# Bundle to standard output
codebundler bundle

# Bundle with a custom separator
codebundler bundle --separator="CUSTOM_SEPARATOR" bundle.txt

# Unbundle from bundle.txt into the 'output' directory
codebundler unbundle bundle.txt output

# Unbundle into the current working directory
codebundler unbundle bundle.txt

# Unbundle without verifying checksums
codebundler unbundle --no-verify bundle.txt
codebundler unbundle --no-verify bundle.txt output

# Unbundle from standard input into the current directory
cat bundle.txt | codebundler unbundle

# Verify the integrity of bundle.txt without extracting
codebundler unbundle --trial-run bundle.txt
```

## Bundle Format

```
<SEPARATOR>
Description: <...>
<SEPARATOR>
Filename: <path/to/file1>
Checksum: SHA256:<sha256_hash_of_file1_content>
<content of file1>
<SEPARATOR>
Filename: <path/to/file2>
Checksum: SHA256:<sha256_hash_of_file2_content>
<content of file2>
<SEPARATOR>
...
```

## Building

Requires CMake (3.14+) and a C++17 compatible compiler. Google Test is fetched automatically if not found system-wide.

```bash
git clone <repository_url>
cd codebundler
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /path/to/install # Optional install
```

## Implementation

*   Uses PicoSHA2 for SHA-256 hashing.  Please see https://github.com/okdshin/PicoSHA2
*   Written with AI assistance.
