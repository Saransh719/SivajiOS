[BITS 32]

global enter_user_mode
global ring3_return_trampoline
global user_saved_kernel_esp
global user_saved_kernel_eflags

section .text
enter_user_mode:
    ; Preserve the callee-saved state and remember the suspended kernel stack.
    push ebp
    push ebx
    push esi
    push edi
    mov eax, [esp + 20]        ; entry argument
    mov edx, [esp + 24]        ; user stack argument
    mov [user_saved_kernel_esp], esp
    pushfd
    pop ecx
    mov [user_saved_kernel_eflags], ecx

    ; Load user data selectors before iret; CPL remains 0 until iret.
    mov cx, 0x23
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx

    push dword 0x23
    push edx
    and ecx, 0xFFFFCFFF       ; force IOPL=0
    or ecx, 0x00000200        ; IF=1 after IDT/PIC/TSS are installed
    push ecx
    push dword 0x1B
    push eax
    iret

ring3_return_trampoline:
    ; The syscall/fault iret entered this label at CPL 0 on the TSS stack.
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov esp, [user_saved_kernel_esp]
    pop edi
    pop esi
    pop ebx
    pop ebp
    push dword [user_saved_kernel_eflags]
    popfd
    ret

section .bss
align 4
user_saved_kernel_esp:
    resd 1
user_saved_kernel_eflags:
    resd 1
