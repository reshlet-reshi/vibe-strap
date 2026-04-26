#!/bin/sh
. ./emit

# ELF Header:
#  Magic:
#    7f 45 4c 46                    ELF signature
#    01                             ELFCLASS32
#    01                             ELFDATA2LSB
#    01                             EV_CURRENT
#    00                             ELFOSABI_SYSV
#    00                             ABI version
#    00 00 00 00 00 00 00           padding
#  Type:                              EXEC (Executable file)
#  Machine:                           Intel 80386
#  Version:                           0x1
#  Entry point address:               0x8048054
#  Start of program headers:          52 (bytes into file)
#  Start of section headers:          0 (bytes into file)
#  Flags:                             0x0
#  Size of this header:               52 (bytes)
#  Size of program headers:           32 (bytes)
#  Number of program headers:         1
#  Size of section headers:           0 (bytes)
#  Number of section headers:         0
#  Section header string table index: 0
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
