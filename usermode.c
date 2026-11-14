#include "usermode.h"
#include "gdt.h"
#include "mm/paging.h"
#include "mm/pmm.h"
#include "drivers/serial.h"
#include "syscall.h"
#include "usermem.h"

#define COM1 0x3F8u
#define USER_CODE  0x40000000u
#define USER_DATA  0x40001000u
#define USER_STACK 0xBFFFF000u
#define USER_TOP   0xC0000000u
#define USER_KERNEL_PROBE 0xC0118000u
#define USER_PAGE_FLAGS (PAGING_PRESENT | PAGING_USER)

extern uint8_t user_test_start[];
extern uint8_t user_test_end[];
extern uint8_t user_protection_entry[];
extern uint8_t user_syscall_entry[];
extern void enter_user_mode(uint32_t entry, uint32_t user_stack);
extern void ring3_return_trampoline(void);

static int user_active;
static int protection_test_active;
static int user_exit_reason;
static uint32_t user_exit_status;
static uint32_t user_saved_cpl;

enum { EXIT_NONE, EXIT_SYSCALL, EXIT_EXPECTED_PROTECTION, EXIT_UNEXPECTED };
static volatile uint32_t syscall_kernel_guard = 0x51AFC0DEu;

static void serial_text(const char *text)
{
    unsigned int length = 0;
    while (text[length]) length++;
    serial_write(COM1, text, length);
}

static void serial_hex(uint32_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    char text[8];
    for (unsigned int i = 0; i < 8; i++)
        text[i] = digits[(value >> ((7u - i) * 4u)) & 15u];
    serial_write(COM1, text, sizeof(text));
}

/* The sole callable user gate and expected user faults return through iret
 * into this Ring 0 trampoline. It restores the suspended C stack saved by
 * enter_user_mode; never resume Ring 3 by jumping directly to kernel code. */
static void prepare_kernel_return(struct cpu_state *state)
{
    uint32_t *frame = (uint32_t *)state;
    frame[10] = (uint32_t)ring3_return_trampoline;
    frame[11] = GDT_KERNEL_CODE_SELECTOR;
    /* Keep interrupts masked on the TSS stack until the trampoline restores
     * the suspended Ring 0 stack and its original EFLAGS. */
    frame[12] = 0x2u;
    user_active = 0;
}

int usermode_handle_interrupt(struct cpu_state *state, unsigned int vector,
                              uint32_t error, uint32_t fault_address)
{
    uint32_t *frame = (uint32_t *)state;
    uint32_t cs = frame[11];
    if (!user_active || (cs & 3u) != 3u) return 0;
    user_saved_cpl = cs & 3u;

    if (vector == 0x80u) {
        uint32_t exit_status = 0;
        if (syscall_dispatch(state, &exit_status)) {
            user_exit_status = exit_status;
            user_exit_reason = EXIT_SYSCALL;
            serial_text("Ring 3 sys_exit: saved CPL=3, status=0x");
            serial_hex(user_exit_status);
            serial_text("\n");
            prepare_kernel_return(state);
        }
        return 1;
    }

    if (vector == 14u) {
        serial_text("User page fault CR2=0x");
        serial_hex(fault_address);
        serial_text(" error=0x");
        serial_hex(error);
        serial_text("\n");
        if (protection_test_active && fault_address == USER_KERNEL_PROBE &&
            (error & 7u) == 5u) {
            user_exit_reason = EXIT_EXPECTED_PROTECTION;
            user_exit_status = 0;
            serial_text("Ring 3 supervisor-page protection: PASS\n");
        } else {
            user_exit_reason = EXIT_UNEXPECTED;
            user_exit_status = vector;
            serial_text("Ring 3 unexpected page fault; returning to kernel.\n");
        }
        prepare_kernel_return(state);
        return 1;
    }

    user_exit_reason = EXIT_UNEXPECTED;
    user_exit_status = vector;
    serial_text("Ring 3 exception vector ");
    serial_hex(vector);
    serial_text("; returning to kernel.\n");
    prepare_kernel_return(state);
    return 1;
}

static void release_user_pages(const uint32_t frames[3], unsigned int mapped)
{
    static const uint32_t addresses[3] = { USER_CODE, USER_DATA, USER_STACK };
    while (mapped) {
        unsigned int index = --mapped;
        (void)paging_unmap_page(addresses[index]);
        pmm_free_frame(frames[index]);
    }
}

int usermode_run_test(int test_kind)
{
    static const uint32_t addresses[3] = { USER_CODE, USER_DATA, USER_STACK };
    uint32_t frames[3] = { PMM_FRAME_FAILURE, PMM_FRAME_FAILURE,
                           PMM_FRAME_FAILURE };
    unsigned int allocated = 0;
    unsigned int mapped = 0;
    uint32_t code_size = (uint32_t)(user_test_end - user_test_start);
    if (!code_size || code_size > PMM_FRAME_SIZE) {
        serial_text("Ring 3 setup FAIL: user image does not fit one page.\n");
        return 0;
    }

    for (unsigned int i = 0; i < 3; i++) {
        if (paging_get_physical_address(addresses[i], 0)) {
            serial_text("Ring 3 setup FAIL: user range is already mapped.\n");
            return 0;
        }
    }
    for (; allocated < 3; allocated++) {
        frames[allocated] = pmm_alloc_frame();
        if (frames[allocated] == PMM_FRAME_FAILURE) break;
    }
    if (allocated != 3) {
        while (allocated) pmm_free_frame(frames[--allocated]);
        serial_text("Ring 3 setup FAIL: PMM frame allocation failed.\n");
        return 0;
    }

    for (; mapped < 3; mapped++) {
        uint32_t flags = USER_PAGE_FLAGS;
        if (mapped != 0) flags |= PAGING_WRITABLE;
        if (!paging_map_page(addresses[mapped], frames[mapped], flags)) {
            release_user_pages(frames, mapped);
            pmm_free_frame(frames[mapped]);
            serial_text("Ring 3 setup FAIL: user page mapping failed.\n");
            return 0;
        }
    }

    uint8_t *destination = (uint8_t *)USER_CODE;
    for (uint32_t i = 0; i < code_size; i++) destination[i] = user_test_start[i];
    *(volatile uint32_t *)USER_DATA = 0;
    *(volatile uint32_t *)(USER_DATA + 4u) = 0;
    *(volatile uint32_t *)(USER_DATA + 8u) = 0;

    int user_copy_helpers_ok = 1;
    if (test_kind == USERMODE_TEST_SYSCALL) {
        uint32_t expected = 0xA55A3CC3u;
        uint32_t observed = 0;
        user_copy_helpers_ok =
            copy_to_user(USER_DATA + 12u, &expected, sizeof(expected)) &&
            copy_from_user(&observed, USER_DATA + 12u, sizeof(observed)) &&
            observed == expected;
        serial_text(user_copy_helpers_ok ?
            "Valid copy_to_user/copy_from_user: PASS.\n" :
            "Valid copy_to_user/copy_from_user: FAIL.\n");
    }

    user_active = 1;
    protection_test_active = test_kind == USERMODE_TEST_PROTECTION;
    user_exit_reason = EXIT_NONE;
    user_exit_status = 0xFFFFFFFFu;
    user_saved_cpl = 0;
    uint32_t entry = USER_CODE;
    if (test_kind == USERMODE_TEST_PROTECTION)
        entry += (uint32_t)(user_protection_entry - user_test_start);
    else if (test_kind == USERMODE_TEST_SYSCALL)
        entry += (uint32_t)(user_syscall_entry - user_test_start);

    serial_text(test_kind == USERMODE_TEST_PROTECTION ?
        "Entering Ring 3 protection test at 0x" :
        test_kind == USERMODE_TEST_SYSCALL ?
        "Entering Ring 3 syscall test at 0x" :
        "Entering Ring 3 user test at 0x");
    serial_hex(entry);
    serial_text("\n");
    enter_user_mode(entry, USER_TOP);

    int passed;
    if (test_kind == USERMODE_TEST_PROTECTION) {
        passed = user_exit_reason == EXIT_EXPECTED_PROTECTION &&
                 user_saved_cpl == 3u;
        serial_text(passed ?
            "Ring 3 memory protection test: PASS; returned to CPL 0.\n" :
            "Ring 3 memory protection test: FAIL.\n");
    } else if (test_kind == USERMODE_TEST_SYSCALL) {
        uint32_t expected_mask = 0x7Fu;
        uint32_t copy_value = 0xBAD0BAD0u;
        uint32_t guard_before = syscall_kernel_guard;
        int rejected_kernel_write = !copy_to_user(
            (uint32_t)&syscall_kernel_guard, &copy_value, sizeof(copy_value));
        int guard_unchanged = syscall_kernel_guard == guard_before;
        int guard_ok = rejected_kernel_write && guard_unchanged;
        serial_text(guard_ok ?
            "Rejected kernel-destination copy left kernel guard unchanged: PASS.\n" :
            "Rejected kernel-destination copy guard check: FAIL.\n");
        passed = user_exit_reason == EXIT_SYSCALL && user_exit_status == 0 &&
                 user_saved_cpl == 3u &&
                 *(volatile uint32_t *)(USER_DATA + 8u) == expected_mask &&
                 guard_ok && user_copy_helpers_ok;
        serial_text(passed ?
            "Ring 3 syscall validation suite: PASS.\n" :
            "Ring 3 syscall validation suite: FAIL.\n");
    } else {
        passed = user_exit_reason == EXIT_SYSCALL &&
                 user_exit_status == 0 && user_saved_cpl == 3u &&
                 *(volatile uint32_t *)USER_DATA == 0xC0DEF00Du;
        serial_text(passed ?
            "Ring 3 execution and user read/write test: PASS.\n" :
            "Ring 3 execution and user read/write test: FAIL.\n");
        if (passed) serial_text("Safe syscall return to CPL 0: PASS.\n");
    }
    user_active = 0;
    release_user_pages(frames, mapped);
    return passed;
}
