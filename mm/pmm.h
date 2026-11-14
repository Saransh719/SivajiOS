#ifndef MM_PMM_H
#define MM_PMM_H

#include "../multiboot.h"

/* Allocation failure sentinel. Physical frame zero is reserved by pmm_init. */
#define PMM_FRAME_FAILURE 0xFFFFFFFFu
#define PMM_FRAME_SIZE    4096u

void pmm_init(const struct multiboot_info *mbi);
uint32_t pmm_alloc_frame(void);
void pmm_free_frame(uint32_t physical_address);
uint32_t pmm_get_free_frame_count(void);
/* Frames below the highest physical end reported by the Multiboot map. */
uint32_t pmm_get_total_frame_count(void);

/* Runs reversible allocator checks. Returns zero only if all checks pass. */
int pmm_run_self_tests(void);

#endif
