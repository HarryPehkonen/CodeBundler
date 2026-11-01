#!/usr/bin/env bash
#
# package — prove the install rules and the CPack packages, not just that they exist.
#
# CodeBundler has no gate.toml/gate.sh harness, so this is the packaging stage
# as a standalone script: the same shape as Computo's scripts/package.sh
# (commit aacb3d3). Run it as the package gate; it exits non-zero on any failure.
#
# What this asserts, and why each assertion exists:
#
#   1. A fresh configure + build with CODEBUNDLER_REQUIRE_SYSTEM_FSMGINE=ON
#      (packaged artifacts must be deterministic: system FSMgine or loud failure,
#      never a silent network fetch of a moving ref).
#   2. Both DEB and RPM build, with DEB-DEFAULT/RPM-DEFAULT names.
#   3. Contents: exactly /usr/bin/codebundler + the man page. No FSMgine or
#      PicoSHA2 artifacts, no /usr/include leaks — CodeBundler ships only the CLI.
#   4. Control fields: Section: utils, Maintainer, and Depends from shlibdeps.
#   5. Both packaged binaries run (extracted from the .deb and from the .rpm).
#   6. RPM file list: no /usr/share/man directory entries (a package shipping a
#      directory Fedora's `filesystem` owns cannot be installed at all), and no
#      CPack-boilerplate Description (that is what `dnf info` shows).
#   7. The shipped man page documents every option the CLI accepts.
#
# NOTE ON PIPES: every check below captures its command's output into a variable
# and greps that, instead of piping into `grep -q`. Under `set -o pipefail`,
# `grep -q` exits as soon as it matches, the producer dies of SIGPIPE (status
# 141), and pipefail turns a MATCH into a failure — which made this gate report a
# correct package as broken roughly one run in five (seen 2026-11-01: the same
# build failed on "Section is not utils", then on "Maintainer wrong", then
# passed). Never reintroduce `cmd | grep -q` here.
set -euo pipefail
cd "$(dirname "$0")/.."

fail() { echo "FAIL: $*" >&2; exit 1; }

for tool in cmake cpack dpkg-deb dpkg-shlibdeps rpm rpm2cpio cpio; do
    command -v "$tool" >/dev/null 2>&1 || fail "missing tool: $tool"
done

tmp=$(mktemp -d "${TMPDIR:-/tmp}/cb-package-XXXXXX")
trap 'rm -rf "$tmp"' EXIT
pkgroot="$tmp/pkgs"
echo "== fresh configure + build in $tmp/build (REQUIRE_SYSTEM_FSMGINE=ON)"
cmake -S . -B "$tmp/build" -DCMAKE_BUILD_TYPE=Release \
      -DCODEBUNDLER_ENABLE_TESTING=OFF -DCODEBUNDLER_REQUIRE_SYSTEM_FSMGINE=ON \
      > "$tmp/configure.log" 2>&1 || { cat "$tmp/configure.log"; fail "cmake configure failed"; }
cmake --build "$tmp/build" -j "$(nproc)" > "$tmp/build.log" 2>&1 \
    || { tail -30 "$tmp/build.log"; fail "cmake --build failed"; }

echo "== cpack DEB + RPM"
cpack -G "DEB;RPM" --config "$tmp/build/CPackConfig.cmake" -B "$pkgroot" \
    > "$tmp/cpack.log" 2>&1 || { cat "$tmp/cpack.log"; fail "cpack failed"; }
ls -1 "$pkgroot" | sed 's/^/    /'

deb=$(ls "$pkgroot"/codebundler_*_amd64.deb 2>/dev/null | head -1)
rpm=$(ls "$pkgroot"/codebundler-*.rpm 2>/dev/null | head -1)
[ -n "$deb" ] || fail "no codebundler_<version>_amd64.deb — DEB-DEFAULT naming regressed (see $tmp/cpack.log)"
[ -n "$rpm" ] || fail "no codebundler-*.rpm in $pkgroot"

echo "== contents ($deb)"
deb_contents=$(dpkg-deb -c "$deb")
printf '%s\n' "$deb_contents" | sed 's/^/    /'
grep -qE '\./usr/bin/codebundler$' <<<"$deb_contents" \
    || fail "package does not ship /usr/bin/codebundler"
grep -qE '\./usr/share/man/man1/codebundler\.1(\.gz)?$' <<<"$deb_contents" \
    || fail "package does not ship the man page"

# No dependency-source leaks: FSMgine/PicoSHA2 are built in-tree; if their
# install rules leak into ours, this catches it in the gate, not on a system.
leak=$(grep -Ei 'fsmgine|picosha|/usr/include/' <<<"$deb_contents" || true)
[ -z "$leak" ] || fail "dependency/header leak in package: $leak"

echo "== control fields"
deb_control=$(dpkg-deb -I "$deb")
printf '%s\n' "$deb_control" | sed -n '1,12p' | sed 's/^/    /'
grep -q '^ Section: utils$' <<<"$deb_control" || fail "Section is not utils"
grep -q '^ Maintainer: Harri Pehkonen <harry.pehkonen@gmail.com>$' <<<"$deb_control" \
    || fail "Maintainer wrong"
grep -q '^ Depends: ' <<<"$deb_control" || fail "no Depends (SHLIBDEPS not applied)"

echo "== packaged binary runs"
dpkg-deb -x "$deb" "$tmp/root"
"$tmp/root/usr/bin/codebundler" --help > /dev/null 2>&1 || fail "packaged binary does not run"

# 6. RPM contents. The DEB checks above cannot see this: on Fedora a package that
#    ships a directory the `filesystem` package owns is not merely untidy, it is
#    UNINSTALLABLE (rpm aborts: "file /usr/share/man/man1 conflicts with file from
#    package filesystem"). CPack's RPM generator lists those directories by
#    default, so this asserts the exclusion in cmake/CPack.cmake still holds.
echo "== rpm contents ($rpm)"
rpm_files=$(rpm -qlp "$rpm")
printf '%s\n' "$rpm_files" | sed 's/^/    /'
grep -qx '/usr/bin/codebundler' <<<"$rpm_files" || fail "rpm does not ship /usr/bin/codebundler"
grep -qE '^/usr/share/man/man1/codebundler\.1(\.gz)?$' <<<"$rpm_files" \
    || fail "rpm does not ship the man page"
if grep -qE '^/usr/share/man$|^/usr/share/man/man1$' <<<"$rpm_files"; then
    fail "rpm ships /usr/share/man directories (conflicts with Fedora's filesystem package)"
fi
rpm_license=$(rpm -qp --qf '%{LICENSE}' "$rpm")
grep -qi unlicense <<<"$rpm_license" || fail "rpm License is not Unlicense (got: $rpm_license)"
# CPack's default Description is "This is an installer created using CPack ...",
# which is what users see in `dnf info`. cmake/CPack.cmake overrides it.
if grep -q 'created using CPack' <<<"$(rpm -qp --qf '%{DESCRIPTION}' "$rpm")"; then
    fail "rpm Description is CPack boilerplate (set CPACK_RPM_PACKAGE_DESCRIPTION)"
fi
rpm -Kv "$rpm" > /dev/null 2>&1 || true # unsigned at build time is expected: signing happens in publish-rpm.sh

echo "== rpm packaged binary runs"
rm -rf "$tmp/rpmroot" && mkdir -p "$tmp/rpmroot"
rpm2cpio "$rpm" | (cd "$tmp/rpmroot" && cpio -idm --quiet) || fail "could not extract the rpm"
"$tmp/rpmroot/usr/bin/codebundler" --help > /dev/null 2>&1 || fail "rpm packaged binary does not run"

# 7. Docs cannot drift: every option the CLI accepts must be documented in the
#    shipped man page as an option DEFINITION — a line that starts with a bold
#    marker (`*--opt* <arg>`), which is how scdoc renders an option. A mere
#    mention in prose or in an example does not count, and this must fail if the
#    definition is deleted while the option survives in an example (both verified
#    by sabotaging a copy of the man page and re-running this script).
#    --output-dir was parsed by main.cpp and documented nowhere until this
#    assertion existed. Both packages carry the page, so a gap here is a gap in
#    what users get.
#    Keep this check line-based (grep). A re-implementation with Python's
#    re.search and no re.M will happily match a "*" many lines above the option,
#    because [^*] crosses newlines, and report a sabotaged page as fine.
echo "== man page documents every CLI option =="
cli_options=$(grep -oE '"--?[a-z][a-z-]*"' src/main.cpp | tr -d '"' | sort -u)
undocumented=""
for option in $cli_options; do
    grep -qE "^\*.*${option}" man/codebundler.1.scd || undocumented="$undocumented $option"
done
[ -z "$undocumented" ] || fail "man page does not define:$undocumented"
echo "    $(wc -w <<<"$cli_options") options documented:$(printf ' %s' $cli_options)"

echo "PASS: package contents, control fields, rpm file list, man-page coverage, and smoke tests all verified"
