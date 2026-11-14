
#ifndef IDT_H
#define IDT_H

/* One IDT entry is exactly 8 bytes */
struct idt_entry {
    unsigned short offset_low;
    unsigned short selector;
    unsigned char zero;
    unsigned char type_attr;
    unsigned short offset_high;
} __attribute__((packed));

/* IDTR pointer */
struct idt_ptr {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

/* Initialize the IDT */
void idt_install(void);
int idt_is_installed(void);

/* Load the IDT using assembly */
void idt_flush(unsigned int);

void idt_set_gate(
    unsigned char,
    unsigned int,
    unsigned short,
    unsigned char
);

#endif
