#!/bin/sh
. ./emit

# ELF Header:
emit_hex 7f 45 4c 46    # e_ident[EI_MAG0..EI_MAG3]: ELF signature
emit_hex 01             # e_ident[EI_CLASS]: ELFCLASS32
emit_hex 01             # e_ident[EI_DATA]: ELFDATA2LSB
emit_hex 01             # e_ident[EI_VERSION]: EV_CURRENT
emit_hex 00             # e_ident[EI_OSABI]: ELFOSABI_SYSV
emit_hex 00             # e_ident[EI_ABIVERSION]: 0
emit_hex 00 00 00 00    # e_ident[EI_PAD..]: padding
emit_hex 00 00 00       # ...
emit_hex 02 00          # e_type = 2; ET_EXEC, executable file
emit_hex 03 00          # e_machine = 3; EM_386, Intel 80386
emit_hex 01 00 00 00    # e_version = 1; EV_CURRENT

# e_entry points at the first instruction:
#   base vaddr  0x08048000 (p_vaddr in the program header)
# + code offset       0x54 (52-byte ELF header + 32-byte program header)
# = entry vaddr 0x08048054
emit_hex 54 80 04 08

# e_phoff = 52; e_shoff = 0; e_flags = 0.
emit_hex 34 00 00 00 00 00 00 00 00 00 00 00
# e_ehsize = 52; e_phentsize = 32; e_phnum = 1; no section headers.
emit_hex 34 00 20 00 01 00 00 00 00 00 00 00

# Program header: one RX PT_LOAD segment containing the whole file.
# p_filesz/p_memsz = 133 bytes.
emit_hex 01 00 00 00 00 00 00 00 00 80 04 08 00 80 04 08
emit_hex 85 00 00 00 85 00 00 00 05 00 00 00 00 10 00 00

# Code at file offset 84 / vaddr 0x08048054.
#   mov eax, 4          ; sys_write
#   mov ebx, 1          ; stdout
#   mov ecx, msg
#   mov edx, 18
#   int 0x80
#   mov eax, 1          ; sys_exit
#   xor ebx, ebx
#   int 0x80
emit_hex b8 04 00 00 00 bb 01 00 00 00 b9 73 80 04 08
emit_hex ba 12 00 00 00 cd 80 b8 01 00 00 00 31 db cd 80

emit_raw 'vibe-strap stage0'
emit_hex 0a

chmod +x "$out"

printf 'wrote %s\n' "$out"
