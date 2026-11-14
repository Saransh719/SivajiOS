
CC = gcc
AS = nasm
LD = ld
BUILD_DIR = build
BIOS_DIR = $(BUILD_DIR)/bios
UEFI_DIR = $(BUILD_DIR)/uefi
MODULE_SRC = modules/program.s
MODULE_BIN = iso/modules/program
USER_ELF = iso/modules/user.elf
USER_FAULT_ELF = iso/modules/user-fault.elf

CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin \
         -fno-stack-protector -nostartfiles \
         -nodefaultlibs -Wall -Wextra -Werror -c

ASFLAGS = -f elf32
LDFLAGS = -T link.ld -melf_i386

BIOS_OBJECTS = $(BIOS_DIR)/loader.o \
               $(BIOS_DIR)/kmain.o \
               $(BIOS_DIR)/gdt.o \
               $(BIOS_DIR)/tss.o \
               $(BIOS_DIR)/usermode.o \
               $(BIOS_DIR)/elf.o \
               $(BIOS_DIR)/elf_validate.o \
               $(BIOS_DIR)/shell.o \
               $(BIOS_DIR)/usermem.o \
               $(BIOS_DIR)/syscall.o \
               $(BIOS_DIR)/usermode_asm.o \
               $(BIOS_DIR)/ring3_test.o \
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
               $(BIOS_DIR)/heap.o \
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
$(BIOS_DIR)/kmain.o: kmain.c drivers/framebuffer.h mm/paging.h mm/pmm.h mm/heap.h gdt.h tss.h usermode.h keyboard.h exec/elf.h shell/shell.h idt.h
	mkdir -p $(BIOS_DIR)
	$(CC) $(CFLAGS) $< -o $@

# Compile GDT implementation
$(BIOS_DIR)/gdt.o: gdt.c gdt.h
	mkdir -p $(BIOS_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIOS_DIR)/tss.o: tss.c tss.h gdt.h multiboot.h
	mkdir -p $(BIOS_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIOS_DIR)/usermode.o: usermode.c usermode.h usermem.h syscall.h syscall_abi.h interrupt.h gdt.h mm/paging.h mm/pmm.h drivers/serial.h
	mkdir -p $(BIOS_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIOS_DIR)/elf.o: exec/elf.c exec/elf.h exec/elf_validate.h usermode.h mm/paging.h mm/pmm.h mm/heap.h
	mkdir -p $(BIOS_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIOS_DIR)/elf_validate.o: exec/elf_validate.c exec/elf_validate.h multiboot.h
	mkdir -p $(BIOS_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIOS_DIR)/shell.o: shell/shell.c shell/shell.h keyboard.h drivers/framebuffer.h mm/pmm.h mm/heap.h exec/elf.h exec/elf_validate.h version.h
	mkdir -p $(BIOS_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIOS_DIR)/usermem.o: usermem.c usermem.h mm/paging.h multiboot.h
	mkdir -p $(BIOS_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIOS_DIR)/syscall.o: syscall.c syscall.h syscall_abi.h usermem.h interrupt.h multiboot.h drivers/serial.h drivers/framebuffer.h
	mkdir -p $(BIOS_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIOS_DIR)/usermode_asm.o: usermode.s
	mkdir -p $(BIOS_DIR)
	$(AS) $(ASFLAGS) $< -o $@

$(BIOS_DIR)/ring3_test.o: ring3_test.S syscall_abi.h
	mkdir -p $(BIOS_DIR)
	$(CC) -E -P -x assembler-with-cpp $< -o $(BIOS_DIR)/ring3_test.i
	$(AS) $(ASFLAGS) $(BIOS_DIR)/ring3_test.i -o $@

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
$(BIOS_DIR)/interrupt_c.o: interrupt.c interrupt.h usermode.h mm/paging.h
	mkdir -p $(BIOS_DIR)
	$(CC) $(CFLAGS) $< -o $@

# Compile PIC driver
$(BIOS_DIR)/pic.o: pic.c pic.h io/io.h
	mkdir -p $(BIOS_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIOS_DIR)/keyboard.o: keyboard.c keyboard.h keyboard_map.h io/io.h
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

$(BIOS_DIR)/heap.o: mm/heap.c mm/heap.h mm/paging.h mm/pmm.h drivers/serial.h
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

# ELF user application; no libc, interpreter, or hosted startup.
$(BUILD_DIR)/user.o: user/program.c syscall_abi.h
	mkdir -p $(BUILD_DIR)
	$(CC) -m32 -std=c11 -ffreestanding -fno-pic -fno-pie -fno-stack-protector \
		-fno-builtin -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -c $< -o $@

$(BUILD_DIR)/user_entry.o: user/entry.s
	mkdir -p $(BUILD_DIR)
	$(AS) -f elf32 $< -o $@

$(USER_ELF): $(BUILD_DIR)/user.o $(BUILD_DIR)/user_entry.o user/link.ld
	mkdir -p iso/modules
	$(LD) -melf_i386 -static -T user/link.ld $(BUILD_DIR)/user_entry.o $(BUILD_DIR)/user.o -o $@

$(BUILD_DIR)/user_fault_entry.o: user/fault_entry.s
	mkdir -p $(BUILD_DIR)
	$(AS) -f elf32 $< -o $@

$(USER_FAULT_ELF): $(BUILD_DIR)/user_fault_entry.o user/fault_link.ld
	mkdir -p iso/modules
	$(LD) -melf_i386 -static -T user/fault_link.ld $(BUILD_DIR)/user_fault_entry.o -o $@

# Generate BIOS + UEFI bootable ISO
os.iso: kernel.elf $(UEFI_DIR)/BOOTX64.EFI $(MODULE_BIN) $(USER_ELF) $(USER_FAULT_ELF) iso/boot/grub/grub.cfg
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
	rm -f $(USER_ELF)
	rm -f $(USER_FAULT_ELF)

.PHONY: uefi

.PHONY: test-elf-validator
test-elf-validator:
	mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -Wall -Wextra -Werror tests/elf_validate_test.c exec/elf_validate.c -o $(BUILD_DIR)/elf_validate_test
	$(BUILD_DIR)/elf_validate_test
