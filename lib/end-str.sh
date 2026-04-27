# only meant to be used paired with/after begin-str.sh
# begin-str.sh leaves str addr in ecx

# trailing null
emit_hex 00             #   "\0"

# edx ends up pointing AFTER the '\0'. to get the str len,
# patch it to point AT the \0, then subtract the string addr
emit_hex 4a             #   dec edx             ; --edx; // point AT \0
emit_hex 29 ca          #   sub edx, ecx        ; edx = str_sz;
