
#include "idt.h"

/* The IDT contains 256 entries */
static struct idt_entry idt_entries[256];
static struct idt_ptr idt_pointer;

/* Assembly interrupt handler declarations */
#define DECLARE_ISR(n) extern void interrupt_handler_##n(void)

DECLARE_ISR(0);  DECLARE_ISR(1);
DECLARE_ISR(2);  DECLARE_ISR(3);
DECLARE_ISR(4);  DECLARE_ISR(5);
DECLARE_ISR(6);  DECLARE_ISR(7);
DECLARE_ISR(8);  DECLARE_ISR(9);
DECLARE_ISR(10); DECLARE_ISR(11);
DECLARE_ISR(12); DECLARE_ISR(13);
DECLARE_ISR(14); DECLARE_ISR(15);
DECLARE_ISR(16); DECLARE_ISR(17);
DECLARE_ISR(18); DECLARE_ISR(19);
DECLARE_ISR(20); DECLARE_ISR(21);
DECLARE_ISR(22); DECLARE_ISR(23);
DECLARE_ISR(24); DECLARE_ISR(25);
DECLARE_ISR(26); DECLARE_ISR(27);
DECLARE_ISR(28); DECLARE_ISR(29);
DECLARE_ISR(30); DECLARE_ISR(31);

DECLARE_ISR(32); DECLARE_ISR(33);
DECLARE_ISR(34); DECLARE_ISR(35);
DECLARE_ISR(36); DECLARE_ISR(37);
DECLARE_ISR(38); DECLARE_ISR(39);
DECLARE_ISR(40); DECLARE_ISR(41);
DECLARE_ISR(42); DECLARE_ISR(43);
DECLARE_ISR(44); DECLARE_ISR(45);
DECLARE_ISR(46); DECLARE_ISR(47);
extern void interrupt_handler_128(void);

#undef DECLARE_ISR

/* Array containing all 48 handler addresses */
static void (*interrupt_handlers[48])(void) = {
    interrupt_handler_0,  interrupt_handler_1,
    interrupt_handler_2,  interrupt_handler_3,
    interrupt_handler_4,  interrupt_handler_5,
    interrupt_handler_6,  interrupt_handler_7,
    interrupt_handler_8,  interrupt_handler_9,
    interrupt_handler_10, interrupt_handler_11,
    interrupt_handler_12, interrupt_handler_13,
    interrupt_handler_14, interrupt_handler_15,
    interrupt_handler_16, interrupt_handler_17,
    interrupt_handler_18, interrupt_handler_19,
    interrupt_handler_20, interrupt_handler_21,
    interrupt_handler_22, interrupt_handler_23,
    interrupt_handler_24, interrupt_handler_25,
    interrupt_handler_26, interrupt_handler_27,
    interrupt_handler_28, interrupt_handler_29,
    interrupt_handler_30, interrupt_handler_31,

    interrupt_handler_32, interrupt_handler_33,
    interrupt_handler_34, interrupt_handler_35,
    interrupt_handler_36, interrupt_handler_37,
    interrupt_handler_38, interrupt_handler_39,
    interrupt_handler_40, interrupt_handler_41,
    interrupt_handler_42, interrupt_handler_43,
    interrupt_handler_44, interrupt_handler_45,
    interrupt_handler_46, interrupt_handler_47
};

/* Set an individual IDT entry */
void idt_set_gate(
    unsigned char num,
    unsigned int base,
    unsigned short selector,
    unsigned char flags
)
{
    idt_entries[num].offset_low = base & 0xFFFF;
    idt_entries[num].offset_high = (base >> 16) & 0xFFFF;

    idt_entries[num].selector = selector;
    idt_entries[num].zero = 0;
    idt_entries[num].type_attr = flags;
}

/* Initialize and load the IDT */
void idt_install(void)
{
    /* Configure IDT pointer */
    idt_pointer.limit = sizeof(idt_entries) - 1;
    idt_pointer.base = (unsigned int)&idt_entries;

    /* Clear all 256 entries */
    for (int i = 0; i < 256; i++)
    {
        idt_entries[i].offset_low = 0;
        idt_entries[i].offset_high = 0;
        idt_entries[i].selector = 0;
        idt_entries[i].zero = 0;
        idt_entries[i].type_attr = 0;
    }

    /* Register CPU exceptions and hardware IRQ handlers */
    for (int i = 0; i < 48; i++)
    {
        idt_set_gate(
            i,
            (unsigned int)interrupt_handlers[i],
            0x08,
            0x8F
        );
    }

    /* The only user-callable gate: present 32-bit interrupt gate, DPL 3. */
    idt_set_gate(0x80, (unsigned int)interrupt_handler_128, 0x08, 0xEE);

    /* Load IDT into the CPU */
    idt_flush((unsigned int)&idt_pointer);
}
