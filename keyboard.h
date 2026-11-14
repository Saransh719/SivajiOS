#ifndef KEYBOARD_H
#define KEYBOARD_H

extern volatile unsigned char keyboard_last_scancode;

#define KEYBOARD_EVENT_NONE      (-1)
#define KEYBOARD_EVENT_ENTER     0x100
#define KEYBOARD_EVENT_BACKSPACE 0x101

void keyboard_handler(void);
/* Single-consumer polling interface; events are translated in the IRQ path. */
int keyboard_read_event(void);

#endif
