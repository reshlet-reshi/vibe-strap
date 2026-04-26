#!/bin/sh
. ./emit.sh
. ./lib/header.sh

# const char pChzMsg[] = "vibe-strap\n";
# size_t msg_sz = strlen(pChzMsg);
emit_hex e8 00 00 00 00 #   call .+5            ; push init;
                        # init:
emit_hex 59             #   pop ecx             ; ecx = init
emit_hex 83 c1 0f       #   add ecx, 15         ; ecx = pChzMsg;
emit_hex 89 ca          #   mov edx, ecx        ; edx = pChzMsg;
                        # scan:
emit_hex 42             #   inc edx             ; ++edx;
emit_hex 80 3a 00       #   cmp byte [edx], 0   ; if (*edx != 0)
emit_hex 75 fa          #   jne scan            ;   goto scan;
emit_hex 42             #   inc edx             ; ++edx; // point AFTER \0
emit_hex ff e2          #   jmp edx             ; goto print;

# literal bytes of pChzMsg
emit_raw 'vibe-strap'   #   "vibe-strap"
emit_hex 0a             #   "\n"
emit_hex 00             #   "\0"

                        # print:
emit_hex 4a             #   dec edx             ; --edx; // point AT \0
emit_hex 29 ca          #   sub edx, ecx        ; edx = msg_sz;
emit_hex b8 04 00 00 00 #   mov eax, 4          ; syscall: write
emit_hex bb 01 00 00 00 #   mov ebx, 1          ; fd: stdout
emit_hex cd 80          #   int 0x80            ; write(stdout, ecx, edx)
emit_hex b8 01 00 00 00 #   mov eax, 1          ; syscall: exit
emit_hex 31 db          #   xor ebx, ebx        ; status: 0
emit_hex cd 80          #   int 0x80            ; exit(0)

patch_sizes
end_emit
