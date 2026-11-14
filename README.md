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
virtual addresses. The low identity mapping is retained for boot structures,
modules, boot page tables, and legacy physical pointers; it remains
supervisor-only.

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

The kernel heap uses the lazy virtual range `[0xD0000000, 0xD1000000)`, between
the kernel's `0xC...` mappings and framebuffer alias at `0xE0000000`. `heap_init`
checks the range is unmapped but does not pre-map it. `kmalloc` grows it on
demand by requesting PMM physical frames and mapping each supervisor-writable
frame with the paging API. Physical addresses are never dereferenced directly.
Allocation headers and the doubly linked free list are inside mapped heap
pages; pointers are validated by walking known blocks. Blocks split on
allocation and adjacent free blocks coalesce. Reallocation can grow in place
or allocate/copy/free. Mapped heap pages are retained after `kfree` for reuse.

The “Heap allocator test” GRUB entry checks small and multi-page allocations,
alignment, reads/writes, reuse, splitting, coalescing, realloc, invalid and
duplicate frees, and an oversized request. It prints each result over COM1
and halts. A failed physical allocation or page mapping unwinds frames mapped
during that growth attempt. The 16 MiB heap ceiling, static paging-table pool,
single-threaded assumptions, and lack of heap-page reclamation are current
limitations; allocation failure is returned as `NULL`.

## BIOS Ring 3 and TSS

The BIOS GDT selectors are kernel code `0x08` (DPL 0), kernel data `0x10`
(DPL 0), user code `0x1B` (descriptor `0x18`, RPL 3), user data `0x23`
(descriptor `0x20`, RPL 3), and TSS `0x28`. `tss.c` installs a packed
104-byte 32-bit TSS with `ss0=0x10` and a dedicated 16 KiB higher-half Ring 0
interrupt stack as `esp0`; `ltr 0x28` loads it and the CPU marks the descriptor
busy. There is no I/O bitmap, and user EFLAGS keep IOPL at zero.

The isolated test address space uses user code at `0x40000000` (user,
read-only PTE), user data at `0x40001000` (user, writable PTE), and one user
stack page at `0xBFFFF000` with initial ESP `0xC0000000`. These sit below the
kernel split and do not overlap the kernel, heap (`0xD0000000` range),
framebuffer alias (`0xE0000000` range), or page-table storage. Each page is
backed by a PMM frame and mapped through `paging_map_page`; failed setup rolls
back mapped pages and frames. Physical frame addresses are not dereferenced.
The mapper sets the PDE user bit for those page tables while user PTEs carry
their own permissions; kernel PTEs remain supervisor-only. The test pages are
unmapped and returned to PMM after the user context exits.

The IDT exposes only vector `0x80` as a DPL 3 gate. The interrupt stub saves
the interrupted data selectors and loads kernel selectors before calling C.
On user entry, hardware obtains the kernel stack from the TSS. The syscall
handler changes the saved `iret` frame to a Ring 0 trampoline, which restores
the suspended kernel stack. User exceptions are reported and returned to the
test caller; kernel exceptions retain their fatal behavior. The separate
memory-protection GRUB test reads a mapped supervisor-only kernel text page
and expects a present, user-mode page fault (`CR2=0xC0118000`, error bits
`0x5`).

Ring 3 is currently a fixed diagnostic context, not a process model. There is
no ELF loader, process-level syscall ABI, per-process page directory, or
scheduler. User mapping addresses and the existing static page-table pool are
fixed; kernel low identity mappings remain in place.

## BIOS system-call ABI

Ring 3 calls the sole user-callable IDT gate with `int 0x80`. The register ABI
is `EAX=number`, `EBX=argument 1`, `ECX=argument 2`, `EDX=argument 3`, with a
signed 32-bit result in EAX. `SYS_WRITE` (1) takes a user buffer in EBX and a
byte count in ECX, writes to COM1, and returns the count. A zero count returns
zero. Writes above 4096 bytes return `-E2BIG` (`-7`); invalid or inaccessible
user ranges return `-EFAULT` (`-14`). `SYS_EXIT` (2) takes its status in EBX
and returns through the existing Ring 0 trampoline. Unknown numbers return
`-ENOSYS` (`-38`) without leaving user mode.
Syscall numbers and error constants come from `syscall_abi.h`, shared by the C
dispatcher and preprocessed NASM test program.

`validate_user_range`, `copy_from_user`, and `copy_to_user` check 32-bit range
overflow, reject addresses at or above `0xC0000000`, and inspect effective
permissions from both PDE and PTE. Read access requires present user mappings;
write access additionally requires writable bits in both levels. The syscall
dispatcher validates the entire `sys_write` range before copying in bounded
chunks, so kernel or unmapped pointers do not reach the serial driver. The
syscall diagnostic checks a valid code/data buffer, unknown-call handling, a
supervisor kernel pointer, an address range crossing the user/kernel boundary,
an unmapped user address, 32-bit pointer-plus-length wraparound, and a rejected
`copy_to_user` against a kernel guard.

This is still a fixed single-user test environment. There is no process
management, descriptor table, filesystem, general console routing, or ELF
loader. `sys_exit` returns to the one suspended Ring 0 caller; it does not
schedule another task.
