#ifndef USERMODE_H
#define USERMODE_H

#include "interrupt.h"
#include "multiboot.h"

#define USERMODE_TEST_RING3       0
#define USERMODE_TEST_PROTECTION  1
#define USERMODE_TEST_SYSCALL     2

/* Called by the common interrupt dispatcher for user-origin syscalls/faults. */
int usermode_handle_interrupt(struct cpu_state *state, unsigned int vector,
                              uint32_t error, uint32_t fault_address);
int usermode_run_test(int test_kind);
/* Run an already mapped ELF entry and return after SYS_EXIT or a user fault. */
int usermode_run_loaded(uint32_t entry, uint32_t stack, uint32_t *exit_status);

#endif
