#ifndef DEBUG_H
#define DEBUG_H

static inline void debug_char(char c)
{
    __asm__ volatile (
        "outb %0, $0xE9"
        :
        : "a"(c)
    );
}

static void debug_string(const char *str)
{
    while (*str)
        debug_char(*str++);
}

static void debug_hex(unsigned int value)
{
    const char hex[] = "0123456789ABCDEF";

    debug_string("0x");

    for (int i = 7; i >= 0; i--)
        debug_char(hex[(value >> (i * 4)) & 0xF]);

    debug_char('\n');
}

#endif