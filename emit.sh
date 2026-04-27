set -eu

# Sourced by vibe-strap.sh. The caller passes the include path as $2 and output
# file as $3. This helper truncates the output before defining emit functions.
if [ -z "${2-}" ]; then
    printf '%s\n' 'missing argument 2 (include path)' >&2
    exit 1
fi

if [ -z "${3-}" ]; then
    printf '%s\n' 'missing argument 3 (out file)' >&2
    exit 1
fi

include_path=$2
out="$3"
unpatched=

# Create the parent directory when the output path includes one.
out_dir=${out%/*}
if [ "$out_dir" != "$out" ]; then
    mkdir -p "$out_dir"
fi

# Ensure/trancate output file.
: > "$out"

emit_raw() {
    printf '%s' "$1" >> "$out"
}

emit_hex() {
    bytes=
    for byte do
        # Convert each hex byte to an octal escape for POSIX printf %b.
        bytes="${bytes}\\$(printf '%03o' "0x$byte")"
    done
    printf '%b' "$bytes" >> "$out"
}

include() {
    . "$include_path/$1"
}

patch_at() {
    offset=$1
    dd of="$out" bs=1 seek="$offset" conv=notrunc 2>/dev/null
}

emit_unpatched_size() {
    set -- $(wc -c < "$out")
    offset=$1
    unpatched="${unpatched}${unpatched:+ }$offset"
    emit_hex 00 00 00 00
}

patch_sizes() {
    set -- $(wc -c < "$out")
    size=$1

    b0=$((size & 255))
    b1=$(((size >> 8) & 255))
    b2=$(((size >> 16) & 255))
    b3=$(((size >> 24) & 255))
    bytes=$(printf '\\%03o\\%03o\\%03o\\%03o' "$b0" "$b1" "$b2" "$b3")

    for offset in $unpatched; do
        printf '%b' "$bytes" | patch_at "$offset"
    done
}

end_emit() {
    chmod +x "$out"
    exit 0
}
