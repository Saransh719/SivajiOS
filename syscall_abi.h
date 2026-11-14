#ifndef SYSCALL_ABI_H
#define SYSCALL_ABI_H

/* Shared by C and preprocessed NASM user programs. */
#define SYS_WRITE 1
#define SYS_EXIT 2

#define SYSCALL_ERR_E2BIG 7
#define SYSCALL_ERR_EFAULT 14
#define SYSCALL_ERR_NOSYS 38
#define SYSCALL_MAX_WRITE 4096

#endif
