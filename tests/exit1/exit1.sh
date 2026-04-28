# shellcheck shell=sh
inline_std elf/begin

emit_hex b8 01 00 00 00 #   mov eax, 1      ; syscall: exit
emit_hex bb 01 00 00 00 #   mov ebx, 1      ; status: 1
emit_hex cd 80          #   int 0x80        ; exit(1)

inline_std elf/end
