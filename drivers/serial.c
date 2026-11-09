#include "serial.h"
#include "../io/io.h"

/* COM1 base port */
#define SERIAL_COM1_BASE 0x3F8

/* Serial port registers */

#define SERIAL_DATA_PORT(base)          (base)
#define SERIAL_FIFO_COMMAND_PORT(base)  (base + 2)
#define SERIAL_LINE_COMMAND_PORT(base)  (base + 3)
#define SERIAL_MODEM_COMMAND_PORT(base) (base + 4)
#define SERIAL_LINE_STATUS_PORT(base)   (base + 5)

/* Commands */

#define SERIAL_LINE_ENABLE_DLAB 0x80

void serial_configure_baud_rate(
    unsigned short com,
    unsigned short divisor
)
{
    outb(
        SERIAL_LINE_COMMAND_PORT(com),
        SERIAL_LINE_ENABLE_DLAB
    );

    outb(
        SERIAL_DATA_PORT(com),
        (divisor >> 8) & 0x00FF
    );

    outb(
        SERIAL_DATA_PORT(com),
        divisor & 0x00FF
    );
}

void serial_configure_line(unsigned short com)
{
    outb(SERIAL_LINE_COMMAND_PORT(com), 0x03);
}

void serial_configure_buffers(unsigned short com)
{
    outb(SERIAL_FIFO_COMMAND_PORT(com), 0xC7);
}

void serial_configure_modem(unsigned short com)
{
    outb(SERIAL_MODEM_COMMAND_PORT(com), 0x03);
}

int serial_is_transmit_fifo_empty(unsigned int com)
{
    return inb(SERIAL_LINE_STATUS_PORT(com)) & 0x20;
}

void serial_write_char(unsigned short com, char c)
{
    while (serial_is_transmit_fifo_empty(com) == 0)
    {
        /* Wait until transmitter is ready */
    }

    outb(SERIAL_DATA_PORT(com), c);
}

int serial_write(
    unsigned short com,
    const char *buf,
    unsigned int len
)
{
    if (!buf)
        return -1;

    for (unsigned int i = 0; i < len; i++)
    {
        serial_write_char(com, buf[i]);
    }

    return (int)len;
}

void serial_initialize(void)
{
    unsigned short com = SERIAL_COM1_BASE;

    serial_configure_baud_rate(com, 1);
    serial_configure_line(com);
    serial_configure_buffers(com);
    serial_configure_modem(com);
}