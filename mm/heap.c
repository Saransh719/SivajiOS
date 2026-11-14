#include "heap.h"
#include "paging.h"
#include "pmm.h"
#include "../drivers/serial.h"

#define COM1 0x3F8u
#define HEAP_START 0xD0000000u
#define HEAP_END   0xD1000000u
#define HEAP_BYTES (HEAP_END - HEAP_START)
#define PAGE_SIZE  4096u
#define ALIGNMENT  16u
#define BLOCK_MAGIC 0x48454150u
#define DEAD_MAGIC  0xDEADFEEEu
#define MIN_SPLIT_SIZE ALIGNMENT
#define MAX_BLOCKS (HEAP_BYTES / sizeof(struct heap_block) + 1u)

struct heap_block {
    uint32_t magic;
    uint32_t size;
    uint32_t is_free;
    struct heap_block *previous;
    struct heap_block *next;
} __attribute__((aligned(ALIGNMENT)));

#define BLOCK_HEADER_SIZE ((uint32_t)sizeof(struct heap_block))

static struct heap_block *block_head;
static uint32_t mapped_end;
static int heap_initialized;
/* This kernel is currently single-threaded. Add an allocator lock around
 * public operations before introducing preemptive tasks or heap-using IRQs. */
static uint32_t rollback_test_frames[8192];

static void serial_text(const char *text)
{
    unsigned int length = 0;
    while (text[length]) length++;
    serial_write(COM1, text, length);
}

static void heap_error(const char *text)
{
    serial_text("HEAP ERROR: ");
    serial_text(text);
    serial_text("\n");
}

static int align_size(size_t requested, uint32_t *aligned)
{
    if (requested > HEAP_BYTES - BLOCK_HEADER_SIZE - (ALIGNMENT - 1u))
        return 0;
    *aligned = ((uint32_t)requested + ALIGNMENT - 1u) & ~(ALIGNMENT - 1u);
    return *aligned != 0;
}

static int block_valid(const struct heap_block *block)
{
    uint32_t address = (uint32_t)block;
    if (address < HEAP_START || address > mapped_end ||
        mapped_end - address < BLOCK_HEADER_SIZE)
        return 0;
    if (block->magic != BLOCK_MAGIC || (block->size & (ALIGNMENT - 1u)))
        return 0;
    if (block->size > mapped_end - address - BLOCK_HEADER_SIZE)
        return 0;
    uint32_t block_end = address + BLOCK_HEADER_SIZE + block->size;
    if (block->next) {
        if ((uint32_t)block->next != block_end || block->next <= block)
            return 0;
    } else if (block_end != mapped_end) {
        return 0;
    }
    return 1;
}

/* Walk only known in-heap metadata; never derive and dereference a header
 * directly from an untrusted pointer supplied to kfree/krealloc. */
static struct heap_block *find_block(const void *pointer)
{
    if (!heap_initialized || !pointer) return 0;
    struct heap_block *block = block_head;
    struct heap_block *previous = 0;
    for (uint32_t count = 0; block && count < MAX_BLOCKS; count++) {
        if (!block_valid(block) || block->previous != previous) return 0;
        if ((const void *)((uint8_t *)block + BLOCK_HEADER_SIZE) == pointer)
            return block;
        previous = block;
        block = block->next;
    }
    return 0;
}

static struct heap_block *validated_tail(void)
{
    struct heap_block *block = block_head;
    struct heap_block *previous = 0;
    for (uint32_t count = 0; block && count < MAX_BLOCKS; count++) {
        if (!block_valid(block) || block->previous != previous) return 0;
        if (!block->next) return block;
        previous = block;
        block = block->next;
    }
    return (struct heap_block *)0;
}

static void rollback_mappings(uint32_t start, uint32_t page_count)
{
    while (page_count) {
        uint32_t virtual_address = start + (--page_count * PAGE_SIZE);
        uint32_t physical_address;
        if (paging_get_physical_address(virtual_address, &physical_address) &&
            paging_unmap_page(virtual_address))
            pmm_free_frame(physical_address & 0xFFFFF000u);
    }
}

/* Map enough fresh PMM frames to extend the contiguous heap virtual range.
 * On any failure, all mappings and frames acquired by this extension unwind. */
static int grow_heap(uint32_t minimum_bytes)
{
    if (!heap_initialized || mapped_end > HEAP_END) return 0;
    uint32_t growth = minimum_bytes > PAGE_SIZE ? minimum_bytes : PAGE_SIZE;
    if (growth > HEAP_END - mapped_end) return 0;
    uint32_t page_count = (growth + PAGE_SIZE - 1u) / PAGE_SIZE;
    if (page_count > (HEAP_END - mapped_end) / PAGE_SIZE) return 0;
    uint32_t old_end = mapped_end;
    uint32_t mapped_count = 0;

    while (mapped_count < page_count) {
        uint32_t virtual_address = old_end + mapped_count * PAGE_SIZE;
        uint32_t physical_address = pmm_alloc_frame();
        if (physical_address == PMM_FRAME_FAILURE) {
            rollback_mappings(old_end, mapped_count);
            return 0;
        }
        if (!paging_map_page(virtual_address, physical_address,
                             PAGING_PRESENT | PAGING_WRITABLE)) {
            pmm_free_frame(physical_address);
            rollback_mappings(old_end, mapped_count);
            return 0;
        }
        mapped_count++;
    }

    uint32_t new_end = old_end + page_count * PAGE_SIZE;
    struct heap_block *tail = validated_tail();
    if (block_head && !tail) {
        rollback_mappings(old_end, mapped_count);
        return 0;
    }

    if (!block_head) {
        if (new_end - HEAP_START <= BLOCK_HEADER_SIZE) {
            rollback_mappings(old_end, mapped_count);
            return 0;
        }
        block_head = (struct heap_block *)HEAP_START;
        block_head->magic = BLOCK_MAGIC;
        block_head->size = new_end - HEAP_START - BLOCK_HEADER_SIZE;
        block_head->is_free = 1;
        block_head->previous = 0;
        block_head->next = 0;
    } else if (tail->is_free) {
        tail->size += new_end - old_end;
    } else {
        if (new_end - old_end <= BLOCK_HEADER_SIZE) {
            rollback_mappings(old_end, mapped_count);
            return 0;
        }
        struct heap_block *new_block = (struct heap_block *)old_end;
        new_block->magic = BLOCK_MAGIC;
        new_block->size = new_end - old_end - BLOCK_HEADER_SIZE;
        new_block->is_free = 1;
        new_block->previous = tail;
        new_block->next = 0;
        tail->next = new_block;
    }
    mapped_end = new_end;
    return 1;
}

static void split_block(struct heap_block *block, uint32_t requested)
{
    if (block->size < requested + BLOCK_HEADER_SIZE + MIN_SPLIT_SIZE) return;
    uint32_t remaining = block->size - requested - BLOCK_HEADER_SIZE;
    struct heap_block *split = (struct heap_block *)
        ((uint8_t *)block + BLOCK_HEADER_SIZE + requested);
    split->magic = BLOCK_MAGIC;
    split->size = remaining;
    split->is_free = 1;
    split->previous = block;
    split->next = block->next;
    if (split->next) split->next->previous = split;
    block->next = split;
    block->size = requested;
}

static int merge_next(struct heap_block *block)
{
    struct heap_block *next = block->next;
    if (!next || !block_valid(block) || !block_valid(next) ||
        !next->is_free || next->previous != block)
        return 0;
    block->size += BLOCK_HEADER_SIZE + next->size;
    block->next = next->next;
    if (block->next) block->next->previous = block;
    next->magic = DEAD_MAGIC;
    return 1;
}

static uint32_t free_payload_bytes(void)
{
    uint32_t total = 0;
    struct heap_block *block = block_head;
    struct heap_block *previous = 0;
    for (uint32_t count = 0; block && count < MAX_BLOCKS; count++) {
        if (!block_valid(block) || block->previous != previous) return 0xFFFFFFFFu;
        if (block->is_free) total += block->size;
        previous = block;
        block = block->next;
    }
    return block ? 0xFFFFFFFFu : total;
}

void heap_init(void)
{
    if (heap_initialized) return;
    if (HEAP_START >= HEAP_END ||
        (HEAP_START & (PAGE_SIZE - 1u)) ||
        (HEAP_END & (PAGE_SIZE - 1u))) {
        heap_error("invalid virtual range");
        return;
    }

    /* Refuse to claim any range already owned by the kernel or another map. */
    for (uint32_t address = HEAP_START; address < HEAP_END;
         address += PAGE_SIZE) {
        if (paging_get_physical_address(address, 0)) {
            heap_error("virtual range overlaps an existing mapping");
            return;
        }
    }
    block_head = 0;
    mapped_end = HEAP_START;
    heap_initialized = 1;
}

int heap_is_initialized(void) { return heap_initialized; }

int heap_get_stats(struct heap_stats *stats)
{
    if (!stats || !heap_initialized) return 0;
    uint32_t free_bytes = 0, allocated = 0, blocks = 0;
    struct heap_block *block = block_head, *previous = 0;
    while (block && blocks < MAX_BLOCKS) {
        if (!block_valid(block) || block->previous != previous) return 0;
        if (block->is_free) free_bytes += block->size;
        else allocated += block->size;
        previous = block;
        block = block->next;
        blocks++;
    }
    if (block) return 0;
    stats->capacity_bytes = HEAP_BYTES;
    stats->mapped_bytes = mapped_end - HEAP_START;
    stats->allocated_payload_bytes = allocated;
    stats->free_payload_bytes = free_bytes;
    stats->unmapped_bytes = HEAP_END - mapped_end;
    stats->block_count = blocks;
    return 1;
}

void *kmalloc(size_t size)
{
    if (!heap_initialized || !size) return 0;
    uint32_t aligned_size;
    if (!align_size(size, &aligned_size)) return 0;

    for (uint32_t attempts = 0; attempts < 2; attempts++) {
        struct heap_block *block = block_head;
        struct heap_block *previous = 0;
        uint32_t count = 0;
        while (block && count++ < MAX_BLOCKS) {
            if (!block_valid(block) || block->previous != previous) return 0;
            if (block->is_free && block->size >= aligned_size) {
                split_block(block, aligned_size);
                block->is_free = 0;
                return (uint8_t *)block + BLOCK_HEADER_SIZE;
            }
            previous = block;
            block = block->next;
        }
        if (block || !grow_heap(aligned_size + BLOCK_HEADER_SIZE)) return 0;
    }
    return 0;
}

void kfree(void *pointer)
{
    struct heap_block *block = find_block(pointer);
    if (!block || block->is_free) return;
    block->is_free = 1;

    if (block->previous && block->previous->is_free &&
        merge_next(block->previous))
        block = block->previous;
    while (block->next && block->next->is_free)
        if (!merge_next(block)) break;
}

void *krealloc(void *pointer, size_t new_size)
{
    if (!pointer) return new_size ? kmalloc(new_size) : 0;
    if (!new_size) {
        kfree(pointer);
        return 0;
    }

    struct heap_block *block = find_block(pointer);
    if (!block || block->is_free) return 0;
    uint32_t aligned_size;
    if (!align_size(new_size, &aligned_size)) return 0;

    if (aligned_size <= block->size) {
        split_block(block, aligned_size);
        if (block->next && block->next->is_free) (void)merge_next(block);
        return pointer;
    }

    uint32_t expanded_size = block->size;
    struct heap_block *next = block->next;
    if (next && next->is_free && block_valid(block) && block_valid(next) &&
        next->previous == block)
        expanded_size += BLOCK_HEADER_SIZE + next->size;
    if (expanded_size >= aligned_size) {
        if (expanded_size != block->size) (void)merge_next(block);
        split_block(block, aligned_size);
        return pointer;
    }

    void *replacement = kmalloc(new_size);
    if (!replacement) return 0;
    uint32_t copy_size = block->size < new_size ? block->size : new_size;
    uint8_t *source = (uint8_t *)pointer;
    uint8_t *destination = (uint8_t *)replacement;
    for (uint32_t i = 0; i < copy_size; i++) destination[i] = source[i];
    kfree(pointer);
    return replacement;
}

static int test_result(const char *name, int passed)
{
    serial_text("HEAP TEST ");
    serial_text(passed ? "PASS: " : "FAIL: ");
    serial_text(name);
    serial_text("\n");
    return passed;
}

int heap_run_self_tests(void)
{
    int passed = 1;
    if (!heap_initialized) {
        heap_error("test requested before heap_init");
        return 0;
    }

    uint8_t *small = (uint8_t *)kmalloc(24u);
    passed &= test_result("small allocation", small != 0);
    passed &= test_result("16-byte alignment",
                          small && (((uint32_t)small & (ALIGNMENT - 1u)) == 0));
    if (small) {
        for (uint32_t i = 0; i < 24u; i++) small[i] = (uint8_t)(i ^ 0x5Au);
        int contents_ok = 1;
        for (uint32_t i = 0; i < 24u; i++)
            if (small[i] != (uint8_t)(i ^ 0x5Au)) contents_ok = 0;
        passed &= test_result("small allocation read/write", contents_ok);
    }

    struct heap_block *small_block = find_block(small);
    int split_ok = small_block && small_block->next &&
        small_block->next->is_free && small_block->size == 32u;
    passed &= test_result("free block split", split_ok);

    void *small_address = small;
    kfree(small);
    void *reused = kmalloc(24u);
    passed &= test_result("freed block reuse", reused == small_address);
    kfree(reused);

    uint32_t large_size = 70u * 1024u;
    uint8_t *large = (uint8_t *)kmalloc(large_size);
    int large_ok = large != 0 &&
        (((uint32_t)large & (ALIGNMENT - 1u)) == 0);
    if (large) {
        for (uint32_t i = 0; i < large_size; i += PAGE_SIZE)
            large[i] = (uint8_t)((i / PAGE_SIZE) + 0x31u);
        large[large_size - 1u] = 0xA7u;
        for (uint32_t i = 0; i < large_size; i += PAGE_SIZE)
            if (large[i] != (uint8_t)((i / PAGE_SIZE) + 0x31u))
                large_ok = 0;
        if (large[large_size - 1u] != 0xA7u) large_ok = 0;
    }
    passed &= test_result("large multi-page allocation and access", large_ok);
    kfree(large);

    void *a = kmalloc(64u);
    void *b = kmalloc(64u);
    void *c = kmalloc(64u);
    int adjacent = a && b && c &&
        (uint32_t)b - (uint32_t)a == BLOCK_HEADER_SIZE + 64u &&
        (uint32_t)c - (uint32_t)b == BLOCK_HEADER_SIZE + 64u;
    if (a && b && c) {
        kfree(a);
        kfree(b);
        void *combined = kmalloc(128u);
        passed &= test_result("adjacent free blocks coalesce",
                              adjacent && combined == a);
        kfree(combined);
        kfree(c);
    } else {
        passed &= test_result("adjacent free blocks coalesce", 0);
        kfree(a);
        kfree(b);
        kfree(c);
    }

    uint8_t *resizing = (uint8_t *)kmalloc(32u);
    uint8_t *blocker = (uint8_t *)kmalloc(32u);
    int realloc_ok = resizing && blocker;
    if (realloc_ok) {
        for (uint32_t i = 0; i < 32u; i++) resizing[i] = (uint8_t)(0xC0u + i);
        uint8_t *grown = (uint8_t *)krealloc(resizing, 128u);
        realloc_ok = grown != 0 && grown != resizing;
        if (grown) {
            for (uint32_t i = 0; i < 32u; i++)
                if (grown[i] != (uint8_t)(0xC0u + i)) realloc_ok = 0;
            uint8_t *shrunk = (uint8_t *)krealloc(grown, 16u);
            if (shrunk != grown) realloc_ok = 0;
            kfree(shrunk);
        } else {
            kfree(resizing);
        }
    }
    kfree(blocker);
    passed &= test_result("realloc growth, preserved data, and shrink", realloc_ok);

    uint32_t before_invalid = free_payload_bytes();
    kfree((void *)0x12345678u);
    kfree((void *)(HEAP_START + 1u));
    passed &= test_result("invalid pointer ignored",
                          free_payload_bytes() == before_invalid);

    void *once = kmalloc(48u);
    kfree(once);
    uint32_t before_double = free_payload_bytes();
    kfree(once);
    passed &= test_result("double free ignored",
                          once && free_payload_bytes() == before_double);

    uint32_t before_failure = free_payload_bytes();
    uint32_t end_before_failure = mapped_end;
    void *too_large = kmalloc(HEAP_BYTES);
    passed &= test_result("oversized allocation fails without heap mutation",
                          !too_large && free_payload_bytes() == before_failure &&
                          mapped_end == end_before_failure);

    uint32_t available_before_drain = pmm_get_free_frame_count();
    if (available_before_drain <= sizeof(rollback_test_frames) /
                                  sizeof(rollback_test_frames[0])) {
        uint32_t drained = 0;
        int drained_ok = 1;
        while (pmm_get_free_frame_count() > 2u) {
            uint32_t frame = pmm_alloc_frame();
            if (frame == PMM_FRAME_FAILURE) {
                drained_ok = 0;
                break;
            }
            rollback_test_frames[drained++] = frame;
        }

        uint32_t free_before_rollback = pmm_get_free_frame_count();
        uint32_t payload_before_rollback = free_payload_bytes();
        uint32_t end_before_rollback = mapped_end;
        uint32_t request = HEAP_END - mapped_end - PAGE_SIZE -
                           BLOCK_HEADER_SIZE;
        request &= ~(ALIGNMENT - 1u);
        void *failed_growth = drained_ok ? kmalloc(request) : (void *)0;
        int rollback_ok = drained_ok && free_before_rollback == 2u &&
            !failed_growth &&
            pmm_get_free_frame_count() == free_before_rollback &&
            free_payload_bytes() == payload_before_rollback &&
            mapped_end == end_before_rollback;
        while (drained) pmm_free_frame(rollback_test_frames[--drained]);
        rollback_ok = rollback_ok &&
            pmm_get_free_frame_count() == available_before_drain;
        passed &= test_result("PMM exhaustion rolls back partial heap growth",
                              rollback_ok);
    } else {
        serial_text("HEAP TEST SKIP: PMM rollback exhaustion test exceeds test buffer.\n");
    }

    serial_text(passed ? "HEAP TEST SUMMARY: PASS\n" : "HEAP TEST SUMMARY: FAIL\n");
    return passed;
}
