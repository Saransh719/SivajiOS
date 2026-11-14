#include "shell.h"
#include "../drivers/framebuffer.h"
#include "../drivers/serial.h"
#include "../keyboard.h"
#include "../mm/pmm.h"
#include "../exec/elf.h"
#include "../exec/elf_validate.h"
#include "../version.h"

#define COM1 0x3F8u
#define INPUT_CAPACITY 128u
#define MODULE_MAIN_TAG "shell-main"
#define MODULE_FAULT_TAG "shell-fault"

struct shell_module { uint32_t start, size; int state; };
static struct shell_module main_module;
static struct shell_module fault_module;
static char input[INPUT_CAPACITY];
static uint32_t input_length;
static int input_overflow;
static int executable_ready;

static void write_bytes(const char *text, uint32_t length)
{
    if (!length) return;
    (void)fb_write((char *)text, length);
    (void)serial_write(COM1, text, length);
}

static void puts(const char *text)
{
    uint32_t length = 0;
    while (text[length]) length++;
    write_bytes(text, length);
}

static void print_u32(uint32_t value)
{
    char digits[10];
    uint32_t count = 0;
    do { digits[count++] = (char)('0' + value % 10u); value /= 10u; }
    while (value && count < sizeof(digits));
    while (count) write_bytes(&digits[--count], 1);
}

static uint64_t divmod_u64_u32(uint64_t value, uint32_t divisor,
                               uint32_t *remainder)
{
    uint64_t quotient = 0;
    uint64_t partial = 0;
    for (int bit = 63; bit >= 0; bit--) {
        partial = (partial << 1) | ((value >> bit) & 1u);
        if (partial >= divisor) {
            partial -= divisor;
            quotient |= (uint64_t)1u << bit;
        }
    }
    *remainder = (uint32_t)partial;
    return quotient;
}

static void print_u64(uint64_t value)
{
    char digits[20];
    uint32_t count = 0;
    do {
        uint32_t remainder;
        value = divmod_u64_u32(value, 10u, &remainder);
        digits[count++] = (char)('0' + remainder);
    } while (value && count < sizeof(digits));
    while (count) write_bytes(&digits[--count], 1);
}

static void print_mib(uint64_t bytes)
{
    const uint32_t mib = 1024u * 1024u;
    uint32_t fraction = ((uint32_t)(bytes & (mib - 1u)) * 100u) / mib;
    print_u64(bytes >> 20);
    puts(fraction < 10u ? ".0" : ".");
    print_u32(fraction);
    puts(" MiB");
}

static int module_tag_matches(uint32_t address, const char *wanted)
{
    if (!address) return 0;
    const char *tag = (const char *)address;
    uint32_t i = 0;
    while (wanted[i] && i < 32u && tag[i] == wanted[i]) i++;
    return wanted[i] == '\0' && i < 32u && (tag[i] == '\0' || tag[i] == ' ');
}

static void record_module(struct shell_module *selected,
                          const struct multiboot_module *module)
{
    if (selected->state != 0) { selected->state = -1; return; }
    if (module->mod_end <= module->mod_start) { selected->state = -1; return; }
    selected->start = module->mod_start;
    selected->size = module->mod_end - module->mod_start;
    selected->state = 1;
}

static void discover_modules(const struct multiboot_info *mbi)
{
    main_module = (struct shell_module){0, 0, 0};
    fault_module = (struct shell_module){0, 0, 0};
    if (!mbi || !(mbi->flags & MULTIBOOT_INFO_MODS) || !mbi->mods_count)
        return;
    const struct multiboot_module *modules =
        (const struct multiboot_module *)mbi->mods_addr;
    for (uint32_t i = 0; i < mbi->mods_count; i++) {
        if (module_tag_matches(modules[i].string, MODULE_MAIN_TAG))
            record_module(&main_module, &modules[i]);
        if (module_tag_matches(modules[i].string, MODULE_FAULT_TAG))
            record_module(&fault_module, &modules[i]);
    }
    if (main_module.state == 1) {
        elf32_plan plan;
        executable_ready = elf32_validate_image(
            (const uint8_t *)main_module.start, main_module.size, &plan);
    }
}

static int equal(const char *left, const char *right)
{
    uint32_t i = 0;
    while (left[i] && right[i] && left[i] == right[i]) i++;
    return left[i] == '\0' && right[i] == '\0';
}

static void command_help(void)
{
    puts("help      show commands\n");
    puts("clear     clear the terminal\n");
    puts("echo      print text\n");
    puts("about     show system information\n");
    puts("meminfo   show physical memory information\n");
    puts("run       run the tagged ELF test module (run fault selects fault test)\n");
    puts("features  list implemented kernel capabilities\n");
    puts("version   show the sivajiOS version\n");
}

static void command_about(void)
{
    puts("Operating System : sivajiOS\n");
    puts("Creator           : Saransh Sapra\n");
    puts("Architecture      : x86 32-bit\n");
    puts("Bootloader        : GRUB (Multiboot 1)\n");
    puts("Languages         : C and NASM\n");
    puts("Build System      : GNU Make\n");
    puts("Emulator          : QEMU\n\n");
    puts("Description:\n");
    puts("A minimal operating system developed from scratch, featuring\n");
    puts("protected memory, paging, kernel heap, interrupt handling,\n");
    puts("user-mode execution and system calls.\n");
}

static void command_meminfo(void)
{
    pmm_stats_t stats;
    if (!pmm_get_stats(&stats)) {
        puts("Physical memory statistics: unavailable.\n");
        return;
    }
    puts("Physical Memory Information\n---------------------------\n");
    puts("Tracked address space : ");
    print_u64(stats.tracked_address_space_bytes / (1024u * 1024u));
    puts(" MiB\nDetected usable RAM   : ");
    print_mib(stats.usable_ram_bytes); puts("\n\n");
    puts("Physical Memory Manager\n-----------------------\n");
    puts("Managed frames        : "); print_u32(stats.managed_frames); puts("\n");
    puts("Usable frames         : "); print_u32(stats.usable_frames); puts("\n");
    puts("Free frames           : "); print_u32(stats.free_frames); puts("\n");
    puts("Reserved/unavailable  : "); print_u32(stats.reserved_frames); puts("\n\n");
    puts("Memory Availability\n-------------------\n");
    puts("Free physical memory  : ");
    print_mib((uint64_t)stats.free_frames * PMM_FRAME_SIZE); puts("\n");
}

static void command_features(void)
{
    puts("Boot and Kernel\n- GRUB Multiboot 1 boot; higher-half kernel\n");
    puts("- GDT, IDT, interrupt handling, and 32-bit TSS\n\n");
    puts("Memory Management\n- Paging and page-fault handling\n");
    puts("- Physical frame allocator and kernel heap\n");
    puts("- Heap allocation, deallocation, and reallocation\n\n");
    puts("Drivers\n- Framebuffer terminal, keyboard, and serial output\n");
    puts("- PIC interrupt controller\n\n");
    puts("User Mode\n- Ring 3 execution and user/kernel memory isolation\n");
    puts("- TSS kernel stack switching; system calls and user-pointer checks\n\n");
    puts("Executable Loading\n- ELF32 i386 validation and PT_LOAD segments\n");
    puts("- BSS zeroing, user stack, execution, and cleanup\n");
    puts("Single-context educational OS: no scheduler, filesystem, networking,\n");
    puts("or dynamic linking.\n");
}

static void startup_status(const char *name, int ready)
{
    puts(ready ? "[OK] " : "[--] "); puts(name); puts("\n");
}

static void command_version(void)
{
    puts("sivajiOS version "); puts(SIVAJIOS_VERSION); puts(" (x86 32-bit BIOS)\n");
}

static void command_run(const char *arguments)
{
    struct shell_module *module;
    if (!arguments[0]) module = &main_module;
    else if (equal(arguments, "fault")) module = &fault_module;
    else { puts("Usage: run [fault]\n"); return; }

    if (module->state == 0) {
        puts("run: requested tagged ELF module was not supplied by GRUB.\n");
        return;
    }
    if (module->state < 0) {
        puts("run: module tag is ambiguous or has invalid bounds.\n");
        return;
    }
    puts(arguments[0] ? "Running tagged ELF fault test...\n" :
                        "Running tagged ELF user program...\n");
    if (elf32_run_module(module->start, module->size))
        puts("run: program exited successfully.\n");
    else
        puts("run: program faulted or exited with a nonzero status.\n");
}

static void execute_command(char *line)
{
    char *command = line;
    while (*command == ' ') command++;
    if (!*command) return;
    char *arguments = command;
    while (*arguments && *arguments != ' ') arguments++;
    if (*arguments) {
        *arguments++ = '\0';
        while (*arguments == ' ') arguments++;
    }

    if (equal(command, "help")) command_help();
    else if (equal(command, "clear")) fb_clear(FB_WHITE, FB_BLACK);
    else if (equal(command, "echo")) {
        puts(arguments);
        puts("\n");
    } else if (equal(command, "about")) command_about();
    else if (equal(command, "meminfo")) command_meminfo();
    else if (equal(command, "features")) command_features();
    else if (equal(command, "version")) command_version();
    else if (equal(command, "zeus") && !arguments[0])
        puts("zoos!?\nJOOOOOOOOOOOOOOOOOOOOOOS\n");
    else if (equal(command, "run")) command_run(arguments);
    else { puts("Unknown command: "); puts(command); puts("\nType help for commands.\n"); }
}

static void show_prompt(void) { puts("sivajiOS> "); }

static void submit_line(void)
{
    puts("\n");
    if (input_overflow) puts("Command rejected: maximum length is 127 characters.\n");
    else {
        input[input_length] = '\0';
        execute_command(input);
    }
    input_length = 0;
    input_overflow = 0;
    input[0] = '\0';
    show_prompt();
}

static void consume_event(int event)
{
    if (event == KEYBOARD_EVENT_ENTER) { submit_line(); return; }
    if (event == KEYBOARD_EVENT_BACKSPACE) {
        if (input_length) {
            input_length--;
            input[input_length] = '\0';
            fb_backspace();
            (void)serial_write(COM1, "\b \b", 3);
        }
        return;
    }
    if (event < 32 || event > 126) return;
    if (input_length + 1u >= INPUT_CAPACITY) {
        input_overflow = 1;
        return;
    }
    input[input_length++] = (char)event;
    input[input_length] = '\0';
    char c = (char)event;
    write_bytes(&c, 1);
}

int shell_run(const struct multiboot_info *mbi, int paging_ready,
              int framebuffer_ready, int idt_ready, int tss_ready,
              int pmm_ready, int heap_ready, int keyboard_ready)
{
    discover_modules(mbi);
    fb_clear(FB_WHITE, FB_BLACK);
    puts("================================================\n");
    puts("                    sivajiOS\n");
    puts("          A 32-bit Operating System\n");
    puts("================================================\n\n");
    puts("              Created by Saransh Sapra\n\n");
    puts("       Built from scratch using C and NASM\n\n");
    puts("Initializing system...\n\n");
    startup_status("GDT", 1);
    startup_status("IDT", idt_ready);
    startup_status("Paging", paging_ready);
    startup_status("Physical Memory Manager", pmm_ready);
    startup_status("Kernel Heap", heap_ready);
    startup_status("Framebuffer", framebuffer_ready);
    startup_status("Keyboard", keyboard_ready);
    startup_status("System Calls", idt_ready);
    startup_status("Ring 3", idt_ready && tss_ready);
    startup_status("ELF32 Loader", executable_ready);
    puts("\nSystem initialization complete.\n\n");
    puts("Type 'help' to explore sivajiOS.\n\n");
    show_prompt();
    for (;;) {
        int event = keyboard_read_event();
        if (event == KEYBOARD_EVENT_NONE) {
            /* Recheck with IRQs disabled. An IRQ arriving after this check
             * remains pending until the atomic STI;HLT pair wakes the CPU. */
            __asm__ volatile ("cli" ::: "memory");
            event = keyboard_read_event();
            if (event == KEYBOARD_EVENT_NONE) {
                __asm__ volatile ("sti; hlt" ::: "memory");
                continue;
            }
            __asm__ volatile ("sti" ::: "memory");
        }
        consume_event(event);
    }
}
