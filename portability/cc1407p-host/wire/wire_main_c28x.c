/* C28x entry for the wire packer test: watchdog off, run the test, park at wire_halt for the debugger. */
#include <stdint.h>
extern void wire_run(void);
void wire_halt(void) { }
int main(void)
{
    __asm(" EALLOW");
    *(volatile uint16_t *)0x7029u = 0x68u;   /* WDCR: watchdog off, as SysCtl_disableWatchdog() does */
    __asm(" EDIS");
    wire_run();
    wire_halt();
    for (;;) { }
}
