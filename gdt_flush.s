
global gdt_flush

section .text

gdt_flush:
    ; Get the address of the GDT pointer
    mov eax, [esp + 4]

    ; Load the GDT
    lgdt [eax]

    ; Load the kernel data segment selector
    mov ax, 0x10

    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Reload CS using a far jump
    jmp 0x08:flush_cs

flush_cs:
    ret
