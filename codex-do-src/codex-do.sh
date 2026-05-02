#!/bin/sh
set -eu

usage() {
    printf 'usage: %s <command> [args...]\n' "$0" >&2
}

if test "$#" -lt 1; then
    usage
    exit 2
fi

command_name="$1"
shift

script_dir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)

case "$command_name" in
    query-vscode-unpersisted-documents)
        exec sh "$script_dir/query-vscode-unpersisted-documents.sh" "$@"
        ;;
    *)
        printf '%s: denied command: %s\n' "$0" "$command_name" >&2
        exit 126
        ;;
esac
