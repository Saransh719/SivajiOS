global loader
extern kmain
extern gdt_install

MAGIC_NUMBER  equ 0x1BADB002
ALIGN_MODULES equ 0x00000001

CHECKSUM      equ -(MAGIC_NUMBER + ALIGN_MODULES)

KERNEL_STACK_SIZE equ 4096

section .text
align 4

    dd MAGIC_NUMBER
    dd ALIGN_MODULES
    dd CHECKSUM

loader:
    mov esp, kernel_stack + KERNEL_STACK_SIZE

    ; Preserve Multiboot information
    push ebx
    push eax

    ; Initialize and load our GDT
    call gdt_install


    ; kmain(multiboot_magic, multiboot_info)
    push dword [esp + 4]
    push dword [esp + 4]
    call kmain

    add esp, 8


.loop:
    jmp .loop

section .bss
align 16

kernel_stack:
    resb KERNEL_STACK_SIZE
