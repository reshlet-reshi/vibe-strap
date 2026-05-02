#!/bin/sh
set -eu

program=$(basename -- "$0")
source_dir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)
target_dir=${CODEX_DO_DIR:-"$HOME/codex-do"}
check_only=false
force=false
self_test=false
skip_vscode_check=false

usage() {
    printf 'usage: %s [--check] [--force] [--self-test] [--skip-vscode-check] [--target DIR]\n' "$0" >&2
    printf 'installs/checks codex-do scripts at %s\n' "$target_dir" >&2
}

err() {
    printf '%s: %s\n' "$program" "$1" >&2
}

warn() {
    printf 'warn: %s\n' "$1" >&2
}

say() {
    printf '%s\n' "$1"
}

absolute_target_path() {
    _target=$1
    _parent=$(dirname -- "$_target")
    _base=$(basename -- "$_target")

    if test -d "$_target"; then
        CDPATH='' cd -- "$_target" && pwd -P
        return
    fi

    if test -d "$_parent"; then
        _parent_abs=$(CDPATH='' cd -- "$_parent" && pwd -P)
        printf '%s/%s\n' "$_parent_abs" "$_base"
        return
    fi

    printf '%s\n' "$_target"
}

while test "$#" -gt 0; do
    case "$1" in
        --check)
            check_only=true
            ;;
        --force)
            force=true
            ;;
        --self-test)
            self_test=true
            ;;
        --skip-vscode-check)
            skip_vscode_check=true
            ;;
        --target)
            shift
            if test "$#" -eq 0 || test -z "$1"; then
                err '--target requires a non-empty directory'
                exit 2
            fi
            target_dir=$1
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

target_dir=$(absolute_target_path "$target_dir")

check_failed=false

mark_failed() {
    check_failed=true
}

require_command() {
    if command -v "$1" > /dev/null 2>&1; then
        say "ok: found $1"
    else
        err "missing required command: $1"
        mark_failed
    fi
}

check_source_layout() {
    _base=$(basename -- "$source_dir")
    case "$_base" in
        codex-do-src | codex-do)
            ;;
        *)
            err "unexpected source directory basename: $_base"
            mark_failed
            ;;
    esac

    if test -f "$source_dir/codex-do.sh"; then
        say 'ok: source has codex-do.sh'
    else
        err "source dispatcher not found: $source_dir/codex-do.sh"
        mark_failed
    fi

    _executable_file=$(
        find "$source_dir" -name .git -prune -o -type f -perm /111 -print -quit
    )
    if test -n "$_executable_file"; then
        err "source file has execute bits set: $_executable_file"
        mark_failed
    else
        say 'ok: source scripts are not executable'
    fi
}

check_workspace_settings() {
    _workspace_dir=$(dirname -- "$source_dir")

    if ! test -d "$_workspace_dir/.vscode"; then
        warn "no .vscode directory beside $source_dir; skipping workspace checks"
        return
    fi

    if sh "$source_dir/install-vscode-workspace.sh" \
        --workspace "$_workspace_dir" \
        --check
    then
        say 'ok: VS Code workspace config is installed'
    else
        err 'VS Code workspace config needs install: sh codex-do-src/install-vscode-workspace.sh'
        mark_failed
    fi
}

check_target() {
    if ! test -d "$target_dir"; then
        err "target directory not found: $target_dir"
        mark_failed
        return
    fi

    if test -f "$target_dir/codex-do.sh"; then
        say 'ok: target has codex-do.sh'
    else
        err "target dispatcher not found: $target_dir/codex-do.sh"
        mark_failed
        return
    fi

    if _diff_output=$(diff -qr --exclude=.git "$source_dir" "$target_dir"); then
        say 'ok: target matches source'
    else
        printf '%s\n' "$_diff_output" >&2
        err "target is not in sync with source: $target_dir"
        mark_failed
    fi
}

check_vscode_endpoint() {
    _url=${CODEX_DO_VSCODE_UNPERSISTED_DOCUMENTS_URL:-http://127.0.0.1:17817/api/unpersisted-documents}

    if test "$skip_vscode_check" = true; then
        say 'ok: skipped VS Code REST API endpoint check'
        return
    fi

    if ! command -v wget > /dev/null 2>&1; then
        err 'wget is required to query the VS Code REST API endpoint'
        mark_failed
        return
    fi

    if _body=$(wget -T 2 -t 1 -qO- "$_url" 2> /dev/null); then
        case "$_body" in
            *'"ok":true'*)
                say "ok: VS Code REST API endpoint answered at $_url"
                ;;
            *)
                err "VS Code REST API endpoint returned an unexpected response: $_url"
                mark_failed
                ;;
        esac
    else
        err "VS Code REST API endpoint is not reachable: $_url"
        warn 'install/enable VS Code extension mkloubert.vs-rest-api, open this workspace, and reload VS Code'
        mark_failed
    fi
}

print_codex_rule() {
    printf '\nCodex approval rule:\n'
    printf 'prefix_rule(pattern=["sh", "%s/codex-do.sh"], decision="allow")\n' "$target_dir"
}

install_target() {
    _target_parent=$(dirname -- "$target_dir")
    _target_base=$(basename -- "$target_dir")

    if test "$_target_base" != codex-do; then
        err "target directory basename must be codex-do: $target_dir"
        exit 1
    fi

    if ! test -d "$_target_parent"; then
        mkdir -p "$_target_parent"
    fi
    if ! test -d "$target_dir"; then
        mkdir -p "$target_dir"
    fi
    target_dir=$(absolute_target_path "$target_dir")

    if test "$target_dir" = "$source_dir"; then
        say "source and target are the same directory: $target_dir"
        return
    fi

    if ! test -w "$target_dir"; then
        err "target directory is not writable: $target_dir"
        exit 1
    fi

    _first_entry=$(find "$target_dir" -mindepth 1 -maxdepth 1 ! -name .git -print -quit)
    if test -n "$_first_entry" && ! test -f "$target_dir/codex-do.sh" && test "$force" = false; then
        err "target exists and does not look like codex-do: $target_dir"
        err 'pass --force only if this directory is safe to overwrite'
        exit 1
    fi

    find "$target_dir" -mindepth 1 -maxdepth 1 ! -name .git \
        -exec rm -rf -- {} +

    find "$source_dir" -mindepth 1 -maxdepth 1 ! -name .git \
        -exec cp -R -- {} "$target_dir/" ';'

    say "installed codex-do scripts to $target_dir"
}

run_checks() {
    check_source_layout
    require_command git
    require_command python3
    require_command wget
    check_workspace_settings
    check_target
    check_vscode_endpoint

    if test "$check_failed" = true; then
        return 1
    fi
    return 0
}

run_self_test() {
    _tmp=$(mktemp -d)
    _self_home="$_tmp/home"
    _out="$_tmp/out.txt"
    _err="$_tmp/err.txt"
    mkdir -p "$_self_home"

    cleanup_self_test() {
        rm -rf "$_tmp"
    }
    trap cleanup_self_test EXIT HUP INT TERM

    if CODEX_DO_DIR="$_self_home/codex-do" \
        HOME="$_self_home" \
        sh "$source_dir/bootstrap.sh" --check --skip-vscode-check \
        > "$_out" 2> "$_err"; then
        err 'self-test expected --check to fail before install'
        exit 1
    fi

    CODEX_DO_DIR="$_self_home/codex-do" \
        HOME="$_self_home" \
        sh "$source_dir/bootstrap.sh" --skip-vscode-check \
        > "$_out" 2> "$_err"

    CODEX_DO_DIR="$_self_home/codex-do" \
        HOME="$_self_home" \
        sh "$source_dir/bootstrap.sh" --check --skip-vscode-check \
        > "$_out" 2> "$_err"

    if CODEX_DO_VSCODE_UNPERSISTED_DOCUMENTS_URL=http://127.0.0.1:9/api/unpersisted-documents \
        HOME="$_self_home" \
        sh "$_self_home/codex-do/codex-do.sh" query-vscode-unpersisted-documents \
        > "$_out" 2> "$_err"; then
        err 'self-test expected query to fail when the VS Code endpoint is unreachable'
        exit 1
    fi
    if ! grep -q 'VS Code REST API endpoint is not reachable' "$_err"; then
        err 'self-test expected a helpful VS Code endpoint failure message'
        exit 1
    fi

    cleanup_self_test
    trap - EXIT HUP INT TERM
    say 'self-test OK'
}

if test "$self_test" = true; then
    run_self_test
    exit 0
fi

if test "$check_only" = false; then
    check_source_layout
    if test "$check_failed" = true; then
        exit 1
    fi
    install_target
fi

if run_checks; then
    print_codex_rule
    say 'bootstrap OK'
else
    print_codex_rule
    err 'bootstrap checks failed'
    exit 1
fi
