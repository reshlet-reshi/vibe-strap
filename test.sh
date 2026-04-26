#!/bin/sh
set -eu

if [ -z "${1-}" ]; then
    printf '%s\n' 'missing argument 1 (out file)' >&2
    exit 1
fi

out="$1"
expected='vibe-strap stage0'

sh vibe-strap.sh "$out"
actual=$(./"$out")
test "$actual" = "$expected"
printf '%s\n' 'smoke test passed'
