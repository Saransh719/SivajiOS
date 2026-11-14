#include "multiboot.h"
#include "drivers/framebuffer.h"
#include "drivers/serial.h"
#include "idt.h"
#include "pic.h"

int fb_init(const struct multiboot_info *mbi);

void kmain(uint32_t magic, uint32_t mbi_address)
{
    // Verify Multiboot bootloader
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
        return;

    struct multiboot_info *mbi =
        (struct multiboot_info *)mbi_address;

    // Initialize framebuffer
    if (!fb_init(mbi))
        return;

    // Initialize hardware and interrupts
    serial_initialize();
    idt_install();
    pic_remap();
    pic_clear_mask(1);
    __asm__ volatile ("sti");

    serial_write(
        0x3F8,
        "IDT successfully loaded!\n",
        sizeof("IDT successfully loaded!\n") - 1
    );

    serial_write(
        0x3F8,
        "Hello from my kernel!\n",
        sizeof("Hello from my kernel!\n") - 1
    );

    // Initialize framebuffer display
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

    // --------------------------------
    // External Program Module Loading
    // --------------------------------

    // Check if GRUB provided module information
    if (!(mbi->flags & MULTIBOOT_INFO_MODS))
    {
        serial_write(0x3F8,
            "ERROR: No module information!\n",
            sizeof("ERROR: No module information!\n") - 1);
        return;
    }

    // Verify exactly one module was loaded
    if (mbi->mods_count != 1)
    {
        serial_write(0x3F8,
            "ERROR: Expected exactly one module!\n",
            sizeof("ERROR: Expected exactly one module!\n") - 1);
        return;
    }

    // Get module descriptor
    struct multiboot_module *module =
        (struct multiboot_module *)mbi->mods_addr;

    // Retrieve module memory boundaries
    uint32_t program_start = module->mod_start;
    uint32_t program_end = module->mod_end;

    // Validate module boundaries
    if (program_end <= program_start)
    {
        serial_write(0x3F8,
            "ERROR: Invalid module boundaries!\n",
            sizeof("ERROR: Invalid module boundaries!\n") - 1);
        return;
    }

    uint32_t program_size = program_end - program_start;


    // Module successfully located
    serial_write(0x3F8,
        "External program loaded successfully!\n",
        sizeof("External program loaded successfully!\n") - 1);

    // Convert program address into a function pointer
    typedef void (*program_entry_t)(void);
        
    program_entry_t start_program =
        (program_entry_t)program_start;
        
    // Execute external program
    serial_write(0x3F8,
        "Starting external program...\n",
        sizeof("Starting external program...\n") - 1);
    
    start_program();
    
    // This should never execute because program loops forever
    serial_write(0x3F8,
        "ERROR: Program returned!\n",
        sizeof("ERROR: Program returned!\n") - 1);

    (void)program_size;
}
