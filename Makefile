
CC = gcc
AS = nasm
LD = ld
BUILD_DIR = build
BIOS_DIR = $(BUILD_DIR)/bios
UEFI_DIR = $(BUILD_DIR)/uefi
MODULE_SRC = modules/program.s
MODULE_BIN = iso/modules/program

CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin \
         -fno-stack-protector -nostartfiles \
         -nodefaultlibs -Wall -Wextra -Werror -c

ASFLAGS = -f elf32
LDFLAGS = -T link.ld -melf_i386

BIOS_OBJECTS = $(BIOS_DIR)/loader.o \
               $(BIOS_DIR)/kmain.o \
               $(BIOS_DIR)/gdt.o \
               $(BIOS_DIR)/gdt_flush.o \
               $(BIOS_DIR)/idt.o \
               $(BIOS_DIR)/idt_flush.o \
               $(BIOS_DIR)/interrupt.o \
               $(BIOS_DIR)/interrupt_c.o \
               $(BIOS_DIR)/framebuffer.o \
               $(BIOS_DIR)/io.o \
               $(BIOS_DIR)/serial.o \
			   $(BIOS_DIR)/pic.o \
			   $(BIOS_DIR)/keyboard.o \
               $(BIOS_DIR)/paging.o \
               $(BIOS_DIR)/pmm.o \
               $(BIOS_DIR)/paging_enable.o \


UEFI_OBJECTS = $(UEFI_DIR)/entry.o $(UEFI_DIR)/kmain.o $(UEFI_DIR)/framebuffer.o

.PHONY: all clean run run-uefi

# Build the complete bootable ISO
all: os.iso

# Compile assembly
$(BIOS_DIR)/loader.o: loader.s
	mkdir -p $(BIOS_DIR)
	$(AS) $(ASFLAGS) $< -o $@

# Compile kernel
$(BIOS_DIR)/kmain.o: kmain.c drivers/framebuffer.h mm/paging.h mm/pmm.h gdt.h keyboard.h
	mkdir -p $(BIOS_DIR)
	$(CC) $(CFLAGS) $< -o $@

# Compile GDT implementation
$(BIOS_DIR)/gdt.o: gdt.c gdt.h
	mkdir -p $(BIOS_DIR)
	$(CC) $(CFLAGS) $< -o $@

# Compile GDT assembly
$(BIOS_DIR)/gdt_flush.o: gdt_flush.s
	mkdir -p $(BIOS_DIR)
	$(AS) $(ASFLAGS) $< -o $@

# Compile framebuffer driver
$(BIOS_DIR)/framebuffer.o: drivers/framebuffer.c drivers/framebuffer.h io/io.h mm/paging.h
	mkdir -p $(BIOS_DIR)
	$(CC) $(CFLAGS) $< -o $@

# Compile I/O assembly
$(BIOS_DIR)/io.o: io/io.s
	mkdir -p $(BIOS_DIR)
	$(AS) $(ASFLAGS) $< -o $@

# Compile serial driver
$(BIOS_DIR)/serial.o: drivers/serial.c drivers/serial.h io/io.h
	mkdir -p $(BIOS_DIR)
	$(CC) $(CFLAGS) $< -o $@

# Compile IDT implementation
$(BIOS_DIR)/idt.o: idt.c idt.h
	mkdir -p $(BIOS_DIR)
	$(CC) $(CFLAGS) $< -o $@

# Compile IDT assembly
$(BIOS_DIR)/idt_flush.o: idt_flush.s
	mkdir -p $(BIOS_DIR)
	$(AS) $(ASFLAGS) $< -o $@

# Compile interrupt assembly handlers
$(BIOS_DIR)/interrupt.o: interrupt.s
	mkdir -p $(BIOS_DIR)
	$(AS) $(ASFLAGS) $< -o $@

# Compile C interrupt handler
$(BIOS_DIR)/interrupt_c.o: interrupt.c interrupt.h
	mkdir -p $(BIOS_DIR)
	$(CC) $(CFLAGS) $< -o $@

# Compile PIC driver
$(BIOS_DIR)/pic.o: pic.c pic.h io/io.h
	mkdir -p $(BIOS_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIOS_DIR)/keyboard.o: keyboard.c keyboard.h keyboard_map.h io/io.h drivers/framebuffer.h
	mkdir -p $(BIOS_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIOS_DIR)/paging.o: mm/paging.c mm/paging.h multiboot.h drivers/serial.h
	mkdir -p $(BIOS_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIOS_DIR)/paging_enable.o: mm/paging_enable.s
	mkdir -p $(BIOS_DIR)
	$(AS) $(ASFLAGS) $< -o $@

$(BIOS_DIR)/pmm.o: mm/pmm.c mm/pmm.h mm/paging.h multiboot.h drivers/serial.h
	mkdir -p $(BIOS_DIR)
	$(CC) $(CFLAGS) $< -o $@

# Link kernel
kernel.elf: $(BIOS_OBJECTS) link.ld
	$(LD) $(LDFLAGS) $(BIOS_OBJECTS) -o $@

$(UEFI_DIR)/entry.o: uefi/entry.s
	mkdir -p $(UEFI_DIR)
	$(AS) -f elf64 $< -o $@

$(UEFI_DIR)/kmain.o: uefi/kmain.c uefi/uefi.h drivers/framebuffer.h
	mkdir -p $(UEFI_DIR)
	$(CC) -m64 -mcmodel=large -std=c11 -ffreestanding -fno-stack-protector -fno-pic \
		-fno-builtin -fno-asynchronous-unwind-tables -mno-red-zone \
		-Wall -Wextra -Werror -c $< -o $@

$(UEFI_DIR)/framebuffer.o: drivers/framebuffer64.c drivers/framebuffer.h uefi/uefi.h
	mkdir -p $(UEFI_DIR)
	$(CC) -m64 -mcmodel=large -std=c11 -ffreestanding -fno-stack-protector -fno-pic \
		-fno-builtin -fno-asynchronous-unwind-tables -mno-red-zone \
		-Wall -Wextra -Werror -c $< -o $@

$(UEFI_DIR)/BOOTX64.EFI: $(UEFI_OBJECTS) uefi/link.ld
	$(LD) -mi386pep --subsystem 10 --entry efi_entry -T uefi/link.ld \
		$(UEFI_OBJECTS) -o $@

uefi: $(UEFI_DIR)/BOOTX64.EFI

# Compile external program as a flat binary
$(MODULE_BIN): $(MODULE_SRC)
	mkdir -p iso/modules
	$(AS) -f bin $< -o $@

# Generate BIOS + UEFI bootable ISO
os.iso: kernel.elf $(UEFI_DIR)/BOOTX64.EFI $(MODULE_BIN) iso/boot/grub/grub.cfg
	mkdir -p iso/boot/grub
	cp kernel.elf iso/boot/kernel.elf
	mkdir -p iso/EFI/BOOT
	cp $(UEFI_DIR)/BOOTX64.EFI iso/EFI/BOOT/BOOTX64.EFI
	grub-mkrescue -o os.iso iso

# Launch QEMU
run: os.iso
	qemu-system-i386 -monitor stdio -serial file:com1.out \
	                  -cdrom os.iso \
	                  -boot order=d -m 32 \
	                  -display gtk -no-reboot -no-shutdown

run-uefi: os.iso
	@if [ ! -f $(UEFI_DIR)/OVMF_VARS.fd ]; then \
		cp /usr/share/edk2/x64/OVMF_VARS.4m.fd $(UEFI_DIR)/OVMF_VARS.fd; \
	fi
	qemu-system-x86_64 -monitor stdio -cdrom os.iso \
		-m 256 \
		-display gtk -no-reboot -no-shutdown \
		-drive if=pflash,format=raw,unit=0,readonly=on,file=/usr/share/edk2/x64/OVMF_CODE.4m.fd \
		-drive if=pflash,format=raw,unit=1,file=$(UEFI_DIR)/OVMF_VARS.fd

# Clean generated files
clean:
	rm -rf $(BUILD_DIR)
	rm -f loader.o kmain.o drivers/framebuffer.o kernel.elf os.iso
	rm -f iso/boot/kernel.elf iso/EFI/BOOT/BOOTX64.EFI
	rm -f $(MODULE_BIN)

.PHONY: uefi
