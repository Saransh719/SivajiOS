[BITS 32]

start:
    ; Load a recognizable value into EAX
    mov eax, 0xDEADBEEF

    ; Stop execution here
    jmp $