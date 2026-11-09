#include "interrupt.h"
#include "pic.h"
#include "keyboard.h"

volatile unsigned int last_interrupt_number = 0;
volatile unsigned int interrupt_count = 0;

void interrupt_handler(struct cpu_state *cpu, unsigned int interrupt) {
    (void)cpu;

    last_interrupt_number = interrupt;
    interrupt_count++;

    if (interrupt == 33) {
        keyboard_handler();
    }

    if (interrupt >= 32 && interrupt <= 47) {
        pic_acknowledge(interrupt);
    }
}