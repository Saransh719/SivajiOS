#include "tss.h"
#include "gdt.h"

static struct tss32 kernel_tss;
static uint8_t ring0_interrupt_stack[16384] __attribute__((aligned(16)));

typedef char tss32_must_be_104_bytes[sizeof(struct tss32) == 104 ? 1 : -1];

void tss_install(void)
{
    uint8_t *bytes = (uint8_t *)&kernel_tss;
    for (unsigned int i = 0; i < sizeof(kernel_tss); i++) bytes[i] = 0;

    kernel_tss.ss0 = GDT_KERNEL_DATA_SELECTOR;
    kernel_tss.esp0 = (uint32_t)(ring0_interrupt_stack +
                                 sizeof(ring0_interrupt_stack));
    /* Offset past the descriptor limit means there is no I/O bitmap. */
    kernel_tss.iomap_base = sizeof(kernel_tss);

    gdt_set_tss_descriptor((uint32_t)&kernel_tss,
                           sizeof(kernel_tss) - 1u);
    uint16_t selector = GDT_TSS_SELECTOR;
    __asm__ volatile ("ltr %0" :: "r"(selector) : "memory");
}
