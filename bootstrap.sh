#!/bin/sh

set -eu

# Locate the repository root from the script path so callers can run this from
# any working directory.
name="$(basename "$0")"
dir="$(dirname "$0")"

errln() {
    if test "$#" -ne 1; then
        printf 'errln must be passed exactly one argument\n' >&2
        exit 2
    fi
    printf '%s\n' "$1" >&2
}

# The vendored compiler is a Linux x86_64 musl toolchain, so keep the bootstrap
# host check explicit.
machine="$(uname -m)"
if test "${machine}" != x86_64; then
    errln "${name}: we only support x86_64 hosts for now, not '${machine}'"
    exit 1
fi
system="$(uname -s)"
if test "${system}" != Linux; then
    errln "${name}: we only support Linux hosts for now, not '${system}'"
    exit 1
fi

cd "${dir}" > /dev/null

# Reuse unpacked vendor archives between runs. The whole directory is ignored
# by git.
cache="./.ignore/.cache"
mkdir -p "${cache}"

# Bootstrap runme.c with the vendored static musl compiler.
muslcc_archive="./vendor/muslcc/x86_64-linux-musl-cross.tgz"
if ! test -f "$muslcc_archive"; then
    errln "${name}: vendored muslcc archive '${muslcc_archive}' not found"
    exit 1
fi

cc="${cache}/x86_64-linux-musl-cross/bin/x86_64-linux-musl-cc"
if ! test -f "$cc"; then
    # Recreate the expected unpack root if the compiler is missing.
    rm -rf "${cache}/x86_64-linux-musl-cross"
    tar -xzf "$muslcc_archive" -C "$cache"
fi

if ! test -f "$cc"; then
    errln "${name}: vendored muslcc compiler '${cc}' not found"
    exit 1
fi

# The resulting binary is intentionally untracked; see .gitignore.
"$cc" -static -o ./runme ./runme.c
