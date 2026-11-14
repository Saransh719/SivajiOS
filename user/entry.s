[BITS 32]
global _start
extern user_main
section .text
_start:
    call user_main
    jmp $
