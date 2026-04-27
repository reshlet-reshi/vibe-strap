# shellcheck shell=sh
# only meant to be used paired with end-str.sh

# const char str[] = "...";     // stored in ecx
# size_t str_sz = strlen(str);  // stored in edx (patched by end-str.sh)
emit_hex e8 00 00 00 00 #   call .+5            ; push init;
                        # init:
emit_hex 59             #   pop ecx             ; ecx = init
emit_hex 83 c1 0f       #   add ecx, 15         ; ecx = str;
emit_hex 89 ca          #   mov edx, ecx        ; edx = str;
                        # scan:
emit_hex 42             #   inc edx             ; ++edx;
emit_hex 80 3a 00       #   cmp byte [edx], 0   ; if (*edx != 0)
emit_hex 75 fa          #   jne scan            ;   goto scan;
emit_hex 42             #   inc edx             ; ++edx; // point AFTER \0
emit_hex ff e2          #   jmp edx             ; jump past str
                        # str:
                        #   ...
