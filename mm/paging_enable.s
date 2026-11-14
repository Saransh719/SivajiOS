[BITS 32]

global paging_enable
extern boot_page_directory
extern boot_identity_tables
extern boot_higher_tables

BOOT_IDENTITY_SIZE equ 0x04000000 ; low identity map for boot data/modules
BOOT_HIGH_SIZE     equ 0x01000000 ; higher alias covers first 16 MiB physical
PAGE_SIZE     equ 4096

section .boot.text
paging_enable:
    ; Called once from the physical Multiboot entry. Fill 4 KiB PTEs for a
    ; 64 MiB identity window and a 16 MiB higher alias.
    mov edi, boot_page_directory
    xor eax, eax
    mov ecx, 1024
    rep stosd

    mov edi, boot_identity_tables
    xor ebx, ebx
    mov ecx, (BOOT_IDENTITY_SIZE / PAGE_SIZE)
.fill_identity:
    mov eax, ebx
    or eax, 0x003                 ; present, writable, supervisor
    mov [edi], eax
    add edi, 4
    add ebx, PAGE_SIZE
    loop .fill_identity

    mov edi, boot_higher_tables
    xor ebx, ebx
    mov ecx, (BOOT_HIGH_SIZE / PAGE_SIZE)
.fill_higher:
    mov eax, ebx
    or eax, 0x003
    mov [edi], eax
    add edi, 4
    add ebx, PAGE_SIZE
    loop .fill_higher

    mov eax, boot_identity_tables
    or eax, 0x003
    mov edi, boot_page_directory
    mov ecx, (BOOT_IDENTITY_SIZE / 0x00400000)
.identity_pdes:
    mov [edi], eax
    add edi, 4
    add eax, PAGE_SIZE
    loop .identity_pdes

    mov eax, boot_higher_tables
    or eax, 0x003
    mov edi, boot_page_directory + 768 * 4
    mov ecx, (BOOT_HIGH_SIZE / 0x00400000)
.higher_pdes:
    mov [edi], eax
    add edi, 4
    add eax, PAGE_SIZE
    loop .higher_pdes

    mov eax, boot_page_directory
    mov cr3, eax
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax
    ret
