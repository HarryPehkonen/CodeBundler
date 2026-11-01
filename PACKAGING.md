# Packaging CodeBundler

One package, `codebundler`, for both package managers. It ships exactly two
files: `/usr/bin/codebundler` and the man page generated from
`man/codebundler.1.scd` at build time with `scdoc` (so there is no committed
`.1` copy to drift). No headers, no libraries, and no FSMgine artifacts:
FSMgine is statically linked into the binary by design.

The version is single-sourced from `project(CodeBundler VERSION x.y.z)` in
`CMakeLists.txt`; CPack picks it up as `CPACK_PACKAGE_VERSION`. Nothing else in
the tree carries a version, so a release is: bump that line, rebuild, republish.

## Building the packages

```bash
cmake -S . -B build-pkg -DCMAKE_BUILD_TYPE=Release \
      -DCODEBUNDLER_ENABLE_TESTING=OFF -DCODEBUNDLER_REQUIRE_SYSTEM_FSMGINE=ON
cmake --build build-pkg -j"$(nproc)"
cpack -G "DEB;RPM" --config build-pkg/CPackConfig.cmake -B build-pkg/pkgs
```

Artifacts (names come from `DEB-DEFAULT` / `RPM-DEFAULT`):

* `codebundler_1.0.2_amd64.deb` — Debian/Ubuntu on x86_64
* `codebundler-1.0.2-1.x86_64.rpm` — Fedora/Nobara
* `codebundler_1.0.2_arm64.deb` — built on the Pi, see below

`CODEBUNDLER_REQUIRE_SYSTEM_FSMGINE=ON` is not optional for a package build: it
makes configuration fail loudly when the system FSMgine is missing instead of
network-fetching a moving ref (see `external/CMakeLists.txt`), which is what
keeps an artifact reproducible. It needs FSMgine installed (`/usr/local` on host
`hermes`), plus `scdoc` for the man page and `dpkg-shlibdeps` for the `.deb`
`Depends:` field.

## Building the arm64 package on the Pi

Build it on the Pi itself; `ssh hermes2` is the alias, and the Pi already has
`g++`, `cmake`, `scdoc`, `git` and the apt repo configured.

```bash
rsync -a --exclude=.git --exclude='build*' ~/hermes-workspace/CodeBundler/ hermes2:~/cb-src-<version>/
ssh hermes2 'cd ~/cb-src-<version> && \
  cmake -S . -B build-pkg -DCMAKE_BUILD_TYPE=Release -DCODEBUNDLER_ENABLE_TESTING=OFF \
        -DCODEBUNDLER_REQUIRE_SYSTEM_FSMGINE=ON -DCMAKE_PREFIX_PATH=$HOME/fsmgine-2.2.0/prefix && \
  cmake --build build-pkg -j4 && cpack -G DEB --config build-pkg/CPackConfig.cmake -B build-pkg/pkgs'
scp hermes2:~/cb-src-<version>/build-pkg/pkgs/codebundler_<version>_arm64.deb .
cd ~/hermes-workspace/pkg-repo && ./publish-apt.sh add ./codebundler_<version>_arm64.deb
```

**Do not point the Pi build at the system FSMgine.** The `libfsmgine-dev` 2.2.0
package in our own apt pool exports a broken CMake config on Debian multiarch:
its config sits in `/usr/lib/<triplet>/cmake/FSMgine/` but it computes its
prefix four directories up (`/usr/lib`) and, on arm64, appends a plain `lib/`,
so `find_package(FSMgine)` dies with
`"/usr/lib/lib/libFSMgine.so.2.2.0" does not exist` (the amd64 package has the
same prefix bug, differently visible — nothing had ever built against the
*packaged* FSMgine, only against `/usr/local`). Until that packaging is fixed,
build FSMgine from source into a private prefix on the Pi and pass it in:

```bash
ssh hermes2 'git clone --branch v2.2.0 --depth 1 https://github.com/HarryPehkonen/FSMgine.git ~/fsmgine-2.2.0 && \
  cmake -S ~/fsmgine-2.2.0 -B ~/fsmgine-2.2.0/build -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=$HOME/fsmgine-2.2.0/prefix \
        -DFSMGINE_BUILD_MULTITHREADED=OFF -DFSMGINE_BUILD_SINGLETHREADED=ON && \
  cmake --build ~/fsmgine-2.2.0/build -j4 --target install'
```

Bundling a static FSMgine from a private prefix still satisfies
`REQUIRE_SYSTEM_FSMGINE=ON` — the configure log says
`-- Using system-installed FSMgine from: /home/harri/fsmgine-2.2.0/prefix/lib/cmake/FSMGine`.
Check that line; it is the difference between a deterministic build and a silent
fetch.

## The packaging gate

```bash
bash scripts/package.sh     # exits non-zero on any failure
```

A fresh temp-dir configure + build, then both packages, then these assertions:

1. `.deb` contents are the CLI and the man page only — no FSMgine/PicoSHA2
   artifacts and no `/usr/include/` leak.
2. `.deb` control fields: `Section: utils`, his maintainer address, and a
   `Depends:` line produced by `shlibdeps` (not hand-maintained).
3. The extracted `.deb` binary runs.
4. `.rpm` file list: CLI + man page, **no `/usr/share/man` directory entries**,
   `License: Unlicense`, no CPack-boilerplate `Description`, and the extracted
   binary runs.
5. Every option the CLI accepts appears in the man page as an option definition
   (a line beginning with a bold marker). `--output-dir` was parsed by
   `main.cpp` and documented nowhere — not in the man page, not even in
   `--help` — until this assertion existed.

Assertion 4 exists because of a real failure: CPack's RPM generator lists
`/usr/share/man` and `/usr/share/man/man1` as package files, Fedora's
`filesystem` package already owns both, and rpm therefore refuses the whole
transaction (`file /usr/share/man/man1 conflicts with file from package
filesystem`). The package builds fine and looks fine in `rpm -qlp` — it just
cannot be installed on a Fedora machine. `cmake/CPack.cmake` excludes those two
paths; the gate keeps them excluded.

Assertion 5 is line-based and deliberately strict about *definitions*: a
mention in prose or in an example does not count. When this check was written
looser, sabotaging the man page (deleting the `--output-dir` definition while
leaving its example) still produced PASS — twice, in two different ways — so
sabotage the check before trusting it.

## Publishing

The repository lives on the VPS and is served statically; only signed packages
reach it. Everything runs on host `hermes` (the machine holding the signing
key), driven by `~/hermes-workspace/pkg-repo/`:

```bash
cd ~/hermes-workspace/pkg-repo
./publish-apt.sh add <path>/codebundler_1.0.2_amd64.deb    # pool/main + Packages + signed InRelease + rsync
./publish-apt.sh add <path>/codebundler_1.0.2_arm64.deb    # same pool, other architecture
./publish-rpm.sh add <path>/codebundler-1.0.2-1.x86_64.rpm # Packages/ + rpmsign + createrepo_c + rsync
./publish-rpm.sh check                                     # verify signatures only, no writes, no push
python3 gen-packages.py                                    # regenerate pehkonen.dev/packages/index.html
```

Both scripts regenerate metadata from scratch, sign it, and `rsync --delete` the
finished tree to `notes-server:/srv/repo/{apt,rpm}`; the VPS holds no key and no
packaging tooling. `publish-rpm.sh` refuses to publish anything unsigned.

Client setup is on https://pehkonen.dev/ (apt and dnf snippets, also in
`README.md`). Verify a release the way the machines consume it:

```bash
sudo apt-get update -o Dir::Etc::sourcelist=/etc/apt/sources.list.d/pehkonen.dev.list \
                    -o Dir::Etc::sourceparts=/dev/null -o APT::Get::List-Cleanup=0
apt list --upgradable | grep codebundler
sudo apt-get install -y --only-upgrade codebundler   # Debian 13 (trixie) client
ssh hermes2 'sudo apt-get update && sudo apt upgrade -y codebundler'   # Pi (arm64)
ssh laptop 'sudo dnf --refresh upgrade codebundler'  # Nobara / Fedora 44 client
```

Both fleet clients are wired to these repos already, which makes them the honest
test: a package that installs nowhere else installs there.

## Upgrading clients: three behaviours worth knowing

* **`apt upgrade <pkg>` is not strictly targeted.** On the Pi,
  `apt upgrade -y codebundler` took the whole pending set with it (codebundler
  plus vim, libnss3, libneatvnc1, notes-cli, mise — nine packages, visible in
  `/var/log/apt/history.log`). When only the one package matters, use
  `apt-get install -y --only-upgrade <pkg>`, and expect the untargeted form to
  apply whatever else is pending.
* **`dnf` caches repo metadata, so a fresh publish can look like nothing new.**
  `dnf -y upgrade codebundler` answered "Nothing to do." minutes after the
  package was published and `createrepo_c` had run; `dnf -y --refresh upgrade
  codebundler` installed 1.0.2-1 immediately. A client that does not pass
  `--refresh` waits for the metadata expiry instead (48 h by default).
* **A stale hand-installed binary in `/usr/local/bin` shadows the packaged one.**
  The Nobara laptop still had a `cmake --install` copy from September in
  `/usr/local/bin`, and `/usr/local/bin` precedes `/usr/bin` in `PATH`, so plain
  `codebundler` ran the pre-1.0.1 build even after `dnf upgrade`. Remove any
  `/usr/local` copy after installing from a package; verify with `type -a
  codebundler`.

## Version and architecture rules

* **One commit per release.** The `.deb`s and the `.rpm` must be built from the
  same source revision — the trees keep old versions installable, so a stray
  half-published release stays in the pool forever.
* **arm64 is built on demand, not automatically.** Rebuilding amd64 + rpm
  without rebuilding arm64 leaves the Pi on the old version while the other
  machines move on. 1.0.2 is published for all three forms
  (`amd64.deb`, `arm64.deb`, `x86_64.rpm`); keep it that way per release, or
  accept the drift knowingly.
* **RPM `Release` is `1`**, whereas the pre-CPack hand-built rpms used
  `1.fc44`. dnf compares version before release, so `1.0.2-1` upgrades
  `1.0.1-2` normally; the repo path (`rpm/fedora/44/x86_64`) is what identifies
  the target distribution, not the file name.
* **Rebuilding without a version bump needs a release bump.** The pool keeps
  superseded packages installable, so republishing different content under the
  same name-version-release is the one thing that must not happen: a client's
  cached checksum would describe a file it no longer receives. Rebuild with a
  bumped release instead:

  ```bash
  cpack -G RPM --config build-pkg/CPackConfig.cmake -B build-pkg/pkgs \
        -D CPACK_RPM_PACKAGE_RELEASE=2
  # .deb equivalent: -D CPACK_DEBIAN_PACKAGE_RELEASE=2
  ```

  That is how `codebundler-1.0.1-2` came to exist: 1.0.1-1 was published, then
  its RPM `Description` turned out to be CPack boilerplate (see below), so the
  fix shipped as 1.0.1-2 rather than as a re-cut of 1.0.1-1. Documentation
  changes shipped as 1.0.2 for the same reason.

## Pitfalls worth not re-learning

* **`cmd | grep -q` under `set -o pipefail` is a race.** `grep -q` exits the
  moment it matches, the producer dies of SIGPIPE (status 141), pipefail
  propagates that, and a MATCH is reported as a failure — roughly one run in
  five (measured: 29/30 correct by pipe, 30/30 correct when the output is
  captured first). This hit the rpm signature check in `publish-rpm.sh` and then
  the `dpkg-deb -I` assertions in `scripts/package.sh`, where the same correct
  package failed twice in a row with *different* messages ("Section is not
  utils", then "Maintainer wrong"). Capture the output into a variable, then
  grep it; never reintroduce a pipe into `grep -q` in a gate.
* **CPack's RPM `Description` defaults to its own boilerplate** — "This is an
  installer created using CPack (https://cmake.org). No additional installation
  instructions provided." — which is what `dnf info codebundler` shows a user.
  `CPACK_PACKAGE_DESCRIPTION` covers the `.deb`; the RPM generator needs
  `CPACK_RPM_PACKAGE_DESCRIPTION` set as well. `scripts/package.sh` asserts the
  boilerplate is gone.
* **Do not check rpm signatures with `rpm -qp --qf '%{SIGPGP:pgpsig}'`.** Header
  signatures (EdDSA, which is what these packages use) do not appear in those
  tags, so every signed package looks unsigned. Use
  `rpm -Kv --dbpath <scratch db with the public key imported>` — that is what
  `publish-rpm.sh` does.
* **A tab-indented line in scdoc must not start with a backtick.** scdoc reads a
  leading `` ` `` as the start of a fenced literal block and aborts the whole man
  page: `Error at <line>: Expected ``` and a newline to begin literal block`.
  Rephrase so the backtick is not the first character of the line.
* `dpkg-deb -c`/`rpm -qlp` show what a package *contains*; only installing it
  shows whether a Fedora client will accept it. Check both `apt` and `dnf` on
  the real clients before calling a release done.
