# shellcheck shell=sh
inline_std elf/begin

inline_std str/begin
emit_raw 'Hello World!' #   "Hello World!"
emit_hex 0a             #   "\n"
inline_std str/end

# inline_std str/begin+end leaves:
#  - str addr in ecx
#  - str len in edx
emit_hex b8 04 00 00 00 #   mov eax, 4      ; syscall: write
emit_hex bb 01 00 00 00 #   mov ebx, 1      ; fd: stdout
emit_hex cd 80          #   int 0x80        ; write(stdout, ecx, edx)
emit_hex b8 01 00 00 00 #   mov eax, 1      ; syscall: exit
emit_hex 31 db          #   xor ebx, ebx    ; status: 0
emit_hex cd 80          #   int 0x80        ; exit(0)

inline_std elf/end
