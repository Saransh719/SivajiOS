ENTRY(loader)

KERNEL_BASE = 0xC0000000;

PHDRS
{
    boot_text PT_LOAD FLAGS(5);
    boot_data PT_LOAD FLAGS(6);
    text   PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data   PT_LOAD FLAGS(6);
}

SECTIONS
{
    /* GRUB loads every PT_LOAD at p_paddr. Keep the entry and Multiboot
     * header in this physical-address bootstrap region. */
    . = 0x00100000;
    _kernel_phys_start = .;

    .boot ALIGN(0x1000) :
    {
        KEEP(*(.multiboot))
        *(.boot.text*)
        *(.boot.rodata*)
        *(.boot.data*)
    } :boot_text

    .boot.bss ALIGN(0x1000) (NOLOAD) :
    {
        *(.boot.bss*)
    } :boot_data

    /* Place higher-half VMAs at C0000000 + their physical LMAs. Bootstrap
     * storage occupies the first physical pages after 1 MiB, so the first
     * regular kernel section begins a few pages above C0100000. */
    _kernel_phys_load = ALIGN(LOADADDR(.boot.bss) + SIZEOF(.boot.bss), 0x1000);
    . = KERNEL_BASE + _kernel_phys_load;
    _kernel_start = .;

    .text ALIGN(0x1000) : AT(ADDR(.text) - KERNEL_BASE)
    {
        *(.text*)
    } :text

    .rodata ALIGN(0x1000) : AT(ADDR(.rodata) - KERNEL_BASE)
    {
        *(.rodata*)
        *(.userprog)
    } :rodata

    .data ALIGN(0x1000) : AT(ADDR(.data) - KERNEL_BASE)
    {
        *(.data*)
    } :data

    .bss ALIGN(0x1000) (NOLOAD) : AT(ADDR(.bss) - KERNEL_BASE)
    {
        *(COMMON)
        *(.bss*)
    } :data

    _kernel_end = .;
    _kernel_phys_end = _kernel_end - KERNEL_BASE;

    ASSERT(_kernel_phys_end <= 0x01000000,
           "kernel image exceeds the 16 MiB bootstrap higher-half mapping")

    /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) }
}
