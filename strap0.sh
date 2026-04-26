#!/bin/sh
#
# strap0.sh - first host bootstrap sketch for vibe-strap.
#
# Contract:
#   - requires /bin/sh
#   - uses printf/redirection for byte emission
#   - accepts chmod +x as the one external Unix utility
#
# It emits a tiny 32-bit x86 Linux ELF executable that uses only int 0x80
# syscalls: write(1, msg, len), then exit(0).

set -eu

out=${1:-./vibe-stage0}

emit() {
    printf '%b' "$1" >> "$out"
}

: > "$out"

# ELF32 header, executable, i386, entry = 0x08048054.
emit '\177ELF\001\001\001\000\000\000\000\000\000\000\000\000'
emit '\002\000\003\000\001\000\000\000\124\200\004\010'
emit '\064\000\000\000\000\000\000\000\000\000\000\000'
emit '\064\000\040\000\001\000\000\000\000\000\000\000'

# Program header: one RX PT_LOAD segment containing the whole file.
# p_filesz/p_memsz = 133 bytes.
emit '\001\000\000\000\000\000\000\000\000\200\004\010\000\200\004\010'
emit '\205\000\000\000\205\000\000\000\005\000\000\000\000\020\000\000'

# Code at file offset 84 / vaddr 0x08048054.
#   mov eax, 4          ; sys_write
#   mov ebx, 1          ; stdout
#   mov ecx, msg
#   mov edx, 18
#   int 0x80
#   mov eax, 1          ; sys_exit
#   xor ebx, ebx
#   int 0x80
emit '\270\004\000\000\000\273\001\000\000\000\271\163\200\004\010'
emit '\272\022\000\000\000\315\200\270\001\000\000\000\061\333\315\200'

emit 'vibe-strap stage0\n'

chmod +x "$out"

printf 'wrote %s\n' "$out"
