#ifndef TSS_H
#define TSS_H

#include "multiboot.h"

/* 32-bit hardware TSS layout (Intel SDM, 104 bytes without an I/O bitmap). */
struct tss32 {
    uint16_t previous_task, reserved0;
    uint32_t esp0;
    uint16_t ss0, reserved1;
    uint32_t esp1;
    uint16_t ss1, reserved2;
    uint32_t esp2;
    uint16_t ss2, reserved3;
    uint32_t cr3, eip, eflags;
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint16_t es, reserved4;
    uint16_t cs, reserved5;
    uint16_t ss, reserved6;
    uint16_t ds, reserved7;
    uint16_t fs, reserved8;
    uint16_t gs, reserved9;
    uint16_t ldt, reserved10;
    uint16_t trap, iomap_base;
} __attribute__((packed));

void tss_install(void);

#endif
