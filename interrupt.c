#include "interrupt.h"
#include "pic.h"
#include "keyboard.h"
#include "mm/paging.h"
#include "usermode.h"

volatile unsigned int last_interrupt_number = 0;
volatile unsigned int interrupt_count = 0;

void interrupt_handler(struct cpu_state *cpu, unsigned int interrupt) {
    uint32_t *frame = (uint32_t *)cpu;
    int from_user = (frame[11] & 3u) == 3u;
    if (interrupt == 14) {
        uint32_t address;
        uint32_t error_code = ((uint32_t *)cpu)[9];
        __asm__ volatile ("mov %%cr2, %0" : "=r"(address));
        if (from_user) {
            paging_report_page_fault(address, error_code);
            (void)usermode_handle_interrupt(cpu, interrupt, error_code,
                                            address);
        } else {
            paging_page_fault(address, error_code);
        }
    } else if (interrupt == 0x80u && from_user) {
        (void)usermode_handle_interrupt(cpu, interrupt, 0, 0);
    } else if (from_user && interrupt < 32u) {
        (void)usermode_handle_interrupt(cpu, interrupt, 0, 0);
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
