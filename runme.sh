#!/bin/sh

# https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#set
# -e 
#   when any command fails, the shell immediately shall exit,
#   with the following exceptions:
#       1. The failure of any individual command in a multi-command 
#          pipeline shall not cause the shell to exit. 
#          Only the failure of the pipeline itself shall be considered.
#       2. The -e setting shall be ignored when executing the compound list 
#          following the while, until, if, or elif reserved word, 
#          a pipeline beginning with the ! reserved word, 
#          or any command of an AND-OR list other than the last.
#       3. If the exit status of a compound command other than a subshell 
#          command was the result of a failure while -e was being ignored, 
#          then -e shall not apply to this command.
#
#   This requirement applies to the shell environment and each subshell 
#   environment separately. For example, in:
#
#       `set -e; (false; echo one) | cat; echo two`
#
#   the false command causes the subshell to exit without executing `echo one;` 
#   however, `echo two` is executed because the exit status of 
#   the pipeline `(false; echo one) | cat` is zero.
#
# -u 
#   When the shell tries to expand an unset parameter other than 
#   the '@' and '*' special parameters, it shall write a message 
#   to standard error and the expansion shall fail
#
set -eu

# dir, name = os.path.split(sys.argv[0])
name="$(basename "$0")"
dir="$(dirname "$0")"

# fprintf(stderr, "%s\n", x);
errln() {
    if test "$#" -ne 1; then
        printf 'errln must be passed exactly one argument\n' >&2
        exit 2
    fi
    printf '%s\n' "$1" >&2
}

# expect host to be x86_64-Linux
machine="$(uname -m)"
if test "${machine}" != x86_64; then
    errln "${name}: we only support x86_64 hosts for now, not '${machine}'"
    exit 1
fi
system="$(uname -s)"
if test "${system}" != Linux; then
    errln "${name}: we only support Linux hosts for now, not '${system}'"
    exit 1
fi

# be grumpy if CDPATH is set
cdpath_is_set() {
    # if CDPATH is set _at all_ (even set to '')
    #   ${CDPATH+x} == x
    # otherwise
    #   ${CDPATH+x} == ''
    #
    # thus,
    #  this function checks
    #  if CDPATH is set _at all_
    test "${CDPATH+x}" = x
}
if cdpath_is_set; then
    errln "${name}: we refuse to run if CDPATH is set"
    errln 'try again in a subshell with CDPATH unset'
    errln 'example: (unset CDPATH && <same command>)'
    exit 1
fi

# does dir exist?
if ! test -d "${dir}"; then
    errln "${name}: script directory '${dir}' not found"
    exit 1
fi

# run from our dir, to avoid confusion.
cd "${dir}" > /dev/null

# vendored shellcheck around and executable?
shellcheck="./vendor/shellcheck/${machine}-${system}/shellcheck"
if ! test -f "${shellcheck}"; then
    errln "${name}: vendored shellcheck '${shellcheck}' not found"
    exit 1
fi
if ! test -x "${shellcheck}"; then
    chmod +x "${shellcheck}" || {
        errln "${name}: vendored shellcheck '${shellcheck}' not executable"
        exit 1
    }
fi

# run shellcheck
exec_for_each_find_by_name() {
    # The '{} +' line noise means: 
    # Use find's batched exec form so each tool run receives as many matching
    # paths as will fit in one command line, avoiding one process per file.
    find . -name "$1" -exec "$2" {} +
}
exec_for_each_find_by_name '*.sh' "${shellcheck}"

# re-creatable tmp dir
tmp=
cleanup() {
    if [ -n "$tmp" ]; then
        rm -rf "$tmp"
        tmp=
    fi
}
trap cleanup EXIT
trap 'cleanup; exit 130' HUP INT TERM

# little combinator to make calling 'sh -c' with two args nicer
sh_call_2() {
    sh -c "$4" "$1" "$2" "$3"
}

# assemble script to binary
_assemble() {
    _assemble_script="$1"
    _assemble_out="$2"

    # shellcheck disable=SC2016
    sh_call_2 vibe-strap "$_assemble_out" "$_assemble_script" '
        . ./vibe-strap.sh
        . "$2"
    '
}

# test assembling source to binary,
#  and check the output of the binary.
_test() {
    # name our args
    _test_src="$1"
    _test_want="$2"
    
    # ensure tmp
    tmp="$(mktemp -d)"
    
    # compile to a.out
    _test_bin="${tmp}/a.out"
    _assemble "$_test_src" "$_test_bin"
    
    # run assembled output and capture its output
    _test_got="${tmp}/got.txt"
    "$_test_bin" > "$_test_got"

    # compare actual V.S. expected
    if ! cmp -s "$_test_want" "$_test_got"; then
        errln 'wanted:'
        cat "$_test_want" >&2
        errln 'got:'
        cat "$_test_got" >&2
        exit 1
    fi

    # ditch tmp
    cleanup
}

_test_dir() {
    _test_dir_dir="$1"
    _test_dir_base="$(basename "$_test_dir_dir")"
    _test_dir_src="${_test_dir_dir}/${_test_dir_base}.sh"
    _test_dir_stdout="${_test_dir_dir}/stdout.txt"

    if ! test -f "$_test_dir_src"; then
        errln "${name}: test source '${_test_dir_src}' not found"
        exit 1
    fi
    if ! test -f "$_test_dir_stdout"; then
        errln "${name}: test stdout '${_test_dir_stdout}' not found"
        exit 1
    fi

    _test "$_test_dir_src" "$_test_dir_stdout"
}

_test_all() {
    _test_all_seen=false

    for _test_all_dir in ./tests/*; do
        if ! test -d "$_test_all_dir"; then
            continue
        fi
        _test_all_seen=true
        _test_dir "$_test_all_dir"
    done

    if test "$_test_all_seen" = false; then
        errln "${name}: no tests found"
        exit 1
    fi
}

# actually test
_test_all
printf 'OK\n'
