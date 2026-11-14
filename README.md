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

The UEFI GRUB menu offers its 64-bit chainload entry and a 32-bit Multiboot
entry. BIOS GRUB offers normal boot and the paging diagnostic entries.

The BIOS kernel uses 4 KiB identity and higher-half mappings. The linker keeps
the Multiboot entry/bootstrap at physical `0x00100000`; regular kernel sections
have VMAs at `0xC0000000 + physical address` and LMAs at their physical load
addresses. The bootstrap tables and stack occupy about 96 KiB after the 1 MiB
load point, so the current first regular kernel section is at `0xC0118000`.

GRUB enters the physical `loader` symbol. It saves the Multiboot registers on
an early low stack, then `mm/paging_enable.s` builds temporary identity
`[0, 64 MiB)` and higher-half `[0xC0000000, 0xC1000000)` mappings, loads CR3,
and enables paging once. `loader` explicitly jumps to `higher_half_entry`,
which switches to the high kernel stack and installs the high-address GDT
before entering C. C paging initialization reuses that active directory,
retains identity mappings for Multiboot structures, modules, and VGA. The
framebuffer is mapped at `0xE0000000`, because its common PCI address near
`0xFD000000` overlaps the higher-half kernel virtual region and cannot remain
identity-mapped there.

Multiboot structure and module pointers remain physical and are accessed
through the retained identity map. The framebuffer address in Multiboot data
also remains physical, while the driver accesses its mapping at `0xE0000000`.
Linker symbols and ordinary kernel globals/functions are high virtual
addresses. `mm/paging.c` converts high kernel VMAs to physical addresses by
subtracting `0xC0000000`; boot symbols and GRUB-provided addresses are already
physical and are not adjusted. Its fixed 4 MiB page-table pool remains
temporary storage, separate from the PMM; the bootstrap requires the linked kernel image to fit within its
16 MiB physical alias and expects initial Multiboot information below 64 MiB.
The upper 1 GiB cannot be identity-mapped because it is reserved for high-half
virtual addresses. No heap, Ring 3, or identity-map removal is implemented.

The GRUB entry “Paging fault diagnostic test (destructive)” deliberately reads
an unmapped page and prints the #PF address and error flags over COM1.
“Paging keyboard IRQ test” waits for a key and confirms IRQ1 delivery over
COM1. Normal boot does not run these tests.

Normal BIOS boot prints a sampled kernel EIP before launching the external
module, then prints the module's physical range. The module is a flat binary
loaded by GRUB and intentionally executes through its low identity-mapped
physical address, so a debugger EIP in that range after module launch is
expected. The kernel's high execution is verified separately before launch.

`mm/pmm.c` manages 4 KiB physical frames using bitmaps initialized from the
Multiboot variable-sized memory map. Frames start reserved; complete frames in
type-1 usable entries become free, then non-usable entries and occupied kernel,
boot, Multiboot, module, and framebuffer ranges are reserved. Its two static
bitmaps live in kernel BSS, so the kernel's physical boundary reservation also
covers their storage. PMM addresses are physical; the PMM does not dereference
allocated frames, and frames above the current mappings need a later mapping
mechanism before use by callers. `0xFFFFFFFF` reports allocation failure.

The PMM diagnostic reports both the highest physical address span and the sum
of usable type-1 map ranges. For example, QEMU's 32 MiB BIOS map may include a
reserved address-space range ending at 4 GiB: the span is then 4096 MiB while
usable RAM remains about 31 MiB. The managed-frame count follows the span;
reserved-frame count includes holes and all non-usable frames below that end.

The “Physical memory manager test” GRUB entry runs reversible allocation,
alignment, uniqueness, free, double-free, invalid-free, and reuse checks, then
halts. Exhaustion testing is omitted to avoid retaining an array of every
allocated frame.
