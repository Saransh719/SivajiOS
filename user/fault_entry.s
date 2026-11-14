[BITS 32]
global _start
section .text
_start:
    mov eax, [0x40002000] ; Deliberately unmapped user page: must return via #PF.
    jmp $
