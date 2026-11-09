
#ifndef PIC_H
#define PIC_H

/* PIC I/O ports */
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21

#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

/* Interrupt vector offsets */
#define PIC1_START_INTERRUPT 0x20
#define PIC2_START_INTERRUPT 0x28
#define PIC2_END_INTERRUPT   0x2F

/* End Of Interrupt command */
#define PIC_ACK 0x20

void pic_remap(void);
void pic_set_mask(unsigned char irq);
void pic_clear_mask(unsigned char irq);
void pic_acknowledge(unsigned int interrupt);

#endif
