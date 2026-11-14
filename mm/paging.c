#include "paging.h"
#include "../drivers/serial.h"

#define PAGE_SIZE 4096u
#define PAGE_MASK 0xFFFFF000u
#define PAGE_ENTRIES 1024u
#define COM1 0x3F8
#define KERNEL_BASE 0xC0000000u

/* Temporary boot-time pool, not a physical page allocator. At most 4 GiB of
 * 32-bit virtual space can be represented; only tables needed by mapped
 * ranges are consumed. A real PMM should replace this fixed 4 MiB pool. */
static uint32_t page_table_pool[PAGE_ENTRIES][PAGE_ENTRIES]
    __attribute__((aligned(PAGE_SIZE)));
static uint32_t page_table_count;
static int paging_active;

extern char _kernel_start;
extern char _kernel_end;
extern uint32_t boot_page_directory[PAGE_ENTRIES];

static uint32_t *const page_directory = boot_page_directory;

/* Linker VMAs at or above C0000000 are backed at VMA - KERNEL_BASE. Low
 * bootstrap objects and Multiboot-provided physical pointers stay unchanged. */
uint32_t paging_virtual_to_physical(uint32_t address)
{
    return address >= KERNEL_BASE ? address - KERNEL_BASE : address;
}

uint32_t paging_physical_to_kernel_virtual(uint32_t address)
{
    return address + KERNEL_BASE;
}

uint32_t paging_physical_to_framebuffer_virtual(uint32_t address)
{
    return PAGING_FRAMEBUFFER_BASE + (address & (PAGE_SIZE - 1u));
}

static void serial_string(const char *s)
{
    unsigned int n = 0;
    while (s[n]) n++;
    serial_write(COM1, s, n);
}

static void serial_hex(uint32_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    char text[8];
    for (unsigned int i = 0; i < 8; i++)
        text[i] = digits[(value >> ((7u - i) * 4u)) & 15u];
    serial_write(COM1, text, 8);
}

static uint32_t *get_table(uint32_t directory_index, int create)
{
    uint32_t entry = page_directory[directory_index];
    if (entry & PAGING_PRESENT)
        return (uint32_t *)(entry & PAGE_MASK);
    if (!create || page_table_count == PAGE_ENTRIES)
        return 0;

    uint32_t *table = page_table_pool[page_table_count++];
    for (uint32_t i = 0; i < PAGE_ENTRIES; i++) table[i] = 0;
    page_directory[directory_index] =
        paging_virtual_to_physical((uint32_t)table) |
        PAGING_PRESENT | PAGING_WRITABLE;
    return table;
}

int paging_map_page(uint32_t va, uint32_t pa, uint32_t flags)
{
    if ((va & (PAGE_SIZE - 1u)) || (pa & (PAGE_SIZE - 1u))) return 0;
    uint32_t *table = get_table(va >> 22, 1);
    if (!table) return 0;
    uint32_t *entry = &table[(va >> 12) & 0x3FFu];
    if (*entry & PAGING_PRESENT) return 0;
    *entry = (pa & PAGE_MASK) | (flags & 7u) | PAGING_PRESENT;
    if (flags & PAGING_USER)
        page_directory[va >> 22] |= PAGING_USER;
    if (paging_active) __asm__ volatile ("invlpg (%0)" :: "r"(va) : "memory");
    return 1;
}

int paging_unmap_page(uint32_t va)
{
    uint32_t *table = get_table(va >> 22, 0);
    if (!table) return 0;
    uint32_t *entry = &table[(va >> 12) & 0x3FFu];
    if (!(*entry & PAGING_PRESENT)) return 0;
    *entry = 0;
    if (paging_active) __asm__ volatile ("invlpg (%0)" :: "r"(va) : "memory");
    return 1;
}

int paging_get_physical_address(uint32_t va, uint32_t *pa)
{
    uint32_t *table = get_table(va >> 22, 0);
    if (!table) return 0;
    uint32_t entry = table[(va >> 12) & 0x3FFu];
    if (!(entry & PAGING_PRESENT)) return 0;
    if (pa) *pa = (entry & PAGE_MASK) | (va & 0xFFFu);
    return 1;
}

void paging_flush_tlb(void)
{
    uint32_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile ("mov %0, %%cr3" :: "r"(cr3) : "memory");
}

static int map_range(uint64_t start, uint64_t length)
{
    if (!length) return 1;
    uint64_t end = start + length;
    if (end < start || end > 0x100000000ULL) return 0;
    uint64_t page = start & ~(uint64_t)(PAGE_SIZE - 1u);
    uint64_t limit = (end + PAGE_SIZE - 1u) & ~(uint64_t)(PAGE_SIZE - 1u);
    while (page < limit) {
        uint32_t existing;
        if (paging_get_physical_address((uint32_t)page, &existing)) {
            if ((existing & PAGE_MASK) != (uint32_t)page) return 0;
        } else if (!paging_map_page((uint32_t)page, (uint32_t)page,
                                    PAGING_WRITABLE)) {
            return 0;
        }
        page += PAGE_SIZE;
    }
    return 1;
}

static int map_kernel_range(uint32_t physical_start, uint32_t length)
{
    uint64_t end = (uint64_t)physical_start + length;
    if (end > 0x100000000ULL) return 0;
    uint64_t page = (uint64_t)physical_start & ~(uint64_t)(PAGE_SIZE - 1u);
    uint64_t limit = (end + PAGE_SIZE - 1u) & ~(uint64_t)(PAGE_SIZE - 1u);
    while (page < limit) {
        uint32_t physical = (uint32_t)page;
        uint32_t virtual_address =
            paging_physical_to_kernel_virtual(physical);
        uint32_t existing;
        if (paging_get_physical_address(virtual_address, &existing)) {
            if ((existing & PAGE_MASK) != physical) return 0;
        } else if (!paging_map_page(virtual_address, physical,
                                    PAGING_WRITABLE)) {
            return 0;
        }
        page += PAGE_SIZE;
    }
    return 1;
}

static int map_framebuffer_range(uint64_t physical_start, uint64_t length)
{
    if (!length) return 1;
    uint64_t end = physical_start + length;
    uint64_t offset = physical_start & (PAGE_SIZE - 1u);
    if (end < physical_start || end > 0x100000000ULL ||
        length + offset > 0x10000000ULL)
        return 0;

    uint64_t page = physical_start & ~(uint64_t)(PAGE_SIZE - 1u);
    uint64_t limit = (end + PAGE_SIZE - 1u) & ~(uint64_t)(PAGE_SIZE - 1u);
    uint32_t virtual_page = PAGING_FRAMEBUFFER_BASE;
    while (page < limit) {
        uint32_t existing;
        if (paging_get_physical_address(virtual_page, &existing)) {
            if ((existing & PAGE_MASK) != (uint32_t)page) return 0;
        } else if (!paging_map_page(virtual_page, (uint32_t)page,
                                    PAGING_WRITABLE)) {
            return 0;
        }
        page += PAGE_SIZE;
        virtual_page += PAGE_SIZE;
    }
    return 1;
}

int paging_init(const struct multiboot_info *mbi)
{
    /* The physical bootstrap has already loaded this directory into CR3 and
     * mapped the low 64 MiB identity and first 16 MiB at C0000000. Never clear active
     * high-half PDEs while executing from them. */
    page_table_count = 0;
    paging_active = 1;

    uint32_t kernel_physical_start = paging_virtual_to_physical(
        (uint32_t)&_kernel_start);
    uint32_t kernel_length = (uint32_t)(&_kernel_end - &_kernel_start);

    /* Keep low memory and the kernel's physical backing identity mapped for
     * boot data, GRUB modules, VGA, and the static table pool. Add/verify a
     * second mapping for the linked higher-half kernel. */
    if (!map_range(0, 0x100000u) ||
        !map_range(kernel_physical_start, kernel_length) ||
        !map_kernel_range(kernel_physical_start, kernel_length))
        return 0;

    if (mbi) {
        if (!map_range((uint32_t)mbi, sizeof(*mbi))) return 0;
        if ((mbi->flags & MULTIBOOT_INFO_CMDLINE) && mbi->cmdline &&
            !map_range(mbi->cmdline, 4096u)) return 0;
        if (mbi->flags & MULTIBOOT_INFO_MODS) {
            uint64_t desc_size = (uint64_t)mbi->mods_count *
                                 sizeof(struct multiboot_module);
            if (mbi->mods_count &&
                !map_range(mbi->mods_addr, desc_size)) return 0;
            const struct multiboot_module *mods =
                (const struct multiboot_module *)mbi->mods_addr;
            for (uint32_t i = 0; i < mbi->mods_count; i++) {
                if (mods[i].mod_end < mods[i].mod_start ||
                    !map_range(mods[i].mod_start,
                               mods[i].mod_end - mods[i].mod_start))
                    return 0;
                if (mods[i].string &&
                    !map_range(mods[i].string, 4096u)) return 0;
            }
        }
        if (mbi->flags & 0x40u) {
            if (!map_range(mbi->mmap_addr, mbi->mmap_length)) return 0;
        }
        if (mbi->flags & 0x1000u) {
            uint64_t fb_size = (uint64_t)mbi->framebuffer_pitch *
                               mbi->framebuffer_height;
            if (!map_framebuffer_range(mbi->framebuffer_addr, fb_size))
                return 0;
        }
    }

    return 1;
}

void paging_page_fault(uint32_t address, uint32_t error)
{
    serial_string("PAGE FAULT address=0x"); serial_hex(address);
    serial_string(" error=0x"); serial_hex(error); serial_string(" (");
    serial_string((error & 1u) ? "protection" : "not-present");
    serial_string((error & 2u) ? ", write" : ", read");
    serial_string((error & 4u) ? ", user" : ", supervisor");
    if (error & 8u) serial_string(", reserved-bit");
    if (error & 16u) serial_string(", instruction-fetch");
    serial_string(")\n");
    for (;;) __asm__ volatile ("cli; hlt");
}

void paging_test_unmapped_fault(void)
{
    uint32_t test_address = 0xF0000000u;
    while (test_address < 0xFFFFF000u &&
           paging_get_physical_address(test_address, 0))
        test_address += PAGE_SIZE;
    if (paging_get_physical_address(test_address, 0)) {
        serial_string("Paging test could not find an unmapped page.\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }
    __asm__ volatile ("movl (%0), %%eax" :: "r"(test_address) : "eax", "memory");
}
