#!/bin/sh
set -eu

if test "$#" -ne 0; then
    printf 'usage: %s\n' "$0" >&2
    exit 2
fi

exec wget -T 2 -t 1 -qO- \
    http://127.0.0.1:17817/api/unpersisted-documents
