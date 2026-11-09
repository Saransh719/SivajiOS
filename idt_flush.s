
[BITS 32]

global idt_flush

section .text

; Loads the Interrupt Descriptor Table
;
; Stack:
; [esp]     = Return address
; [esp + 4] = Address of IDT pointer

idt_flush:
    mov eax, [esp + 4]

    ; Load IDT into IDTR
    lidt [eax]

    ret
