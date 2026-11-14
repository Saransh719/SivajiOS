#ifndef SYSCALL_H
#define SYSCALL_H

#include "interrupt.h"
#include "multiboot.h"
#include "syscall_abi.h"

/*
 * int 0x80 ABI: EAX syscall number/result; EBX, ECX, EDX arguments 1..3.
 * Results are signed 32-bit values in EAX. SYS_EXIT is non-returning to user.
 * The NASM user test preprocesses the shared constants from syscall_abi.h.
 */
/* SYS_WRITE uses EBX=user buffer, ECX=byte count; output is COM1. SYS_EXIT
 * uses EBX=status and returns to suspended Ring 0. ABI errors return negative. */

/* Returns nonzero only when SYS_EXIT requests the safe Ring 0 trampoline. */
int syscall_dispatch(struct cpu_state *state, uint32_t *exit_status);

#endif
