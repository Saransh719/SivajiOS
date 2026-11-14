#include "elf.h"
#include "elf_validate.h"
#include "../mm/paging.h"
#include "../mm/pmm.h"
#include "../mm/heap.h"
#include "../usermode.h"
#include "../drivers/serial.h"

#define PAGE 4096u
#define PAGE_MASK 0xFFFFF000u
#define STACK_BASE ELF32_STACK_BASE
#define STACK_PAGES ELF32_STACK_PAGES
#define COM1 0x3F8u

typedef struct { uint32_t va, pa; } owned_page;

static void say(const char *s)
{ unsigned n=0; while(s[n]) n++; serial_write(COM1,s,n); }

static void rollback(owned_page *pages, uint32_t count)
{
    while (count) {
        owned_page *p = &pages[--count];
        (void)paging_unmap_page(p->va);
        pmm_free_frame(p->pa);
    }
}

int elf32_run_module(uint32_t start, uint32_t size)
{
    elf32_plan plan;
    owned_page *owned;
    uint32_t used = 0;
    if (!start || !elf32_validate_image((const uint8_t *)start, size, &plan)) {
        say("ELF: invalid or unsupported executable.\n");
        return 0;
    }
    for (uint32_t va = STACK_BASE; va < STACK_BASE + STACK_PAGES * PAGE; va += PAGE)
        if (paging_get_physical_address(va, 0)) {
            say("ELF: reserved user stack range is already mapped.\n");
            return 0;
        }
    for (uint32_t i = 0; i < plan.load_count; i++) {
        elf32_load_segment *p = &plan.loads[i];
        uint64_t end = (uint64_t)p->vaddr + p->memsz;
        uint32_t first = p->vaddr & PAGE_MASK;
        uint32_t limit = (uint32_t)((end + PAGE - 1u) & ~(uint64_t)(PAGE - 1u));
        for (uint32_t va = first; va < limit; va += PAGE)
            if (paging_get_physical_address(va, 0)) {
                say("ELF: a load segment overlaps an existing mapping.\n");
                return 0;
            }
    }

    owned = (owned_page *)kmalloc(plan.page_count * sizeof(*owned));
    if (!owned) { say("ELF: allocation tracking failed.\n"); return 0; }
    for (uint32_t i = 0; i < plan.load_count; i++) {
        elf32_load_segment *p = &plan.loads[i];
        uint64_t end = (uint64_t)p->vaddr + p->memsz;
        uint32_t first = p->vaddr & PAGE_MASK;
        uint32_t limit = (uint32_t)((end + PAGE - 1u) & ~(uint64_t)(PAGE - 1u));
        for (uint32_t va = first; va < limit; va += PAGE) {
            uint32_t pa = pmm_alloc_frame();
            uint32_t map_flags = PAGING_PRESENT | PAGING_USER |
                ((p->flags & 2u) ? PAGING_WRITABLE : 0);
            if (pa == PMM_FRAME_FAILURE || !paging_map_page(va, pa, map_flags)) {
                if (pa != PMM_FRAME_FAILURE) pmm_free_frame(pa);
                rollback(owned, used); kfree(owned);
                say("ELF: mapping failed; rolled back.\n"); return 0;
            }
            owned[used++] = (owned_page){va, pa};
            uint32_t flags;
            if (!paging_get_page_flags(va, &flags) || !(flags & PAGING_USER) ||
                (((p->flags & 2u) != 0) != ((flags & PAGING_WRITABLE) != 0))) {
                rollback(owned, used); kfree(owned);
                say("ELF: effective page permissions invalid.\n"); return 0;
            }
            uint8_t *dst = (uint8_t *)va;
            for (uint32_t b = 0; b < PAGE; b++) dst[b] = 0;
        }
        uint8_t *dst = (uint8_t *)p->vaddr;
        const uint8_t *src = (const uint8_t *)(start + p->offset);
        for (uint32_t b = 0; b < p->filesz; b++) dst[b] = src[b];
    }
    for (uint32_t i = 0; i < STACK_PAGES; i++) {
        uint32_t va = STACK_BASE + i * PAGE, pa = pmm_alloc_frame();
        if (pa == PMM_FRAME_FAILURE ||
            !paging_map_page(va, pa, PAGING_PRESENT | PAGING_USER | PAGING_WRITABLE)) {
            if (pa != PMM_FRAME_FAILURE) pmm_free_frame(pa);
            rollback(owned, used); kfree(owned);
            say("ELF: stack setup failed; rolled back.\n"); return 0;
        }
        owned[used++] = (owned_page){va, pa};
        uint8_t *dst = (uint8_t *)va;
        for (uint32_t b = 0; b < PAGE; b++) dst[b] = 0;
    }

    say("ELF: entering Ring 3.\n");
    uint32_t status = 0xFFFFFFFFu;
    int returned = usermode_run_loaded(plan.entry,
                                       STACK_BASE + STACK_PAGES * PAGE,
                                       &status);
    /* The TSS stack and Ring 0 trampoline have resumed before this cleanup. */
    rollback(owned, used);
    kfree(owned);
    say(returned ? "ELF: returned from user context.\n" :
                   "ELF: user fault or exception.\n");
    return returned && status == 0;
}
