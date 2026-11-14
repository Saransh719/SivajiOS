[BITS 32]

MAGIC_NUMBER  equ 0x1BADB002
ALIGN_MODULES equ 0x00000001
CHECKSUM      equ -(MAGIC_NUMBER + ALIGN_MODULES)

BOOT_IDENTITY_SIZE equ 0x04000000 ; GRUB metadata, modules, kernel, and low RAM
BOOT_HIGH_SIZE     equ 0x01000000 ; bootstrap alias for the kernel image
PAGE_SIZE     equ 4096

section .multiboot
align 4
    dd MAGIC_NUMBER
    dd ALIGN_MODULES
    dd CHECKSUM

section .boot.text
global loader
extern paging_enable
extern higher_half_entry

loader:
    cli
    mov [boot_magic], eax
    mov [boot_info], ebx
    mov esp, boot_stack_top

    ; Build initial low/high aliases and enable paging in physical bootstrap.
    call paging_enable

    ; This absolute target is a higher-half VMA. The identity alias keeps the
    ; current instruction stream and stack valid until this explicit jump.
    mov eax, higher_half_entry
    jmp eax

section .text
global higher_half_entry
extern kmain
extern gdt_install

higher_half_entry:
    ; Switch away from the temporary low stack before calling high-linked C.
    mov esp, high_kernel_stack_top
    and esp, 0xFFFFFFF0

    ; Reload the GDT after relocation. C and IDT handler addresses now carry
    ; their higher-half VMAs.
    call gdt_install

    push dword [boot_info]
    push dword [boot_magic]
    call kmain

.halt:
    cli
    hlt
    jmp .halt

section .boot.bss nobits
align PAGE_SIZE
global boot_page_directory
boot_page_directory:
    resd 1024

align PAGE_SIZE
global boot_identity_tables
boot_identity_tables:
    resd (BOOT_IDENTITY_SIZE / PAGE_SIZE)

align PAGE_SIZE
global boot_higher_tables
boot_higher_tables:
    resd (BOOT_HIGH_SIZE / PAGE_SIZE)

align 16
boot_stack:
    resb 4096
boot_stack_top:

boot_magic:
    resd 1
boot_info:
    resd 1

section .bss
align 16
high_kernel_stack:
    resb 16384
high_kernel_stack_top:
