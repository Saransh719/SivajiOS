#include "syscall.h"
#include "usermem.h"
#include "drivers/serial.h"

#define COM1 0x3F8u
#define COPY_CHUNK 128u

static int sys_write(uint32_t user_buffer, uint32_t length)
{
    if (length > SYSCALL_MAX_WRITE) return -(int)SYSCALL_ERR_E2BIG;
    if (!length) return 0;
    if (!validate_user_range(user_buffer, length, 0))
        return -(int)SYSCALL_ERR_EFAULT;

    char buffer[COPY_CHUNK];
    uint32_t written = 0;
    while (written < length) {
        uint32_t amount = length - written;
        if (amount > sizeof(buffer)) amount = sizeof(buffer);
        if (!copy_from_user(buffer, user_buffer + written, amount))
            return written ? (int)written : -(int)SYSCALL_ERR_EFAULT;
        int result = serial_write(COM1, buffer, amount);
        if (result < 0) return written ? (int)written : -1;
        written += (uint32_t)result;
        if ((uint32_t)result != amount) break;
    }
    return (int)written;
}

/* Returns 1 for sys_exit, after writing its status to exit_status. */
int syscall_dispatch(struct cpu_state *state, uint32_t *exit_status)
{
    switch (state->eax) {
    case SYS_WRITE:
        state->eax = (uint32_t)sys_write(state->ebx, state->ecx);
        return 0;
    case SYS_EXIT:
        if (exit_status) *exit_status = state->ebx;
        return 1;
    default:
        state->eax = (uint32_t)-(int)SYSCALL_ERR_NOSYS;
        return 0;
    }
}
