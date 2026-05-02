#!/bin/sh
set -eu

if test "$#" -ne 0; then
    printf 'usage: %s\n' "$0" >&2
    exit 2
fi

url=${CODEX_DO_VSCODE_UNPERSISTED_DOCUMENTS_URL:-http://127.0.0.1:17817/api/unpersisted-documents}

if ! command -v wget > /dev/null 2>&1; then
    printf '%s: wget is required to query VS Code unpersisted documents\n' "$0" >&2
    exit 1
fi

if body=$(wget -T 2 -t 1 -qO- "$url" 2> /dev/null); then
    printf '%s\n' "$body"
else
    printf '%s: VS Code REST API endpoint is not reachable: %s\n' "$0" "$url" >&2
    printf '%s: install/enable VS Code extension mkloubert.vs-rest-api, open this workspace, and reload VS Code\n' "$0" >&2
    printf '%s: then retry or run: sh codex-do-src/bootstrap.sh --check\n' "$0" >&2
    exit 1
fi
