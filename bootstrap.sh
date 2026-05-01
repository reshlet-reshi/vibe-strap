#!/bin/sh

set -eu

rm -rf ./.ignore ./runme

platform="$(uname -m)-$(uname -s)"
expected='x86_64-Linux'
if test "$platform" != "$expected"; then
    echo "'$platform' != '$expected'!" >&2
    exit 1
fi

tmp="$(mktemp -d)"
cleanup() {
    rm -rf "$tmp"
}
trap 'cleanup' EXIT
trap 'cleanup; exit 130' HUP INT TERM

muslcc_archive="./vendor/muslcc/x86_64-linux-musl-cross.tgz"
tar -xzf "$muslcc_archive" -C "${tmp}"

cc="${tmp}/x86_64-linux-musl-cross/bin/x86_64-linux-musl-cc"
"$cc" -static -o ./runme ./runme.c ./src/c/*.c

cleanup
trap - EXIT HUP INT TERM

exec ./runme
