#include "multiboot.h"
#include "drivers/framebuffer.h"
#include "drivers/serial.h"
#include "idt.h"
#include "pic.h"

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

int fb_init(const struct multiboot_info *mbi);

void kmain(uint32_t magic, uint32_t mbi_address)
{
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
        return;

    struct multiboot_info *mbi =
        (struct multiboot_info *)mbi_address;

    if (!fb_init(mbi))
        return;

    serial_initialize();
    idt_install();
    pic_remap();
    pic_clear_mask(1);  // Unmask keyboard IRQ
    __asm__ volatile ("sti");  // Enable CPU interrupts

    serial_write(
        0x3F8,
        "IDT successfully loaded!\n",
        25
    );

    serial_write(
        0x3F8,
        "Hello from my kernel!\n",
        22
    );

    fb_clear(FB_WHITE, FB_BLACK);

    fb_write_string(
        "Welcome to Nigga OS!",
        0, 0,
        FB_GREEN,
        FB_BLACK
    );

    fb_write_string(
        "Framebuffer driver is working!",
        2, 0,
        FB_LIGHT_GREY,
        FB_BLACK
    );

    fb_write_string(
        "Talking directly to hardware.",
        4, 0,
        FB_CYAN,
        FB_BLACK
    );

    fb_clear(FB_WHITE, FB_BLACK);

    fb_write("Hello again from NiggaOS!\n", 26);
    fb_write("This is the new framebuffer driver.", 36);
}