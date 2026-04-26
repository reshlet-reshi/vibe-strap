#!/bin/sh
. ./emit

# ELF Header:
emit '\177ELF'          # e_ident[EI_MAG0..EI_MAG3]: ELF signature
emit '\001'             # e_ident[EI_CLASS]: ELFCLASS32
emit '\001'             # e_ident[EI_DATA]: ELFDATA2LSB
emit '\001'             # e_ident[EI_VERSION]: EV_CURRENT
emit '\000'             # e_ident[EI_OSABI]: ELFOSABI_SYSV
emit '\000'             # e_ident[EI_ABIVERSION]: 0
emit '\000\000\000\000' # e_ident[EI_PAD..]: padding
emit '\000\000\000'     # ...
emit '\002\000'         # e_type = 2; ET_EXEC, executable file
emit '\003\000'         # e_machine = 3; EM_386, Intel 80386
emit '\001\000\000\000' # e_version = 1; EV_CURRENT

# e_entry points at the first instruction:
#   base vaddr  0x08048000 (p_vaddr in the program header)
# + code offset       0x54 (52-byte ELF header + 32-byte program header)
# = entry vaddr 0x08048054
emit '\124\200\004\010'

# e_phoff = 52; e_shoff = 0; e_flags = 0.
emit '\064\000\000\000\000\000\000\000\000\000\000\000'
# e_ehsize = 52; e_phentsize = 32; e_phnum = 1; no section headers.
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
