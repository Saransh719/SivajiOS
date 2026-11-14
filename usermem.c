#include "usermem.h"
#include "mm/paging.h"

#define USER_LIMIT 0xC0000000u
#define PAGE_MASK  0xFFFFF000u

int validate_user_range(uint32_t address, uint32_t length, int write_access)
{
    if (!length) return address < USER_LIMIT;
    uint64_t end = (uint64_t)address + length;
    if (end <= address || end > 0x100000000ULL ||
        address >= USER_LIMIT || end > USER_LIMIT)
        return 0;

    uint32_t page = address & PAGE_MASK;
    uint32_t final_page = (uint32_t)(end - 1u) & PAGE_MASK;
    for (;;) {
        uint32_t flags;
        if (!paging_get_page_flags(page, &flags) ||
            !(flags & PAGING_USER) ||
            (write_access && !(flags & PAGING_WRITABLE)))
            return 0;
        if (page == final_page) break;
        page += 4096u;
    }
    return 1;
}

int copy_from_user(void *destination, uint32_t source, uint32_t length)
{
    if (!length) return 1;
    if (!destination || !validate_user_range(source, length, 0)) return 0;
    uint8_t *to = (uint8_t *)destination;
    const uint8_t *from = (const uint8_t *)source;
    for (uint32_t i = 0; i < length; i++) to[i] = from[i];
    return 1;
}

int copy_to_user(uint32_t destination, const void *source, uint32_t length)
{
    if (!length) return 1;
    if (!source || !validate_user_range(destination, length, 1)) return 0;
    uint8_t *to = (uint8_t *)destination;
    const uint8_t *from = (const uint8_t *)source;
    for (uint32_t i = 0; i < length; i++) to[i] = from[i];
    return 1;
}
