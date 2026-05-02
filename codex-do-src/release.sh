#!/bin/sh
set -eu

target_dir="$HOME/codex-do"
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)

usage() {
    printf 'usage: %s\n' "$0" >&2
    printf 'installs codex-do source scripts to %s\n' "$target_dir" >&2
}

if test "$#" -ne 0; then
    usage
    exit 2
fi

if ! test -e "$target_dir"; then
    printf '%s: target directory does not exist: %s\n' "$0" "$target_dir" >&2
    exit 1
fi

if ! test -d "$target_dir"; then
    printf '%s: target exists but is not a directory: %s\n' \
        "$0" "$target_dir" >&2
    exit 1
fi

if ! test -w "$target_dir"; then
    printf '%s: target directory is not writable: %s\n' "$0" "$target_dir" >&2
    exit 1
fi

target_dir=$(CDPATH= cd -- "$target_dir" && pwd -P)
target_base=$(basename -- "$target_dir")

if test "$target_base" != codex-do; then
    printf '%s: target directory basename is not codex-do: %s\n' \
        "$0" "$target_dir" >&2
    exit 1
fi

if test "$target_dir" = "$source_dir"; then
    printf '%s: source and target directories are the same: %s\n' \
        "$0" "$target_dir" >&2
    exit 1
fi

executable_file=$(
    find "$source_dir" -name .git -prune -o -type f -perm /111 -print -quit
)
if test -n "$executable_file"; then
    printf '%s: source file has execute bits set: %s\n' \
        "$0" "$executable_file" >&2
    exit 1
fi

find "$target_dir" -mindepth 1 -maxdepth 1 ! -name .git \
    -exec rm -rf -- {} +

find "$source_dir" -mindepth 1 -maxdepth 1 ! -name .git \
    -exec cp -R -- {} "$target_dir/" ';'

printf 'released codex-do scripts to %s\n' "$target_dir"
