#ifndef EXEC_ELF_VALIDATE_H
#define EXEC_ELF_VALIDATE_H

#include "../multiboot.h"

#define ELF32_MAX_PROGRAM_HEADERS 32u
#define ELF32_MAX_IMAGE_PAGES 4096u
#define ELF32_STACK_BASE 0xBFFFC000u
#define ELF32_STACK_PAGES 4u

typedef struct __attribute__((packed)) {
    unsigned char ident[16];
    uint16_t type, machine;
    uint32_t version, entry, phoff, shoff, flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} elf32_header;

typedef struct __attribute__((packed)) {
    uint32_t type, offset, vaddr, paddr, filesz, memsz, flags, align;
} elf32_program;

typedef struct {
    uint32_t offset, vaddr, filesz, memsz, flags;
} elf32_load_segment;

typedef struct {
    uint32_t entry, load_count, page_count;
    elf32_load_segment loads[ELF32_MAX_PROGRAM_HEADERS];
} elf32_plan;

/* Pure preflight: does not map memory or allocate frames. */
int elf32_validate_image(const uint8_t *image, uint32_t size, elf32_plan *plan);

#endif
