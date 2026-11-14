#include "interrupt.h"
#include "pic.h"
#include "keyboard.h"
#include "mm/paging.h"

volatile unsigned int last_interrupt_number = 0;
volatile unsigned int interrupt_count = 0;

void interrupt_handler(struct cpu_state *cpu, unsigned int interrupt) {
    if (interrupt == 14) {
        uint32_t address;
        uint32_t error_code = ((uint32_t *)cpu)[9];
        __asm__ volatile ("mov %%cr2, %0" : "=r"(address));
        paging_page_fault(address, error_code);
    }

    last_interrupt_number = interrupt;
    interrupt_count++;

    if (interrupt == 33) {
        keyboard_handler();
    }

    if (interrupt >= 32 && interrupt <= 47) {
        pic_acknowledge(interrupt);
    }
}
