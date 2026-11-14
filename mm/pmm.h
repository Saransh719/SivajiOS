#ifndef MM_PMM_H
#define MM_PMM_H

#include "../multiboot.h"

/* Allocation failure sentinel. Physical frame zero is reserved by pmm_init. */
#define PMM_FRAME_FAILURE 0xFFFFFFFFu
#define PMM_FRAME_SIZE    4096u

/* Read-only physical memory accounting. `usable_ram_bytes` is the union of
 * Multiboot type-1 ranges minus any overlapping non-usable map ranges.
 * `usable_frames` counts whole allocatable frames from those ranges before
 * kernel/boot/module/framebuffer reservations; `reserved_frames` is all
 * managed frames currently unavailable to PMM allocation. */
typedef struct {
    uint64_t tracked_address_space_bytes;
    uint64_t usable_ram_bytes;
    uint32_t managed_frames;
    uint32_t usable_frames;
    uint32_t free_frames;
    uint32_t reserved_frames;
} pmm_stats_t;

void pmm_init(const struct multiboot_info *mbi);
uint32_t pmm_alloc_frame(void);
void pmm_free_frame(uint32_t physical_address);
uint32_t pmm_get_free_frame_count(void);
/* Frames below the highest physical end reported by the Multiboot map. */
uint32_t pmm_get_total_frame_count(void);
int pmm_is_initialized(void);
int pmm_get_stats(pmm_stats_t *stats);

/* Runs reversible allocator checks. Returns zero only if all checks pass. */
int pmm_run_self_tests(void);

#endif
