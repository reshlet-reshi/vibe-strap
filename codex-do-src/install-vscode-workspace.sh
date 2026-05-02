#!/bin/sh
set -eu

program=$(basename -- "$0")
script_dir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)
workspace_dir=.
check_only=false

usage() {
    printf 'usage: %s [--check] [--workspace DIR]\n' "$0" >&2
}

err() {
    printf '%s: %s\n' "$program" "$1" >&2
}

while test "$#" -gt 0; do
    case "$1" in
        --check)
            check_only=true
            ;;
        --workspace)
            shift
            if test "$#" -eq 0 || test -z "$1"; then
                err '--workspace requires a non-empty directory'
                exit 2
            fi
            workspace_dir=$1
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

if ! command -v python3 > /dev/null 2>&1; then
    err 'python3 is required to edit VS Code JSON settings'
    exit 1
fi

workspace_dir=$(CDPATH='' cd -- "$workspace_dir" && pwd -P) || {
    err "workspace directory not found: $workspace_dir"
    exit 1
}

workspace_endpoint_script="$workspace_dir/codex-do-src/vscode-unpersisted-documents.js"
if test -f "$workspace_endpoint_script"; then
    endpoint_script="$workspace_endpoint_script"
else
    endpoint_script="$script_dir/vscode-unpersisted-documents.js"
fi
if ! test -f "$endpoint_script"; then
    err "endpoint script not found: $endpoint_script"
    exit 1
fi
json_installer="$script_dir/install-vscode-workspace-json.py"
if ! test -f "$json_installer"; then
    err "JSON installer not found: $json_installer"
    exit 1
fi

case "$endpoint_script" in
    "$workspace_dir"/*)
        endpoint_setting=./${endpoint_script#"$workspace_dir"/}
        ;;
    *)
        endpoint_setting=$endpoint_script
        ;;
esac

vscode_dir="$workspace_dir/.vscode"
settings_path="$vscode_dir/settings.json"
extensions_path="$vscode_dir/extensions.json"

if test "$check_only" = false; then
    mkdir -p "$vscode_dir"
fi

check_arg=false
if test "$check_only" = true; then
    check_arg=true
fi

python3 "$json_installer" \
    "$settings_path" \
    "$extensions_path" \
    "$endpoint_setting" \
    "$check_arg"

if test "$check_only" = true; then
    printf 'VS Code workspace install check OK: %s\n' "$workspace_dir"
else
    printf 'installed VS Code workspace config: %s\n' "$workspace_dir"
fi
