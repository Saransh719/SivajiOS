
#include "pic.h"
#include "io/io.h"

/* Small delay for PIC initialization */
static void pic_io_wait(void)
{
    outb(0x80, 0);
}

/* Remap the master and slave PICs */
void pic_remap(void)
{
    /* Disable CPU interrupts during PIC configuration */
    __asm__ volatile ("cli");

    /* Mask all hardware interrupts initially */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);

    /* Start PIC initialization sequence */
    outb(PIC1_COMMAND, 0x11);
    pic_io_wait();

    outb(PIC2_COMMAND, 0x11);
    pic_io_wait();

    /* Set interrupt vector offsets */
    outb(PIC1_DATA, PIC1_START_INTERRUPT);
    pic_io_wait();

    outb(PIC2_DATA, PIC2_START_INTERRUPT);
    pic_io_wait();

    /* Tell master PIC that slave is connected to IRQ2 */
    outb(PIC1_DATA, 0x04);
    pic_io_wait();

    /* Tell slave PIC its cascade identity */
    outb(PIC2_DATA, 0x02);
    pic_io_wait();

    /* Set 8086 mode */
    outb(PIC1_DATA, 0x01);
    pic_io_wait();

    outb(PIC2_DATA, 0x01);
    pic_io_wait();

    /* Keep all IRQs masked until handlers are ready */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

/* Mask an IRQ */
void pic_set_mask(unsigned char irq)
{
    unsigned short port;
    unsigned char value;

    if (irq >= 16)
        return;

    if (irq < 8)
        port = PIC1_DATA;
    else
    {
        port = PIC2_DATA;
        irq -= 8;
    }

    value = inb(port) | (1 << irq);
    outb(port, value);
}

/* Unmask an IRQ */
void pic_clear_mask(unsigned char irq)
{
    unsigned short port;
    unsigned char value;

    if (irq >= 16)
        return;

    if (irq < 8)
        port = PIC1_DATA;
    else
    {
        port = PIC2_DATA;
        irq -= 8;
    }

    value = inb(port) & ~(1 << irq);
    outb(port, value);
}

/* Acknowledge a hardware interrupt */
void pic_acknowledge(unsigned int interrupt)
{
    if (interrupt < PIC1_START_INTERRUPT ||
        interrupt > PIC2_END_INTERRUPT)
        return;

    /* Acknowledge slave first for IRQ8-IRQ15 */
    if (interrupt >= PIC2_START_INTERRUPT)
        outb(PIC2_COMMAND, PIC_ACK);

    /* Always acknowledge master */
    outb(PIC1_COMMAND, PIC_ACK);
}
