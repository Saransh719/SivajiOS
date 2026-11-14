#ifndef MM_HEAP_H
#define MM_HEAP_H

#include "../multiboot.h"

#ifndef OS_SIZE_T_DEFINED
typedef unsigned int size_t;
#define OS_SIZE_T_DEFINED
#endif

void heap_init(void);
/* kmalloc(0) returns NULL; invalid pointers passed to kfree are ignored. */
void *kmalloc(size_t size);
void kfree(void *ptr);
/* krealloc(NULL, n) allocates; krealloc(p, 0) frees p and returns NULL. */
void *krealloc(void *ptr, size_t new_size);

/* Runs the isolated allocator diagnostics and prints individual results. */
int heap_run_self_tests(void);

#endif
