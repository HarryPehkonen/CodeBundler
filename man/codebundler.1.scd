codebundler(1)

# NAME

codebundler - bundle tracked project files into one shareable artifact

# SYNOPSIS

*codebundler* *bundle* [_output_file_] [_options_]

*codebundler* *unbundle* [_input_file_] [_output_dir_] [_options_]

# DESCRIPTION

*codebundler* inlines the files tracked by the current Git repository into a
single text artifact, with a SHA256 checksum per file, so a whole source tree
can be shared as one file - for example, pasted to an AI assistant. *unbundle*
verifies those checksums and writes the file tree back.

*bundle* must run inside a Git repository: it asks `git ls-files` which files to
include, so tracked files are bundled and untracked or ignored files are not.
Bundling writes to stdout when no output file is given. Unbundling reads from
stdin when no input file is given and extracts into the current directory when
no output directory is given; the separator is detected automatically from the
first line of the bundle.

The artifact is plain text: ASCII and UTF-8 pass through unchanged. Binary files
are never bundled. A file is treated as binary when a NUL byte appears in its
first 8000 bytes - the window Git itself uses - and the rest of its content is
checked too, so a NUL anywhere counts: a bundle is line-oriented text, and a
file that carries NULs (or a long text prologue in front of binary data, as
archives and PDFs do) cannot be inlined safely. Images, archives and compiled
objects are therefore left out instead of being copied in as mangled text. Each
skipped file is named at the end of the bundle on a
"# Skipped binary file: <path>" line, which *unbundle* reads as a comment; those
files are therefore not restored by *unbundle* and must be copied by other
means.

The format is line-oriented, and two consequences are worth knowing before
comparing an unbundled tree with the original: a file that does not end in a
newline gains one, and the checksum stored for a file covers the content as
written into the bundle rather than the byte-identical original. `git diff`
reports such files as having no newline at end of file.

# COMMANDS

*bundle* [_output_file_]
	Bundle the tracked files of the current repository, to _output_file_ or to
	stdout.

*unbundle* [_input_file_] [_output_dir_]
	Extract files from a bundle, verifying SHA256 checksums. Reads _input_file_
	or stdin and writes into _output_dir_ or the current directory.

# BUNDLE OPTIONS

*--separator* <sep>
	Use a custom separator string (default "========= BOUNDARY ==========").
	The separator delimits entries, so a file whose content contains a line
	equal to it is refused rather than bundled.

*--description* <desc>
	Add an optional description to the bundle header.

# UNBUNDLE OPTIONS

*--output-dir* <dir>
	Extract into _dir_, creating it if needed. Same as passing the directory as
	the second positional argument.

*--no-verify*
	Disable SHA256 checksum verification. Use this for bundles whose checksums
	do not match their content - most often a bundle written by an LLM, which
	typically cannot compute correct checksums.

*--trial-run*
	Verify and report without writing any file.

# OPTIONS

*-v*, *--verbose*
	Enable verbose output (repeat for more detail). Skipped binary files,
	per-file progress and parse states are printed to standard error.

*-h*, *--help*
	Print usage and exit.

# BUNDLE FORMAT

Each entry is a filename line, a checksum line and the file content, followed by
the separator. Files that were not inlined are named at the end.

	<SEPARATOR>
	Description: <optional>
	<SEPARATOR>
	Filename: <path/to/file1>
	Checksum: <sha256 of the content as written>
	<content of file1>
	<SEPARATOR>
	Filename: <path/to/file2>
	Checksum: <sha256 of the content as written>
	<content of file2>
	<SEPARATOR>
	# Skipped binary file: <path/to/binary>

# EXIT STATUS

*0*
	Success.

*1*
	Any failure: a bad or unknown option, an unknown command, a failing call
	to `git ls-files`, an unreadable file, a file containing the separator, a
	checksum mismatch, or a malformed bundle.

# EXAMPLES

Bundle the current repository to a file and restore it elsewhere:

	codebundler bundle /tmp/project.bundle
	cd /elsewhere && codebundler unbundle /tmp/project.bundle .

Pipe through stdin/stdout to another machine:

	codebundler bundle | ssh host codebundler unbundle

Check a bundle without extracting anything:

	codebundler unbundle --trial-run /tmp/project.bundle

Restore a bundle that an assistant produced (its checksums are not usable):

	codebundler unbundle --no-verify bundle.txt output/

Unbundle into a chosen directory, naming each file as it is written:

	codebundler unbundle -vv bundle.txt --output-dir /tmp/restored

# SEE ALSO

*git-ls-files*(1), *sha256sum*(1)

https://github.com/HarryPehkonen/CodeBundler

# AUTHORS

Harri Pehkonen <harry.pehkonen@gmail.com>
