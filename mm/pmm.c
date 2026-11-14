#include "pmm.h"
#include "paging.h"
#include "../drivers/serial.h"

#define COM1 0x3F8u
#define MAX_FRAMES 1048576u /* 4 GiB / 4 KiB, limited by 32-bit physical API */
#define BITMAP_BYTES (MAX_FRAMES / 8u)
#define LOW_MEMORY_END 0x00100000u

/* A set used bit means reserved or allocated. The second bitmap distinguishes
 * PMM-owned allocations from permanent reservations so free cannot release
 * kernel/firmware/module memory or accept a double free. Both arrays live in
 * kernel BSS and are included in the linker-provided kernel bounds. */
static uint8_t frame_bitmap[BITMAP_BYTES] __attribute__((aligned(16)));
static uint8_t allocated_bitmap[BITMAP_BYTES] __attribute__((aligned(16)));

extern char _kernel_phys_start;
extern char _kernel_phys_end;

static uint32_t managed_frames;
static uint32_t free_frames;
static uint32_t search_hint;
static uint32_t detected_mib;
static uint32_t usable_mib;
static int memory_map_valid;

static int bit_is_set(const uint8_t *bitmap, uint32_t frame)
{
    return (bitmap[frame >> 3] & (uint8_t)(1u << (frame & 7u))) != 0;
}

static void bit_set(uint8_t *bitmap, uint32_t frame)
{
    bitmap[frame >> 3] |= (uint8_t)(1u << (frame & 7u));
}

static void bit_clear(uint8_t *bitmap, uint32_t frame)
{
    bitmap[frame >> 3] &= (uint8_t)~(1u << (frame & 7u));
}

static void serial_text(const char *text)
{
    unsigned int length = 0;
    while (text[length]) length++;
    serial_write(COM1, text, length);
}

static void serial_u32(uint32_t value)
{
    char digits[10];
    unsigned int count = 0;
    do {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value && count < sizeof(digits));
    while (count) serial_write(COM1, &digits[--count], 1);
}

static void serial_hex32(uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    char digits[8];
    for (unsigned int i = 0; i < 8; i++)
        digits[i] = hex[(value >> ((7u - i) * 4u)) & 0xFu];
    serial_write(COM1, digits, sizeof(digits));
}

/* Clamp a 64-bit physical interval to the address range supported by this
 * 32-bit physical-frame API. */
static int clamp_range(uint64_t base, uint64_t length,
                       uint64_t *clamped_base, uint64_t *clamped_end)
{
    const uint64_t address_limit = 0x100000000ULL;
    if (!length || base >= address_limit) return 0;
    uint64_t end = base + length;
    if (end < base || end > address_limit) end = address_limit;
    if (end <= base) return 0;
    *clamped_base = base;
    *clamped_end = end;
    return 1;
}

/* Only make fully contained pages available from a usable region. */
static void mark_usable_range(uint64_t base, uint64_t length)
{
    uint64_t start, end;
    if (!clamp_range(base, length, &start, &end)) return;
    uint64_t first = (start + PMM_FRAME_SIZE - 1u) / PMM_FRAME_SIZE;
    uint64_t limit = end / PMM_FRAME_SIZE;
    if (limit > managed_frames) limit = managed_frames;
    for (uint64_t frame = first; frame < limit; frame++) {
        uint32_t index = (uint32_t)frame;
        if (bit_is_set(frame_bitmap, index)) {
            bit_clear(frame_bitmap, index);
            free_frames++;
        }
    }
}

/* Reserved ranges claim every page they overlap. These are applied after
 * usable entries, so non-usable records take precedence if records overlap. */
static void reserve_range(uint64_t base, uint64_t length)
{
    uint64_t start, end;
    if (!clamp_range(base, length, &start, &end)) return;
    uint64_t first = start / PMM_FRAME_SIZE;
    uint64_t limit = (end + PMM_FRAME_SIZE - 1u) / PMM_FRAME_SIZE;
    if (limit > managed_frames) limit = managed_frames;
    for (uint64_t frame = first; frame < limit; frame++) {
        uint32_t index = (uint32_t)frame;
        if (!bit_is_set(frame_bitmap, index)) {
            bit_set(frame_bitmap, index);
            free_frames--;
        }
    }
}

/* Multiboot map records are variable length and may not be naturally aligned. */
static int next_mmap_entry(const struct multiboot_info *mbi,
                           uint32_t *offset,
                           const struct multiboot_mmap_entry **entry)
{
    if (*offset == mbi->mmap_length) return 0;
    if (*offset > mbi->mmap_length || mbi->mmap_length - *offset < 4u)
        return 0;
    uint64_t address = (uint64_t)mbi->mmap_addr + *offset;
    if (address + 4u > 0x100000000ULL) return -1;
    const uint8_t *bytes = (const uint8_t *)(uint32_t)address;
    const struct multiboot_mmap_entry *candidate =
        (const struct multiboot_mmap_entry *)bytes;
    uint32_t payload_size = candidate->size;
    if (payload_size < 20u || payload_size > mbi->mmap_length - *offset - 4u)
        return -1;
    *entry = candidate;
    *offset += payload_size + 4u;
    return 1;
}

static uint64_t update_max_end(uint64_t current, uint64_t base, uint64_t len)
{
    uint64_t start, end;
    if (!clamp_range(base, len, &start, &end)) return current;
    (void)start;
    return end > current ? end : current;
}

static void reserve_module_ranges(const struct multiboot_info *mbi)
{
    if (!(mbi->flags & MULTIBOOT_INFO_MODS) || !mbi->mods_count) return;

    reserve_range(mbi->mods_addr,
                  (uint64_t)mbi->mods_count * sizeof(struct multiboot_module));
    const struct multiboot_module *modules =
        (const struct multiboot_module *)mbi->mods_addr;
    for (uint32_t i = 0; i < mbi->mods_count; i++) {
        if (modules[i].mod_end > modules[i].mod_start)
            reserve_range(modules[i].mod_start,
                          modules[i].mod_end - modules[i].mod_start);
        if (modules[i].string) reserve_range(modules[i].string, 4096u);
    }
}

static void reserve_multiboot_memory(const struct multiboot_info *mbi)
{
    reserve_range((uint32_t)mbi, sizeof(*mbi));
    if ((mbi->flags & MULTIBOOT_INFO_CMDLINE) && mbi->cmdline)
        reserve_range(mbi->cmdline, 4096u);
    if (mbi->flags & MULTIBOOT_INFO_MMAP)
        reserve_range(mbi->mmap_addr, mbi->mmap_length);
    if (mbi->flags & MULTIBOOT_INFO_DRIVES)
        reserve_range(mbi->drives_addr, mbi->drives_length);
    if ((mbi->flags & MULTIBOOT_INFO_BOOT_LOADER) && mbi->boot_loader_name)
        reserve_range(mbi->boot_loader_name, 4096u);
    if (mbi->flags & MULTIBOOT_INFO_VBE) {
        if (mbi->vbe_control_info) reserve_range(mbi->vbe_control_info, 512u);
        if (mbi->vbe_mode_info) reserve_range(mbi->vbe_mode_info, 256u);
    }
    if (mbi->flags & MULTIBOOT_INFO_FRAMEBUFFER) {
        uint64_t size = (uint64_t)mbi->framebuffer_pitch *
                        mbi->framebuffer_height;
        reserve_range(mbi->framebuffer_addr, size);
    }
    reserve_module_ranges(mbi);
}

static void pmm_print_diagnostics(void)
{
    serial_text("PMM Multiboot physical-address span: ");
    serial_u32(detected_mib);
    serial_text(" MiB\nPMM usable memory in map: ");
    serial_u32(usable_mib);
    serial_text(" MiB\nPMM managed frames: ");
    serial_u32(managed_frames);
    serial_text("\nPMM free frames: ");
    serial_u32(free_frames);
    serial_text("\nPMM reserved frames: ");
    serial_u32(managed_frames - free_frames);
    serial_text("\nPMM bitmap physical=0x");
    serial_hex32(paging_virtual_to_physical((uint32_t)frame_bitmap));
    serial_text(" virtual=0x");
    serial_hex32((uint32_t)frame_bitmap);
    serial_text("\n");
    serial_text("PMM ownership bitmap physical=0x");
    serial_hex32(paging_virtual_to_physical((uint32_t)allocated_bitmap));
    serial_text(" virtual=0x");
    serial_hex32((uint32_t)allocated_bitmap);
    serial_text("\n");
}

void pmm_init(const struct multiboot_info *mbi)
{
    for (uint32_t i = 0; i < BITMAP_BYTES; i++) {
        frame_bitmap[i] = 0xFFu;
        allocated_bitmap[i] = 0;
    }
    managed_frames = 0;
    free_frames = 0;
    search_hint = 0;
    detected_mib = 0;
    usable_mib = 0;
    memory_map_valid = 0;

    if (!mbi || !(mbi->flags & MULTIBOOT_INFO_MMAP) || !mbi->mmap_length) {
        serial_text("PMM ERROR: Multiboot memory map unavailable; all frames reserved.\n");
        return;
    }
    if ((uint64_t)mbi->mmap_addr + mbi->mmap_length > 0x100000000ULL) {
        serial_text("PMM ERROR: Multiboot memory map address overflows; allocation disabled.\n");
        return;
    }

    uint64_t highest_end = 0;
    uint64_t usable_bytes = 0;
    uint32_t offset = 0;
    const struct multiboot_mmap_entry *entry;
    int result;
    while ((result = next_mmap_entry(mbi, &offset, &entry)) > 0) {
        highest_end = update_max_end(highest_end, entry->address, entry->length);
        if (entry->type == 1u) {
            uint64_t start, end;
            if (clamp_range(entry->address, entry->length, &start, &end)) {
                uint64_t length = end - start;
                if (usable_bytes > 0x100000000ULL - length)
                    usable_bytes = 0x100000000ULL;
                else
                    usable_bytes += length;
            }
        }
    }
    if (result < 0 || !highest_end) {
        serial_text("PMM ERROR: Invalid or empty Multiboot memory map; all frames reserved.\n");
        return;
    }

    uint64_t frame_limit =
        (highest_end + PMM_FRAME_SIZE - 1u) / PMM_FRAME_SIZE;
    if (frame_limit > MAX_FRAMES) frame_limit = MAX_FRAMES;
    managed_frames = (uint32_t)frame_limit;
    detected_mib = (uint32_t)(highest_end >> 20);
    usable_mib = (uint32_t)(usable_bytes >> 20);

    offset = 0;
    while ((result = next_mmap_entry(mbi, &offset, &entry)) > 0)
        if (entry->type == 1u)
            mark_usable_range(entry->address, entry->length);
    if (result < 0) {
        managed_frames = 0;
        free_frames = 0;
        serial_text("PMM ERROR: Malformed Multiboot map; allocation disabled.\n");
        return;
    }

    /* Apply non-usable records after usable records, then reserve occupied
     * kernel/GRUB/firmware regions explicitly. */
    offset = 0;
    while ((result = next_mmap_entry(mbi, &offset, &entry)) > 0)
        if (entry->type != 1u)
            reserve_range(entry->address, entry->length);
    if (result < 0) {
        managed_frames = 0;
        free_frames = 0;
        serial_text("PMM ERROR: Malformed Multiboot map; allocation disabled.\n");
        return;
    }

    reserve_range(0, LOW_MEMORY_END);
    uint32_t kernel_start = (uint32_t)&_kernel_phys_start;
    uint32_t kernel_end = (uint32_t)&_kernel_phys_end;
    if (kernel_end > kernel_start)
        reserve_range(kernel_start, (uint64_t)kernel_end - kernel_start);
    reserve_multiboot_memory(mbi);
    reserve_range(paging_virtual_to_physical((uint32_t)frame_bitmap),
                  sizeof(frame_bitmap));
    reserve_range(paging_virtual_to_physical((uint32_t)allocated_bitmap),
                  sizeof(allocated_bitmap));
    memory_map_valid = 1;
    pmm_print_diagnostics();
}

uint32_t pmm_alloc_frame(void)
{
    if (!memory_map_valid || !free_frames) return PMM_FRAME_FAILURE;

    for (uint32_t pass = 0; pass < 2; pass++) {
        uint32_t start = pass ? 0 : search_hint;
        uint32_t end = pass ? search_hint : managed_frames;
        for (uint32_t frame = start; frame < end; frame++) {
            if (!bit_is_set(frame_bitmap, frame)) {
                bit_set(frame_bitmap, frame);
                bit_set(allocated_bitmap, frame);
                free_frames--;
                search_hint = frame + 1u;
                return frame * PMM_FRAME_SIZE;
            }
        }
    }
    return PMM_FRAME_FAILURE;
}

void pmm_free_frame(uint32_t physical_address)
{
    if (!memory_map_valid ||
        (physical_address & (PMM_FRAME_SIZE - 1u)) != 0)
        return;
    uint32_t frame = physical_address / PMM_FRAME_SIZE;
    if (frame >= managed_frames || !bit_is_set(allocated_bitmap, frame))
        return;

    bit_clear(allocated_bitmap, frame);
    bit_clear(frame_bitmap, frame);
    free_frames++;
    if (frame < search_hint) search_hint = frame;
}

uint32_t pmm_get_free_frame_count(void)
{
    return free_frames;
}

uint32_t pmm_get_total_frame_count(void)
{
    return managed_frames;
}

static int test_failure(const char *message)
{
    serial_text("PMM TEST FAIL: ");
    serial_text(message);
    serial_text("\n");
    return 0;
}

static void release_test_frames(const uint32_t *frames, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) pmm_free_frame(frames[i]);
}

int pmm_run_self_tests(void)
{
    uint32_t initial_free = pmm_get_free_frame_count();
    uint32_t frames[3];
    uint32_t allocated_count = 0;
    if (!memory_map_valid || initial_free < 3u)
        return test_failure("allocator is invalid or has fewer than three free frames");

    while (allocated_count < 3u) {
        uint32_t frame = pmm_alloc_frame();
        if (frame == PMM_FRAME_FAILURE) {
            release_test_frames(frames, allocated_count);
            return test_failure("allocation unexpectedly exhausted");
        }
        if (frame & (PMM_FRAME_SIZE - 1u)) {
            pmm_free_frame(frame);
            release_test_frames(frames, allocated_count);
            return test_failure("allocated frame is not aligned");
        }
        for (uint32_t j = 0; j < allocated_count; j++)
            if (frame == frames[j]) {
                release_test_frames(frames, allocated_count);
                return test_failure("allocations were not unique");
            }
        frames[allocated_count++] = frame;
    }
    if (pmm_get_free_frame_count() != initial_free - 3u) {
        release_test_frames(frames, 3u);
        return test_failure("free count did not decrease by three");
    }

    uint32_t allocated = frames[0] / PMM_FRAME_SIZE;
    if (!bit_is_set(frame_bitmap, allocated) ||
        !bit_is_set(allocated_bitmap, allocated))
    {
        release_test_frames(frames, 3u);
        return test_failure("allocated bitmap bits were not set");
    }

    for (uint32_t i = 0; i < 3; i++) pmm_free_frame(frames[i]);
    if (pmm_get_free_frame_count() != initial_free)
        return test_failure("free count did not return to initial value");

    pmm_free_frame(frames[0]);
    pmm_free_frame(frames[1] + 1u);
    pmm_free_frame(0);
    if (pmm_get_free_frame_count() != initial_free)
        return test_failure("double/invalid free changed free count");

    uint32_t again = pmm_alloc_frame();
    if (again == PMM_FRAME_FAILURE)
        return test_failure("allocation failed after frees");
    pmm_free_frame(again);
    if (pmm_get_free_frame_count() != initial_free)
        return test_failure("final allocation was not returned");

    serial_text("PMM TEST PASS: alignment, uniqueness, unavailable state, free, double-free, invalid-free, reuse.\n");
    serial_text("PMM TEST NOTE: exhaustion test skipped to avoid consuming all frames.\n");
    return 1;
}
