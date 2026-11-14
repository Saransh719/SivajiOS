#ifndef SHELL_SHELL_H
#define SHELL_SHELL_H

#include "../multiboot.h"

/* Runs the Ring 0 interactive shell; returns only if initialization fails. */
int shell_run(const struct multiboot_info *mbi, int paging_ready,
              int framebuffer_ready, int idt_ready, int tss_ready,
              int pmm_ready, int heap_ready, int keyboard_ready);

#endif
