#include "elf_validate.h"

#define PAGE_SIZE 4096u
#define PAGE_MASK 0xFFFFF000u
#define USER_MIN 0x40000000u
#define USER_MAX 0x80000000u
#define PT_LOAD 1u
#define PT_DYNAMIC 2u
#define PT_INTERP 3u
#define PF_X 1u
#define PF_W 2u
#define PF_R 4u

static int range_in_file(uint32_t offset, uint32_t length, uint32_t size)
{ return offset <= size && length <= size - offset; }

static int is_power_of_two(uint32_t value)
{ return value && !(value & (value - 1u)); }

int elf32_validate_image(const uint8_t *image, uint32_t size, elf32_plan *plan)
{
    const elf32_header *header;
    const elf32_program *programs;
    uint64_t table_size;
    uint32_t pages = ELF32_STACK_PAGES;
    int entry_executable = 0;
    if (!image || !plan || size < sizeof(elf32_header)) return 0;
    header = (const elf32_header *)image;
    if (header->ident[0] != 0x7F || header->ident[1] != 'E' ||
        header->ident[2] != 'L' || header->ident[3] != 'F' ||
        header->ident[4] != 1 || header->ident[5] != 1 ||
        header->ident[6] != 1 || header->type != 2 || header->machine != 3 ||
        header->version != 1 || header->ehsize != sizeof(*header) ||
        header->phentsize != sizeof(elf32_program) || !header->phnum ||
        header->phnum > ELF32_MAX_PROGRAM_HEADERS)
        return 0;
    table_size = (uint64_t)header->phnum * sizeof(elf32_program);
    if (table_size > 0xFFFFFFFFu ||
        !range_in_file(header->phoff, (uint32_t)table_size, size))
        return 0;
    programs = (const elf32_program *)(image + header->phoff);
    plan->entry = header->entry;
    plan->load_count = 0;
    plan->page_count = 0;

    for (uint32_t i = 0; i < header->phnum; i++) {
        const elf32_program *p = &programs[i];
        if (p->type == PT_DYNAMIC || p->type == PT_INTERP) return 0;
        if (p->type != PT_LOAD) continue;
        uint64_t end = (uint64_t)p->vaddr + p->memsz;
        if (!p->memsz || p->filesz > p->memsz ||
            !range_in_file(p->offset, p->filesz, size) ||
            p->vaddr < USER_MIN || end > USER_MAX || end <= p->vaddr ||
            (p->flags & ~7u) ||
            (p->align > 1u && (!is_power_of_two(p->align) ||
             ((p->vaddr - p->offset) & (p->align - 1u)))))
            return 0;

        uint32_t first = p->vaddr & PAGE_MASK;
        uint32_t last = (uint32_t)((end - 1u) & PAGE_MASK);
        uint64_t rounded_end = (end + PAGE_SIZE - 1u) & ~(uint64_t)(PAGE_SIZE - 1u);
        if (first < ELF32_STACK_BASE + ELF32_STACK_PAGES * PAGE_SIZE &&
            rounded_end > ELF32_STACK_BASE)
            return 0;

        /* Reject any shared page: a single PTE cannot express distinct ELF
         * permissions for two segments that occupy the same page. */
        for (uint32_t j = 0; j < plan->load_count; j++) {
            uint64_t other_end = (uint64_t)plan->loads[j].vaddr +
                                 plan->loads[j].memsz;
            uint32_t other_first = plan->loads[j].vaddr & PAGE_MASK;
            uint32_t other_last = (uint32_t)((other_end - 1u) & PAGE_MASK);
            if (first <= other_last && other_first <= last) return 0;
        }
        uint64_t segment_pages = (rounded_end - first) / PAGE_SIZE;
        if (segment_pages > ELF32_MAX_IMAGE_PAGES - pages) return 0;
        pages += (uint32_t)segment_pages;

        elf32_load_segment *load = &plan->loads[plan->load_count++];
        load->offset = p->offset;
        load->vaddr = p->vaddr;
        load->filesz = p->filesz;
        load->memsz = p->memsz;
        load->flags = p->flags;
        if ((p->flags & PF_X) && header->entry >= p->vaddr &&
            (uint64_t)header->entry < end)
            entry_executable = 1;
    }
    if (!entry_executable || !plan->load_count) return 0;
    plan->page_count = pages;
    return 1;
}
