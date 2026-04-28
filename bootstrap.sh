#!/bin/sh

# "strict mode"
set -eu

# DEBUG : always clean for now
rm -rf ./.ignore ./runme
# LATER
#
# already bootstrapped? just exec runme!
#if test -f runme; then
#    exec ./runme
#fi
#

# platform check
platform="$(uname -m)-$(uname -s)"
expected='x86_64-Linux'
if test "$platform" != "$expected"; then
    echo "'$platform' != '$expected'!" >&2
    exit 1
fi

# tmp dir to unpack musl cc
tmp="$(mktemp -d)"
cleanup() {
    rm -rf "$tmp"
}
trap 'cleanup' EXIT
trap 'cleanup; exit 130' HUP INT TERM

# unpack cc
muslcc_archive="./vendor/muslcc/x86_64-linux-musl-cross.tgz"
tar -xzf "$muslcc_archive" -C "${tmp}"

# cc runme.c
cc="${tmp}/x86_64-linux-musl-cross/bin/x86_64-linux-musl-cc"
"$cc" -static -o ./runme ./runme.c

# clean tmp
cleanup
trap - EXIT HUP INT TERM

# make rocket go now
exec ./runme
