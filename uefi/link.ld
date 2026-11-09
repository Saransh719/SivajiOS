OUTPUT_FORMAT(pei-x86-64)
ENTRY(efi_entry)
SECTIONS
{
    . = __image_base__ + 0x1000;
    .text : { *(.text*) }
    .rdata : { *(.rdata*) *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) *(COMMON) }
    /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) }
}
