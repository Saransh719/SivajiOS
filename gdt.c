
#include "gdt.h"

/* Three GDT entries: null, code, data */
static struct gdt_entry gdt_entries[3];

/* GDT pointer */
static struct gdt_ptr gdt_pointer;

/* Set an individual GDT descriptor */
static void gdt_set_gate(
    int index,
    unsigned int base,
    unsigned int limit,
    unsigned char access,
    unsigned char granularity
)
{
    gdt_entries[index].base_low = base & 0xFFFF;
    gdt_entries[index].base_middle = (base >> 16) & 0xFF;
    gdt_entries[index].base_high = (base >> 24) & 0xFF;

    gdt_entries[index].limit_low = limit & 0xFFFF;

    gdt_entries[index].granularity =
        ((limit >> 16) & 0x0F) | (granularity & 0xF0);

    gdt_entries[index].access = access;
}

/* Initialize the GDT */
void gdt_install(void)
{
    gdt_pointer.limit = sizeof(gdt_entries) - 1;
    gdt_pointer.base = (unsigned int)&gdt_entries;

    /* Null descriptor */
    gdt_set_gate(0, 0, 0, 0, 0);

    /* Kernel code segment: Base 0, Limit 4 GB, RX, DPL 0 */
    gdt_set_gate(
        1,
        0x00000000,
        0xFFFFFFFF,
        0x9A,
        0xCF
    );

    /* Kernel data segment: Base 0, Limit 4 GB, RW, DPL 0 */
    gdt_set_gate(
        2,
        0x00000000,
        0xFFFFFFFF,
        0x92,
        0xCF
    );

    /* Load the GDT */
    gdt_flush((unsigned int)&gdt_pointer);
}
