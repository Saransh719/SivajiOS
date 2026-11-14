#include "multiboot.h"
#include "drivers/framebuffer.h"
#include "drivers/serial.h"
#include "idt.h"
#include "pic.h"
#include "mm/paging.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "tss.h"
#include "usermode.h"
#include "keyboard.h"

int fb_init(const struct multiboot_info *mbi);

static uint32_t current_instruction_pointer(void)
{
    uint32_t instruction_pointer;
    __asm__ volatile ("call 1f\n1: popl %0" : "=r"(instruction_pointer));
    return instruction_pointer;
}

static void serial_write_hex32(uint32_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    char text[8];
    for (unsigned int i = 0; i < 8; i++)
        text[i] = digits[(value >> ((7u - i) * 4u)) & 0xFu];
    serial_write(0x3F8, text, sizeof(text));
}

void kmain(uint32_t magic, uint32_t mbi_address)
{
    // Verify Multiboot bootloader
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
        return;

    struct multiboot_info *mbi =
        (struct multiboot_info *)mbi_address;

    // Initialize hardware and interrupts
    serial_initialize();
    tss_install();
    idt_install();
    if (!paging_init(mbi)) {
        serial_write(0x3F8, "ERROR: Paging initialization failed!\n",
                     sizeof("ERROR: Paging initialization failed!\n") - 1);
        return;
    }
    if (!fb_init(mbi)) {
        serial_write(0x3F8, "ERROR: Framebuffer initialization failed!\n",
                     sizeof("ERROR: Framebuffer initialization failed!\n") - 1);
        return;
    }
    pmm_init(mbi);
    heap_init();
    uint32_t translated = 0;
    uint32_t kmain_virtual = (uint32_t)&kmain;
    if (!paging_get_physical_address((uint32_t)&kmain, &translated) ||
        translated != paging_virtual_to_physical(kmain_virtual)) {
        serial_write(0x3F8, "ERROR: Higher-half kernel map check failed!\n",
                     sizeof("ERROR: Higher-half kernel map check failed!\n") - 1);
        return;
    }
    serial_write(0x3F8, "Higher-half kernel mapping verified.\n",
                 sizeof("Higher-half kernel mapping verified.\n") - 1);
    serial_write(0x3F8, "Kernel executing at virtual EIP=0x",
                 sizeof("Kernel executing at virtual EIP=0x") - 1);
    serial_write_hex32(current_instruction_pointer());
    serial_write(0x3F8, "\n", 1);
    if (mbi->flags & 0x1000u) {
        uint32_t framebuffer_physical = 0;
        uint32_t framebuffer_physical_address =
            (uint32_t)mbi->framebuffer_addr;
        uint32_t framebuffer_address =
            paging_physical_to_framebuffer_virtual(
                framebuffer_physical_address);
        if (!paging_get_physical_address(framebuffer_address,
                                         &framebuffer_physical) ||
            framebuffer_physical != framebuffer_physical_address) {
            serial_write(0x3F8, "ERROR: Framebuffer mapping check failed!\n",
                         sizeof("ERROR: Framebuffer mapping check failed!\n") - 1);
            return;
        }
        serial_write(0x3F8, "Framebuffer virtual mapping verified.\n",
                     sizeof("Framebuffer virtual mapping verified.\n") - 1);
    }
    if ((mbi->flags & MULTIBOOT_INFO_MODS) && mbi->mods_count) {
        const struct multiboot_module *module =
            (const struct multiboot_module *)mbi->mods_addr;
        uint32_t module_physical = 0;
        if (module->mod_end <= module->mod_start ||
            !paging_get_physical_address(module->mod_start,
                                         &module_physical) ||
            module_physical != module->mod_start) {
            serial_write(0x3F8, "ERROR: GRUB module mapping check failed!\n",
                         sizeof("ERROR: GRUB module mapping check failed!\n") - 1);
            return;
        }
        serial_write(0x3F8, "GRUB module identity map verified.\n",
                     sizeof("GRUB module identity map verified.\n") - 1);
    }
    if ((mbi->flags & MULTIBOOT_INFO_CMDLINE) && mbi->cmdline) {
        const char *arg = (const char *)mbi->cmdline;
        static const char pmm_test[] = "pmm-test";
        unsigned int i = 0;
        while (pmm_test[i] && arg[i] == pmm_test[i]) i++;
        if (!pmm_test[i] && arg[i] == '\0') {
            (void)pmm_run_self_tests();
            for (;;) __asm__ volatile ("cli; hlt");
        }
    }
    if ((mbi->flags & MULTIBOOT_INFO_CMDLINE) && mbi->cmdline) {
        const char *arg = (const char *)mbi->cmdline;
        static const char heap_test[] = "heap-test";
        unsigned int i = 0;
        while (heap_test[i] && arg[i] == heap_test[i]) i++;
        if (!heap_test[i] && arg[i] == '\0') {
            (void)heap_run_self_tests();
            for (;;) __asm__ volatile ("cli; hlt");
        }
    }
    if ((mbi->flags & MULTIBOOT_INFO_CMDLINE) && mbi->cmdline) {
        const char *arg = (const char *)mbi->cmdline;
        static const char fault_test[] = "paging-fault-test";
        unsigned int i = 0;
        while (fault_test[i] && arg[i] == fault_test[i]) i++;
        if (!fault_test[i] && arg[i] == '\0')
            paging_test_unmapped_fault();
    }
    pic_remap();
    pic_clear_mask(1);
    __asm__ volatile ("sti");

    if ((mbi->flags & MULTIBOOT_INFO_CMDLINE) && mbi->cmdline) {
        const char *arg = (const char *)mbi->cmdline;
        static const char ring3_test[] = "ring3-test";
        unsigned int i = 0;
        while (ring3_test[i] && arg[i] == ring3_test[i]) i++;
        if (!ring3_test[i] && arg[i] == '\0') {
            (void)usermode_run_test(0);
            for (;;) __asm__ volatile ("cli; hlt");
        }
    }
    if ((mbi->flags & MULTIBOOT_INFO_CMDLINE) && mbi->cmdline) {
        const char *arg = (const char *)mbi->cmdline;
        static const char ring3_fault_test[] = "ring3-fault-test";
        unsigned int i = 0;
        while (ring3_fault_test[i] && arg[i] == ring3_fault_test[i]) i++;
        if (!ring3_fault_test[i] && arg[i] == '\0') {
            (void)usermode_run_test(1);
            for (;;) __asm__ volatile ("cli; hlt");
        }
    }
    if ((mbi->flags & MULTIBOOT_INFO_CMDLINE) && mbi->cmdline) {
        const char *arg = (const char *)mbi->cmdline;
        static const char syscall_test[] = "syscall-test";
        unsigned int i = 0;
        while (syscall_test[i] && arg[i] == syscall_test[i]) i++;
        if (!syscall_test[i] && arg[i] == '\0') {
            (void)usermode_run_test(USERMODE_TEST_SYSCALL);
            for (;;) __asm__ volatile ("cli; hlt");
        }
    }

    if ((mbi->flags & MULTIBOOT_INFO_CMDLINE) && mbi->cmdline) {
        const char *arg = (const char *)mbi->cmdline;
        static const char keyboard_test[] = "paging-keyboard-test";
        unsigned int i = 0;
        while (keyboard_test[i] && arg[i] == keyboard_test[i]) i++;
        if (!keyboard_test[i] && arg[i] == '\0') {
            serial_write(0x3F8, "Waiting for keyboard IRQ...\n",
                         sizeof("Waiting for keyboard IRQ...\n") - 1);
            while (!keyboard_last_scancode)
                __asm__ volatile ("hlt");
            serial_write(0x3F8, "Keyboard IRQ received.\n",
                         sizeof("Keyboard IRQ received.\n") - 1);
        }
    }

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

    uint32_t module_physical = 0;
    if (!paging_get_physical_address(program_start, &module_physical) ||
        module_physical != program_start ||
        !paging_get_physical_address(program_end - 1, &module_physical) ||
        module_physical != program_end - 1) {
        serial_write(0x3F8, "ERROR: GRUB module mapping check failed!\n",
                     sizeof("ERROR: GRUB module mapping check failed!\n") - 1);
        return;
    }


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
    serial_write(0x3F8, "External module physical range [0x",
                 sizeof("External module physical range [0x") - 1);
    serial_write_hex32(program_start);
    serial_write(0x3F8, ", 0x", sizeof(", 0x") - 1);
    serial_write_hex32(program_end);
    serial_write(0x3F8, ")\n", sizeof(")\n") - 1);
    
    start_program();
    
    // This should never execute because program loops forever
    serial_write(0x3F8,
        "ERROR: Program returned!\n",
        sizeof("ERROR: Program returned!\n") - 1);

    (void)program_size;
}
