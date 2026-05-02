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

    for _file in bootstrap.sh codex-do.sh query-vscode-unpersisted-documents.sh README.md release.sh; do
        if test -f "$source_dir/$_file"; then
            say "ok: source has $_file"
        else
            err "source file not found: $source_dir/$_file"
            mark_failed
        fi
    done

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
    _settings="$_workspace_dir/.vscode/settings.json"
    _endpoint_script="$_workspace_dir/.vscode/vscode-unpersisted-documents.js"
    _extensions="$_workspace_dir/.vscode/extensions.json"

    if ! test -d "$_workspace_dir/.vscode"; then
        warn "no .vscode directory beside $source_dir; skipping workspace checks"
        return
    fi

    if test -f "$_settings"; then
        say 'ok: workspace has .vscode/settings.json'
    else
        err "workspace settings not found: $_settings"
        mark_failed
    fi

    if test -f "$_endpoint_script"; then
        say 'ok: workspace has VS Code unpersisted-documents endpoint script'
    else
        err "VS Code endpoint script not found: $_endpoint_script"
        mark_failed
    fi

    if test -f "$_extensions" && grep -q 'mkloubert.vs-rest-api' "$_extensions"; then
        say 'ok: workspace recommends mkloubert.vs-rest-api'
    else
        err 'workspace does not recommend mkloubert.vs-rest-api in .vscode/extensions.json'
        mark_failed
    fi
}

check_target() {
    if ! test -d "$target_dir"; then
        err "target directory not found: $target_dir"
        mark_failed
        return
    fi

    for _file in bootstrap.sh codex-do.sh query-vscode-unpersisted-documents.sh README.md release.sh; do
        if test -f "$target_dir/$_file"; then
            say "ok: target has $_file"
        else
            err "target file not found: $target_dir/$_file"
            mark_failed
        fi
    done
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

    find "$source_dir" -mindepth 1 -maxdepth 1 ! -name .git \
        -exec cp -R -- {} "$target_dir/" ';'

    say "installed codex-do scripts to $target_dir"
}

run_checks() {
    check_source_layout
    require_command git
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

    for _file in bootstrap.sh codex-do.sh query-vscode-unpersisted-documents.sh README.md release.sh; do
        if ! test -f "$_self_home/codex-do/$_file"; then
            err "self-test expected installed file: $_file"
            exit 1
        fi
    done

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
