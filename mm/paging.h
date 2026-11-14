#ifndef MM_PAGING_H
#define MM_PAGING_H

#include "../multiboot.h"

#define PAGING_PRESENT  0x001u
#define PAGING_WRITABLE 0x002u
#define PAGING_USER     0x004u
#define PAGING_FRAMEBUFFER_BASE 0xE0000000u

/* Flags are the x86 PTE permission bits: present, writable, and user. */
int paging_init(const struct multiboot_info *mbi);
int paging_map_page(uint32_t virtual_address, uint32_t physical_address,
                    uint32_t flags);
int paging_unmap_page(uint32_t virtual_address);
int paging_get_physical_address(uint32_t virtual_address,
                                uint32_t *physical_address);
void paging_flush_tlb(void);
uint32_t paging_virtual_to_physical(uint32_t virtual_address);
uint32_t paging_physical_to_kernel_virtual(uint32_t physical_address);
uint32_t paging_physical_to_framebuffer_virtual(uint32_t physical_address);
void paging_page_fault(uint32_t address, uint32_t error_code);

/* Manual, intentionally-fatal test. Call only from a disposable test boot. */
void paging_test_unmapped_fault(void);

#endif
