ENTRY(_start)
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); }
SECTIONS {
  . = 0x40000000;
  .text : { *(.text*) } :text
  .rodata ALIGN(16) : { *(.rodata*) } :text
  .data ALIGN(4096) : { *(.data*) } :data
  .bss : { *(.bss*) *(COMMON) } :data
  /DISCARD/ : { *(.comment*) *(.eh_frame*) *(.note*) }
}
