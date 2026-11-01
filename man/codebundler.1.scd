codebundler(1)

# NAME

codebundler - bundle source files into one shareable artifact

# SYNOPSIS

*codebundler* *bundle* [output_file] [options]

*codebundler* *unbundle* [input_file] [output_dir] [options]

# DESCRIPTION

*codebundler* bundles the tracked files of a project into a single artifact
(with SHA256 checksums per file) so it can be shared as one file - for
example, pasted to an AI assistant. *unbundle* verifies the checksums and
restores the file tree.

Bundling writes to stdout if no output file is given; unbundling reads from
stdin if no input file is given and extracts to the current directory if no
output directory is given. The separator is detected automatically from the
first line of a bundle.

Binary files are never bundled. A file is treated as binary when a NUL byte
appears in its first 8000 bytes - the window git itself uses - and the rest of
its content is checked too, so a NUL anywhere makes a file binary: a bundle is
line-oriented text, and a file that carries NULs (or a long text prologue in
front of binary data, as archives and PDFs do) cannot be inlined safely. Images,
archives, compiled objects and the like are therefore left out instead of being
copied in as mangled text. Each one is named at the end of the bundle on a
"# Skipped binary file: <path>" line, which *unbundle* reads as a comment;
those files are therefore not restored by *unbundle* and must be copied by
other means.

# COMMANDS

*bundle* [output_file]
	Bundle the tracked files of the current project.

*unbundle* [input_file] [output_dir]
	Extract files from a bundle, verifying SHA256 checksums.

# BUNDLE OPTIONS

*--separator* <sep>
	Use a custom separator string (default
	"========= BOUNDARY ==========").

*--description* <desc>
	Add an optional description to the bundle header.

# UNBUNDLE OPTIONS

*--no-verify*
	Disable SHA256 checksum verification.

*--trial-run*
	Perform a trial run without writing files.

# OPTIONS

*-v*, *--verbose*
	Enable verbose output (repeat for more detail).

*-h*, *--help*
	Print usage and exit.

# EXAMPLES

Bundle the current repository to a file and restore it elsewhere:

	codebundler bundle /tmp/project.bundle
	cd /elsewhere && codebundler unbundle /tmp/project.bundle .

Pipe through stdin/stdout:

	codebundler bundle | ssh host codebundler unbundle

# SEE ALSO

https://github.com/HarryPehkonen/CodeBundler

# AUTHORS

Harri Pehkonen <harry.pehkonen@gmail.com>
