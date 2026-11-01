# CPack configuration for CodeBundler — included at the END of CMakeLists.txt,
# after every install() rule: CPack consumes the install rules, so anything
# installed after this include would silently miss the packages.
#
# One CMake tree, ONE package (codebundler is a single CLI: /usr/bin/codebundler
# + man page — no component split).
#
# Usage (also documented in PACKAGING.md):
#   cmake -S . -B build -DCODEBUNDLER_REQUIRE_SYSTEM_FSMGINE=ON
#   cmake --build build
#   cpack -G DEB --config build/CPackConfig.cmake -B build/pkgs
#   cpack -G RPM --config build/CPackConfig.cmake -B build/pkgs

set(CPACK_PACKAGE_VENDOR "Harri Pehkonen")
set(CPACK_PACKAGE_CONTACT "Harri Pehkonen <harry.pehkonen@gmail.com>")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "bundle project files into one shareable artifact")
set(CPACK_PACKAGE_DESCRIPTION
    "CodeBundler bundles the tracked files of a project into a single artifact
(with SHA256 checksums per file) so it can be shared as one file — for
example, pasted to an AI assistant. Unbundle verifies the checksums and
restores the file tree. Statically linked against FSMgine at build time.")

# ---------------------------------------------------------------- Debian
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Harri Pehkonen <harry.pehkonen@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION utils)          # the published 1.0.0 deb said devel — wrong for a CLI-only package
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)           # Depends generated from ELF linkage, not hand-maintained

# ---------------------------------------------------------------- RPM
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE "Unlicense")
set(CPACK_RPM_PACKAGE_AUTOREQPROV ON)
# Without this the RPM Description is CPack's own boilerplate ("This is an
# installer created using CPack ..."), which is what `dnf info codebundler`
# shows a user. The DEB generator reuses CPACK_PACKAGE_DESCRIPTION; the RPM one
# does not.
set(CPACK_RPM_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION}")
# Fedora's `filesystem` package owns /usr/share/man and /usr/share/man/man1. A
# package that ships those directories as its own files cannot be installed at
# all: rpm aborts the transaction with "file /usr/share/man/man1 conflicts with
# file from package filesystem". CPack's RPM generator lists them unless told not
# to (the DEB generator is harmless here - dpkg treats shared directories as
# shared), so exclude them explicitly. Asserted in scripts/package.sh.
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION "/usr/share/man" "/usr/share/man/man1")

include(CPack)
