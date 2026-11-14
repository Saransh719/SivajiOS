
#ifndef GDT_H
#define GDT_H

/* One GDT entry is exactly 8 bytes */
struct gdt_entry {
    unsigned short limit_low;
    unsigned short base_low;
    unsigned char  base_middle;
    unsigned char  access;
    unsigned char  granularity;
    unsigned char  base_high;
} __attribute__((packed));

/* Pointer passed to the LGDT instruction */
struct gdt_ptr {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

/* Initialize and load the GDT */
void gdt_install(void);
void gdt_set_tss_descriptor(unsigned int base, unsigned int limit);

#define GDT_KERNEL_CODE_SELECTOR 0x08u
#define GDT_KERNEL_DATA_SELECTOR 0x10u
#define GDT_USER_CODE_SELECTOR   0x1Bu
#define GDT_USER_DATA_SELECTOR   0x23u
#define GDT_TSS_SELECTOR         0x28u

/* Assembly function that loads the GDT */
void gdt_flush(unsigned int);

#endif
