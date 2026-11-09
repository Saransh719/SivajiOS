# OS_DEV build and boot

The project builds two independent x86 targets:

- The existing 32-bit Multiboot 1 kernel boots through GRUB on BIOS.
- The x86-64 kernel is a UEFI PE32+ application. GRUB's EFI chainloader starts
  it directly in firmware-provided long mode, and its framebuffer driver uses
  the UEFI GOP interface.

Build the BIOS bootable ISO and run it with QEMU:

    make
    make run

Build and run the ISO through UEFI with QEMU and OVMF:

    make
    make run-uefi

The first UEFI run copies /usr/share/edk2/x64/OVMF_VARS.4m.fd to
build/uefi/OVMF_VARS.fd if the project-local copy does not exist. Later runs
reuse that copy. The firmware code image is read-only.

make uefi builds only the 64-bit UEFI application. make clean removes build
outputs and generated kernel copies from the ISO staging directory.

Both GRUB menu entries are available under UEFI. BIOS GRUB shows its 32-bit
Multiboot entry.
