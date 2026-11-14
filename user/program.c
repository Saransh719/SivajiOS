#include "../syscall_abi.h"

volatile unsigned int initialized_state = 0x13579BDFu;
volatile unsigned int zero_state;

static int call3(int number, int a, int b, int c)
{
    int result;
    __asm__ volatile ("int $0x80" : "=a"(result) : "a"(number), "b"(a), "c"(b), "d"(c) : "memory");
    return result;
}

static int write_text(const char *text, unsigned int length)
{ return call3(SYS_WRITE, (int)text, (int)length, 0) == (int)length; }

void user_main(void)
{
    static const char start[]="\n[sivajiOS user program]\n\n";
    static const char ring3[]="Running in Ring 3.\n";
    static const char ring3_failed[]="Ring 3 check failed.\n";
    static const char syscall[]="Testing system call interface... ";
    static const char data_test[]="Testing writable data... ";
    static const char data_passed[]="passed.\n";
    static const char data_failed[]="failed.\n";
    static const char bss_test[]="Testing zero-initialized BSS... ";
    static const char calculation[]="Calculation result: 42\n";
    static const char complete[]="\nUser program completed successfully.\nReturning to kernel...\n";
    unsigned int cs;
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));
    int ring3_ok = cs == 0x1Bu;
    int ok = ring3_ok;
    ok &= write_text(start, sizeof(start)-1u);
    ok &= write_text(ring3_ok ? ring3 : ring3_failed,
                     ring3_ok ? sizeof(ring3)-1u : sizeof(ring3_failed)-1u);
    int syscall_test_ok = write_text(syscall, sizeof(syscall)-1u);
    ok &= syscall_test_ok;
    ok &= write_text(syscall_test_ok ? data_passed : data_failed,
                     syscall_test_ok ? sizeof(data_passed)-1u :
                                        sizeof(data_failed)-1u);
    ok &= write_text(data_test, sizeof(data_test)-1u);
    int data_ok = initialized_state == 0x13579BDFu;
    initialized_state = 0x2468ACE0u;
    data_ok &= initialized_state == 0x2468ACE0u;
    ok &= data_ok;
    ok &= write_text(data_ok ? data_passed : data_failed,
                     data_ok ? sizeof(data_passed)-1u : sizeof(data_failed)-1u);
    ok &= write_text(bss_test, sizeof(bss_test)-1u);
    int bss_ok = zero_state == 0u;
    zero_state = 1u;
    bss_ok &= zero_state == 1u;
    ok &= bss_ok;
    ok &= write_text(bss_ok ? data_passed : data_failed,
                     bss_ok ? sizeof(data_passed)-1u : sizeof(data_failed)-1u);
    volatile unsigned int answer=6u*7u;
    ok &= answer == 42u;
    ok &= write_text(calculation, sizeof(calculation)-1u);
    if (ok) ok &= write_text(complete, sizeof(complete)-1u);
    else (void)write_text(data_failed, sizeof(data_failed)-1u);
    (void)call3(SYS_EXIT,ok ? 0 : 1,0,0);
    for (;;) { }
}
