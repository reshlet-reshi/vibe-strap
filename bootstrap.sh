#!/bin/sh

set -eu

name="$(basename "$0")"

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

rm -rf ./.ignore ./runme

cache="./.ignore/.cache"
mkdir -p "${cache}"

muslcc_archive="./vendor/muslcc/x86_64-linux-musl-cross.tgz"
if ! test -f "$muslcc_archive"; then
    errln "${name}: vendored muslcc archive '${muslcc_archive}' not found"
    exit 1
fi

cc="${cache}/x86_64-linux-musl-cross/bin/x86_64-linux-musl-cc"
if ! test -f "$cc"; then
    rm -rf "${cache}/x86_64-linux-musl-cross"
    tar -xzf "$muslcc_archive" -C "$cache"
fi

if ! test -f "$cc"; then
    errln "${name}: vendored muslcc compiler '${cc}' not found"
    exit 1
fi

"$cc" -static -o ./runme ./runme.c
