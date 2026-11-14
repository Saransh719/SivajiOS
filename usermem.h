#ifndef USERMEM_H
#define USERMEM_H

#include "multiboot.h"

/* User pointers must be below C0000000 and backed by effective user PTEs. */
int validate_user_range(uint32_t address, uint32_t length, int write_access);
int copy_from_user(void *kernel_destination, uint32_t user_source,
                   uint32_t length);
int copy_to_user(uint32_t user_destination, const void *kernel_source,
                 uint32_t length);

#endif
