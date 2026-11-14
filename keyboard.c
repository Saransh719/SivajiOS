#include "keyboard.h"
#include "io/io.h"
#include "keyboard_map.h"

volatile unsigned char keyboard_last_scancode = 0;

#define KEY_QUEUE_SIZE 64u
static volatile unsigned char key_queue[KEY_QUEUE_SIZE];
static volatile unsigned char queue_head;
static volatile unsigned char queue_tail;

static int shift_pressed = 0;
static int caps_lock = 0;

static void enqueue_key(unsigned char value)
{
    unsigned char next = (unsigned char)((queue_head + 1u) % KEY_QUEUE_SIZE);
    if (next == queue_tail) return; /* Drop newest input rather than corrupting. */
    key_queue[queue_head] = value;
    __asm__ volatile ("" ::: "memory");
    queue_head = next;
}

int keyboard_read_event(void)
{
    if (queue_tail == queue_head) return KEYBOARD_EVENT_NONE;
    __asm__ volatile ("" ::: "memory");
    unsigned char value = key_queue[queue_tail];
    queue_tail = (unsigned char)((queue_tail + 1u) % KEY_QUEUE_SIZE);
    if (value == '\n') return KEYBOARD_EVENT_ENTER;
    if (value == '\b') return KEYBOARD_EVENT_BACKSPACE;
    return value;
}

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

    if (scancode >= sizeof(keyboard_map))
        return;

    char character = translate_scancode(scancode);
    if (character != 0) enqueue_key((unsigned char)character);
}
