#!/bin/sh
set -eu

target_dir="$HOME/codex-do"
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
commit_requested=false
commit_message=
push_requested=false

usage() {
    printf 'usage: %s [--commit MESSAGE] [--push]\n' "$0" >&2
    printf 'installs codex-do source scripts to %s\n' "$target_dir" >&2
}

while test "$#" -gt 0; do
    case "$1" in
        --commit)
            if test "$commit_requested" = true; then
                printf '%s: --commit may only be passed once\n' "$0" >&2
                exit 2
            fi
            commit_requested=true
            shift
            if test "$#" -eq 0 || test -z "$1"; then
                printf '%s: --commit requires a non-empty message\n' "$0" >&2
                exit 2
            fi
            commit_message="$1"
            ;;
        --push)
            push_requested=true
            ;;
        -h | --help)
            usage
            exit 0
            ;;
        *)
            usage
            exit 2
            ;;
    esac
    shift
done

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

if ! test -d "$target_dir/.git"; then
    printf '%s: target directory is not a git repo: %s\n' "$0" "$target_dir" >&2
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

printf '\ntarget git status:\n'
git -C "$target_dir" status --short --branch

printf '\ntarget git diff stat:\n'
git -C "$target_dir" diff --stat

if test "$commit_requested" = true; then
    git -C "$target_dir" add -A
    if git -C "$target_dir" diff --cached --quiet; then
        printf '\nno target changes to commit\n'
    else
        git -C "$target_dir" commit -m "$commit_message"
    fi

    printf '\ntarget git status after commit:\n'
    git -C "$target_dir" status --short --branch
fi

if test "$push_requested" = true; then
    if ! git -C "$target_dir" diff --quiet; then
        printf '%s: refusing to push with unstaged target changes\n' "$0" >&2
        exit 1
    fi
    if ! git -C "$target_dir" diff --cached --quiet; then
        printf '%s: refusing to push with staged target changes\n' "$0" >&2
        exit 1
    fi
    if test -n "$(git -C "$target_dir" status --short)"; then
        printf '%s: refusing to push with untracked target files\n' "$0" >&2
        exit 1
    fi
    git -C "$target_dir" push
fi
