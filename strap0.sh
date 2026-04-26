#!/bin/sh
. ./emit

# ELF Header:
emit_hex 7f             # e_ident[EI_MAG0..EI_MAG3]: ELF signature
emit_raw 'ELF'          #  ...
emit_hex 01             # e_ident[EI_CLASS]: ELFCLASS32
emit_hex 01             # e_ident[EI_DATA]: ELFDATA2LSB
emit_hex 01             # e_ident[EI_VERSION]: EV_CURRENT
emit_hex 00             # e_ident[EI_OSABI]: ELFOSABI_SYSV
emit_hex 00             # e_ident[EI_ABIVERSION]: 0
emit_hex 00 00 00 00    # e_ident[EI_PAD..]: padding
emit_hex 00 00 00       #  ...
emit_hex 03 00          # e_type = 3; ET_DYN, shared object / PIE
emit_hex 03 00          # e_machine = 3; EM_386, Intel 80386
emit_hex 01 00 00 00    # e_version = 1; EV_CURRENT
emit_hex 54 00 00 00    # e_entry = 0x54;
                        #  relative vaddr of first code byte.
                        #  skips 52-byte ELF header + 32-byte program header.
emit_hex 34 00 00 00    # e_phoff = 52; program header table offset.
emit_hex 00 00 00 00    # e_shoff = 0; no section header table.
emit_hex 00 00 00 00    # e_flags = 0.
emit_hex 34 00          # e_ehsize = 52; ELF header size.
emit_hex 20 00          # e_phentsize = 32; program header entry size.
emit_hex 01 00          # e_phnum = 1; one program header.
emit_hex 00 00          # e_shentsize = 0; no section headers.
emit_hex 00 00          # e_shnum = 0; no section headers.
emit_hex 00 00          # e_shstrndx = 0; no section name string table.

# Program header:
emit_hex 01 00 00 00    # p_type = 1; PT_LOAD, loadable segment.
emit_hex 00 00 00 00    # p_offset = 0; segment starts at file offset 0.
emit_hex 00 00 00 00    # p_vaddr = 0; relative virtual address.
emit_hex 00 00 00 00    # p_paddr = 0; ignored for this executable.
emit_hex 88 00 00 00    # p_filesz = 136; segment bytes in file.
emit_hex 88 00 00 00    # p_memsz = 136; segment bytes in memory.
emit_hex 05 00 00 00    # p_flags = 5; PF_R | PF_X.
emit_hex 00 10 00 00    # p_align = 0x1000; page alignment.

# Code at file offset 84 / relative vaddr 0x54.
emit_hex eb 1b          #   jmp msgref
                        # start:
emit_hex 59             #   pop ecx     ; msg
emit_hex b8 04 00 00 00 #   mov eax, 4  ; sys_write
emit_hex bb 01 00 00 00 #   mov ebx, 1  ; stdout
emit_hex ba 12 00 00 00 #   mov edx, 18
emit_hex cd 80          #   int 0x80
emit_hex b8 01 00 00 00 #   mov eax, 1  ; sys_exit
emit_hex 31 db          #   xor ebx, ebx
emit_hex cd 80          #   int 0x80
                        # msgref:
emit_hex e8 e0 ff ff ff #   call start

emit_raw 'vibe-strap stage0'
emit_hex 0a

chmod +x "$out"

printf 'wrote %s\n' "$out"
