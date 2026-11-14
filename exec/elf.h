#ifndef EXEC_ELF_H
#define EXEC_ELF_H

#include "../multiboot.h"

/* Load a GRUB module as a static ELF32 i386 ET_EXEC, run it in Ring 3, then
 * release all user frames. Returns nonzero when SYS_EXIT was received. */
int elf32_run_module(uint32_t module_start, uint32_t module_size);

#endif
