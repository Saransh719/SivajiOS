[BITS 32]
#include "syscall_abi.h"

global user_test_start
global user_test_end
global user_protection_entry
global user_syscall_entry

section .userprog
user_test_start:
    ; Prove CPL 3, then access the separate writable user data page.
    xor eax, eax
    mov ax, cs
    and eax, 3
    cmp eax, 3
    jne .failed
    mov dword [0x40001000], 0xC0DEF00D
    cmp dword [0x40001000], 0xC0DEF00D
    jne .failed
    mov eax, SYS_EXIT
    xor ebx, ebx
    int 0x80
    jmp $
.failed:
    mov eax, SYS_EXIT
    mov ebx, 1
    int 0x80
    jmp $

user_protection_entry:
    xor eax, eax
    mov ax, cs
    and eax, 3
    cmp eax, 3
    jne .protection_failed
    mov dword [0x40001000], 0xC0DEF00D
    cmp dword [0x40001000], 0xC0DEF00D
    jne .protection_failed
    ; This kernel text page is mapped supervisor-only and must fault at CPL 3.
    mov eax, [0xC0118000]
    mov eax, SYS_EXIT
    mov ebx, 0
    int 0x80
    jmp $
.protection_failed:
    mov eax, SYS_EXIT
    mov ebx, 1
    int 0x80
    jmp $

user_syscall_entry:
    xor eax, eax
    mov ax, cs
    and eax, 3
    cmp eax, 3
    jne .syscall_failed

    ; sys_write from a valid user buffer, then verify its byte-count result.
    mov eax, SYS_WRITE
    mov ebx, 0x40000000 + (syscall_message - user_test_start)
    mov ecx, syscall_message_end - syscall_message
    xor edx, edx
    int 0x80
    cmp eax, syscall_message_end - syscall_message
    jne .syscall_failed
    or dword [0x40001008], 1

    ; A second valid buffer lives in the user-writable data page.
    mov dword [0x40001000], 0x4E495247 ; "GRIN"
    mov dword [0x40001004], 0x0A203345 ; "E3 \n"
    mov eax, SYS_WRITE
    mov ebx, 0x40001000
    mov ecx, 8
    xor edx, edx
    int 0x80
    cmp eax, 8
    jne .syscall_failed
    or dword [0x40001008], 2

    ; Unknown syscall must return -ENOSYS and leave user execution alive.
    mov eax, 0x7FFFFFFF
    int 0x80
    cmp eax, -SYSCALL_ERR_NOSYS
    jne .syscall_failed
    or dword [0x40001008], 4

    ; A mapped supervisor kernel address must be rejected by copy_from_user.
    mov eax, SYS_WRITE
    mov ebx, 0xC0118000
    mov ecx, 4
    xor edx, edx
    int 0x80
    cmp eax, -SYSCALL_ERR_EFAULT
    jne .syscall_failed
    or dword [0x40001008], 8

    ; This span starts in the mapped stack page but overflows into kernel VA.
    mov eax, SYS_WRITE
    mov ebx, 0xBFFFFFFC
    mov ecx, 8
    xor edx, edx
    int 0x80
    cmp eax, -SYSCALL_ERR_EFAULT
    jne .syscall_failed
    or dword [0x40001008], 16

    ; Reject an address below the user split that has no present mapping.
    mov eax, SYS_WRITE
    mov ebx, 0x40002000
    mov ecx, 1
    xor edx, edx
    int 0x80
    cmp eax, -SYSCALL_ERR_EFAULT
    jne .syscall_failed
    or dword [0x40001008], 32

    ; Pointer plus length would wrap a 32-bit address back into low memory.
    mov eax, SYS_WRITE
    mov ebx, 0xFFFFFFFC
    mov ecx, 8
    xor edx, edx
    int 0x80
    cmp eax, -SYSCALL_ERR_EFAULT
    jne .syscall_failed
    or dword [0x40001008], 64

    mov eax, SYS_EXIT
    xor ebx, ebx
    int 0x80
    jmp $
.syscall_failed:
    mov eax, SYS_EXIT
    mov ebx, 1
    int 0x80
    jmp $

syscall_message:
    db "Ring 3 sys_write says hello", 10
syscall_message_end:
user_test_end:
