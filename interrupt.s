
[BITS 32]

global interrupt_handler_0
global interrupt_handler_1
global interrupt_handler_2
global interrupt_handler_3
global interrupt_handler_4
global interrupt_handler_5
global interrupt_handler_6
global interrupt_handler_7
global interrupt_handler_8
global interrupt_handler_9
global interrupt_handler_10
global interrupt_handler_11
global interrupt_handler_12
global interrupt_handler_13
global interrupt_handler_14
global interrupt_handler_15
global interrupt_handler_16
global interrupt_handler_17
global interrupt_handler_18
global interrupt_handler_19
global interrupt_handler_20
global interrupt_handler_21
global interrupt_handler_22
global interrupt_handler_23
global interrupt_handler_24
global interrupt_handler_25
global interrupt_handler_26
global interrupt_handler_27
global interrupt_handler_28
global interrupt_handler_29
global interrupt_handler_30
global interrupt_handler_31

global interrupt_handler_32
global interrupt_handler_33
global interrupt_handler_34
global interrupt_handler_35
global interrupt_handler_36
global interrupt_handler_37
global interrupt_handler_38
global interrupt_handler_39
global interrupt_handler_40
global interrupt_handler_41
global interrupt_handler_42
global interrupt_handler_43
global interrupt_handler_44
global interrupt_handler_45
global interrupt_handler_46
global interrupt_handler_47

extern interrupt_handler

section .text

; Interrupts that do NOT receive an error code from the CPU
%macro no_error_code_interrupt_handler 1
interrupt_handler_%1:
    push dword 0
    push dword %1
    jmp common_interrupt_handler
%endmacro

; Interrupts that DO receive an error code from the CPU
%macro error_code_interrupt_handler 1
interrupt_handler_%1:
    push dword %1
    jmp common_interrupt_handler
%endmacro

; Common handler for all interrupts
common_interrupt_handler:

    ; Save general-purpose registers
    pusha

    ; Save the pointer to the saved registers
    mov eax, esp

    ; Call C handler:
    ; interrupt_handler(register_state, interrupt_number)
    push dword [eax + 32]
    push eax
    call interrupt_handler
    add esp, 8

    ; Restore registers
    popa

    ; Remove interrupt number and error code
    add esp, 8

    ; Return from interrupt
    iret


; CPU exceptions without error codes
no_error_code_interrupt_handler 0
no_error_code_interrupt_handler 1
no_error_code_interrupt_handler 2
no_error_code_interrupt_handler 3
no_error_code_interrupt_handler 4
no_error_code_interrupt_handler 5
no_error_code_interrupt_handler 6
no_error_code_interrupt_handler 7
no_error_code_interrupt_handler 9
no_error_code_interrupt_handler 15
no_error_code_interrupt_handler 16
no_error_code_interrupt_handler 18
no_error_code_interrupt_handler 19
no_error_code_interrupt_handler 20
no_error_code_interrupt_handler 21
no_error_code_interrupt_handler 22
no_error_code_interrupt_handler 23
no_error_code_interrupt_handler 24
no_error_code_interrupt_handler 25
no_error_code_interrupt_handler 26
no_error_code_interrupt_handler 27
no_error_code_interrupt_handler 28
no_error_code_interrupt_handler 29
no_error_code_interrupt_handler 30
no_error_code_interrupt_handler 31

; CPU exceptions with error codes
error_code_interrupt_handler 8
error_code_interrupt_handler 10
error_code_interrupt_handler 11
error_code_interrupt_handler 12
error_code_interrupt_handler 13
error_code_interrupt_handler 14
error_code_interrupt_handler 17

; Hardware IRQs (we will remap PIC to vectors 32-47)
no_error_code_interrupt_handler 32
no_error_code_interrupt_handler 33
no_error_code_interrupt_handler 34
no_error_code_interrupt_handler 35
no_error_code_interrupt_handler 36
no_error_code_interrupt_handler 37
no_error_code_interrupt_handler 38
no_error_code_interrupt_handler 39
no_error_code_interrupt_handler 40
no_error_code_interrupt_handler 41
no_error_code_interrupt_handler 42
no_error_code_interrupt_handler 43
no_error_code_interrupt_handler 44
no_error_code_interrupt_handler 45
no_error_code_interrupt_handler 46
no_error_code_interrupt_handler 47
