
#ifndef INTERRUPT_H
#define INTERRUPT_H

struct cpu_state {
    unsigned int edi;
    unsigned int esi;
    unsigned int ebp;
    unsigned int esp;
    unsigned int ebx;
    unsigned int edx;
    unsigned int ecx;
    unsigned int eax;
} __attribute__((packed));

void interrupt_handler(
    struct cpu_state *cpu,
    unsigned int interrupt
);

#endif
