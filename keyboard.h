#ifndef KEYBOARD_H
#define KEYBOARD_H

extern volatile unsigned char keyboard_last_scancode;

void keyboard_handler(void);

#endif