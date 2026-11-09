bits 64

section .text
global efi_entry
extern efi_main

; The EFI image entry uses the Microsoft x64 calling convention. Reserve the
; required shadow space and keep the stack aligned before calling C.
efi_entry:
    sub rsp, 40
    call efi_main
.halt:
    cli
    hlt
    jmp .halt

section .note.GNU-stack noalloc noexec nowrite progbits
