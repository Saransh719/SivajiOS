#include "keyboard.h"
#include "io/io.h"
#include "keyboard_map.h"
#include "drivers/framebuffer.h"

volatile unsigned char keyboard_last_scancode = 0;

static int shift_pressed = 0;
static int caps_lock = 0;

static char translate_scancode(unsigned char scancode)
{
    char character = keyboard_map[scancode];

    // Letters use Shift XOR Caps Lock.
    if (character >= 'a' && character <= 'z') {
        if (shift_pressed ^ caps_lock)
            character -= 'a' - 'A';

        return character;
    }

    if (!shift_pressed)
        return character;

    // Shifted number-row characters.
    switch (scancode) {
        case 0x02: return '!';
        case 0x0A: return '(';
        case 0x0B: return ')';
        case 0x0C: return '_';
        case 0x0D: return '+';
        case 0x1A: return '{';
        case 0x1B: return '}';
        case 0x27: return ':';
        case 0x33: return '<';
        case 0x34: return '>';
        case 0x35: return '?';
        default: return character;
    }
}

void keyboard_handler(void)
{
    unsigned char scancode = inb(0x60);

    keyboard_last_scancode = scancode;

    // Left or right Shift pressed.
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return;
    }

    // Left or right Shift released.
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
        return;
    }

    // Ignore other key-release events.
    if (scancode & 0x80)
        return;

    // Caps Lock toggles on key press.
    if (scancode == 0x3A) {
        caps_lock = !caps_lock;
        return;
    }

    // Backspace.
    if (scancode == 0x0E) {
        fb_backspace();
        return;
    }

    if (scancode >= sizeof(keyboard_map))
        return;

    char character = translate_scancode(scancode);

    if (character != 0) {
        fb_write(&character, 1);
    }
}