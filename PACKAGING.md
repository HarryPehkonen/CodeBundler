# Packaging CodeBundler

One package, `codebundler`, for both package managers. It ships exactly two
files: `/usr/bin/codebundler` and
`/usr/share/man/man1/codebundler.1.gz` (the man page is generated from
`man/codebundler.1.scd` at build time with `scdoc`, so there is no committed
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

* `codebundler_1.0.1_amd64.deb`
* `codebundler-1.0.1-1.x86_64.rpm`

`CODEBUNDLER_REQUIRE_SYSTEM_FSMGINE=ON` is not optional for a package build: it
makes configuration fail loudly when the system FSMgine is missing instead of
network-fetching a moving ref (see `external/CMakeLists.txt`), which is what
keeps an artifact reproducible. It needs FSMgine installed (`/usr/local` on host
`hermes`), plus `scdoc` for the man page and `dpkg-shlibdeps` for the `.deb`
`Depends:` field.

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
   `License: Unlicense`, and the extracted binary runs.

Assertion 4 exists because of a real failure: CPack's RPM generator lists
`/usr/share/man` and `/usr/share/man/man1` as package files, Fedora's
`filesystem` package already owns both, and rpm therefore refuses the whole
transaction (`file /usr/share/man/man1 conflicts with file from package
filesystem`). The package builds fine and looks fine in `rpm -qlp` — it just
cannot be installed on a Fedora machine. `cmake/CPack.cmake` excludes those two
paths; the gate keeps them excluded.

## Publishing

The repository lives on the VPS and is served statically; only signed packages
reach it. Everything runs on host `hermes` (the machine holding the signing
key), driven by `~/hermes-workspace/pkg-repo/`:

```bash
cd ~/hermes-workspace/pkg-repo
./publish-apt.sh add <path>/codebundler_1.0.1_amd64.deb   # pool/main + Packages + signed InRelease + rsync
./publish-rpm.sh add <path>/codebundler-1.0.1-1.x86_64.rpm # Packages/ + rpmsign + createrepo_c + rsync
./publish-rpm.sh check                                     # verify signatures only, no writes, no push
python3 gen-packages.py                                    # regenerate pehkonen.dev/packages/index.html
```

Both scripts regenerate metadata from scratch, sign it, and `rsync --delete` the
finished tree to `notes-server:/srv/repo/{apt,rpm}`; the VPS holds no key and no
packaging tooling. `publish-rpm.sh` refuses to publish anything unsigned.

Client setup is on https://pehkonen.dev/ (apt and dnf snippets, also in
`README.md`). Verify a release the way the machines consume it:

```bash
sudo apt update && apt-cache policy codebundler      # Debian 13 (trixie) client
sudo apt install codebundler
ssh laptop 'sudo dnf --refresh upgrade codebundler'   # Nobara / Fedora 44 client
```

Both fleet clients are wired to these repos already, which makes them the honest
test: a package that installs nowhere else installs there.

## Version and architecture rules

* **One commit per release.** The `.deb` and the `.rpm` must be built from the
  same source revision — the trees keep old versions installable, so a stray
  half-published release stays in the pool forever.
* **arm64 is built on demand, not automatically.** The apt pool currently holds
  a 1.0.0 arm64 build; publishing a new version without rebuilding arm64 leaves
  arm64 machines (the Pi) on the old version while amd64 moves on. Rebuild it
  there and `publish-apt.sh add` it, or accept the drift knowingly.
* **RPM `Release` is `1`**, whereas the pre-CPack hand-built rpms used
  `1.fc44`. dnf compares version before release, so `1.0.1-1` upgrades
  `1.0.0-1.fc44` normally; the repo path (`rpm/fedora/44/x86_64`) is what
  identifies the target distribution, not the file name.

## Pitfalls worth not re-learning

* **Do not check rpm signatures with `rpm -qp --qf '%{SIGPGP:pgpsig}'`.** Header
  signatures (EdDSA, which is what these packages use) do not appear in those
  tags, so every signed package looks unsigned. Use
  `rpm -Kv --dbpath <scratch db with the public key imported>` — that is what
  `publish-rpm.sh` does.
* **`rpm -Kv … | grep -q` under `set -o pipefail` is a race.** `grep -q` exits
  the moment it matches, rpm dies of SIGPIPE (status 141), pipefail propagates
  that, and the signature check reports a signed package as UNSIGNED roughly one
  run in five — enough to refuse a good publish. Capture the output into a
  variable first, then grep it.
* `dpkg -c`/`rpm -qlp` show what a package *contains*; only installing it shows
  whether a Fedora client will accept it. Check both `apt` and `dnf` on the real
  clients before calling a release done.
