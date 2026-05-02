#!/bin/sh
set -eu

target_dir="$HOME/codex-do"
source_dir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)
project_dir=$(git -C "$source_dir" rev-parse --show-toplevel 2> /dev/null) || {
    printf '%s: source directory is not inside a git repo: %s\n' "$0" "$source_dir" >&2
    exit 1
}
project_dir=$(CDPATH='' cd -- "$project_dir" && pwd -P)
source_rel=${source_dir#"$project_dir"/}
commit_requested=false
commit_message=
push_requested=false
project_only=false

usage() {
    printf 'usage: %s [--project-only] [--commit MESSAGE] [--push]\n' "$0" >&2
    printf 'releases codex-do source scripts; external target is %s\n' "$target_dir" >&2
    printf '  --project-only  skip external release; commit and push only this project repo\n' >&2
}

require_origin_up_to_date() {
    repo_dir=$1
    repo_label=$2

    branch=$(git -C "$repo_dir" symbolic-ref --quiet --short HEAD) || {
        printf '%s: refusing to release: %s is not on a branch\n' \
            "$0" "$repo_label" >&2
        exit 1
    }
    origin_ref=origin/$branch

    git -C "$repo_dir" fetch origin
    git -C "$repo_dir" rev-parse --verify --quiet "$origin_ref^{commit}" \
        > /dev/null || {
        printf '%s: refusing to release: %s has no %s ref\n' \
            "$0" "$repo_label" "$origin_ref" >&2
        exit 1
    }

    local_head=$(git -C "$repo_dir" rev-parse HEAD)
    origin_head=$(git -C "$repo_dir" rev-parse "$origin_ref")
    if test "$local_head" = "$origin_head"; then
        return
    fi

    merge_base=$(git -C "$repo_dir" merge-base HEAD "$origin_ref") || {
        printf '%s: refusing to release: %s cannot compare HEAD with %s\n' \
            "$0" "$repo_label" "$origin_ref" >&2
        exit 1
    }

    if test "$merge_base" = "$local_head"; then
        relation=behind
    elif test "$merge_base" = "$origin_head"; then
        relation=ahead
    else
        relation=diverged
    fi

    printf '%s: refusing to release: %s is %s %s\n' \
        "$0" "$repo_label" "$relation" "$origin_ref" >&2
    exit 1
}

while test "$#" -gt 0; do
    case "$1" in
        --project-only)
            project_only=true
            ;;
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

if test "$commit_requested" = false && test "$push_requested" = false; then
    printf '%s: pass --commit MESSAGE, --push, or both\n' "$0" >&2
    exit 2
fi

if test "$source_rel" = "$source_dir"; then
    printf '%s: source directory is not inside project directory: %s\n' \
        "$0" "$source_dir" >&2
    exit 1
fi

if test "$(basename -- "$source_dir")" != codex-do-src; then
    printf '%s: source directory basename is not codex-do-src: %s\n' \
        "$0" "$source_dir" >&2
    exit 1
fi

bad_project_path=$(
    {
        git -C "$project_dir" diff --name-only
        git -C "$project_dir" diff --cached --name-only
        git -C "$project_dir" ls-files --others --exclude-standard
    } | sort -u | while IFS= read -r path; do
        case "$path" in
            "$source_rel"/*)
                ;;
            *)
                printf '%s\n' "$path"
                break
                ;;
        esac
    done
)

if test -n "$bad_project_path"; then
    printf '%s: refusing to release with project changes outside %s: %s\n' \
        "$0" "$source_rel" "$bad_project_path" >&2
    exit 1
fi

if test "$project_only" = false; then
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

    target_dir=$(CDPATH='' cd -- "$target_dir" && pwd -P)
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

    require_origin_up_to_date "$target_dir" "target repo"
fi

executable_file=$(
    find "$source_dir" -name .git -prune -o -type f -perm /111 -print -quit
)
if test -n "$executable_file"; then
    printf '%s: source file has execute bits set: %s\n' \
        "$0" "$executable_file" >&2
    exit 1
fi

if test "$project_only" = false; then
    find "$target_dir" -mindepth 1 -maxdepth 1 ! -name .git \
        -exec rm -rf -- {} +

    find "$source_dir" -mindepth 1 -maxdepth 1 ! -name .git \
        -exec cp -R -- {} "$target_dir/" ';'

    printf 'released codex-do scripts to %s\n' "$target_dir"
else
    printf 'skipping external codex-do release\n'
fi

printf '\nproject git status:\n'
git -C "$project_dir" status --short --branch

printf '\nproject git diff stat:\n'
git -C "$project_dir" diff --stat

if test "$project_only" = false; then
    printf '\ntarget git status:\n'
    git -C "$target_dir" status --short --branch

    printf '\ntarget git diff stat:\n'
    git -C "$target_dir" diff --stat
fi

if test "$commit_requested" = true; then
    committed=false

    git -C "$project_dir" add -A -- "$source_rel"
    if git -C "$project_dir" diff --cached --quiet -- "$source_rel"; then
        printf '\nno project changes to commit\n'
    else
        git -C "$project_dir" commit -m "$commit_message"
        committed=true
    fi

    printf '\nproject git status after commit:\n'
    git -C "$project_dir" status --short --branch

    if test "$project_only" = false; then
        git -C "$target_dir" add -A
        if git -C "$target_dir" diff --cached --quiet; then
            printf '\nno target changes to commit\n'
        else
            git -C "$target_dir" commit -m "$commit_message"
            committed=true
        fi

        printf '\ntarget git status after commit:\n'
        git -C "$target_dir" status --short --branch
    fi

    if test "$committed" = false; then
        printf '%s: --commit requested but there was nothing to commit\n' "$0" >&2
        exit 1
    fi
fi

if test "$push_requested" = true; then
    if ! git -C "$project_dir" diff --quiet; then
        printf '%s: refusing to push with unstaged project changes\n' "$0" >&2
        exit 1
    fi
    if ! git -C "$project_dir" diff --cached --quiet; then
        printf '%s: refusing to push with staged project changes\n' "$0" >&2
        exit 1
    fi
    if test -n "$(git -C "$project_dir" status --short --untracked-files=all)"; then
        printf '%s: refusing to push with untracked project files\n' "$0" >&2
        exit 1
    fi

    if test "$project_only" = false; then
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
    fi
    git -C "$project_dir" push
    if test "$project_only" = false; then
        git -C "$target_dir" push
    fi
fi
