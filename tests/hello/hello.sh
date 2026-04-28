# shellcheck shell=sh
. ./src/std/elf/begin.sh

. ./src/std/str/begin.sh
emit_raw 'Hello World!' #   "Hello World!"
emit_hex 0a             #   "\n"
. ./src/std/str/end.sh

# ./src/std/str/begin+end leaves:
#  - str addr in ecx
#  - str len in edx
emit_hex b8 04 00 00 00 #   mov eax, 4      ; syscall: write
emit_hex bb 01 00 00 00 #   mov ebx, 1      ; fd: stdout
emit_hex cd 80          #   int 0x80        ; write(stdout, ecx, edx)
emit_hex b8 01 00 00 00 #   mov eax, 1      ; syscall: exit
emit_hex 31 db          #   xor ebx, ebx    ; status: 0
emit_hex cd 80          #   int 0x80        ; exit(0)

. ./src/std/elf/end.sh
