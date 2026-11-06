/* Entry for the M33 build (GPL-2.0, see ../LICENSE). */
#include <stdint.h>
#include "doomgeneric.h"
#ifndef FOX_EMU
#include "ti_msp_dl_config.h"
#include <ti/devices/msp/m33/mspm33c321x.h>
#endif

#ifdef FOX_EMU
#define EMU_DEMO (*(volatile uint32_t *)0x50000008u)   /* the harness says which demo: 1, 2 or 3 */
static char demo_name[] = "demoN";
#define FOX_EMU_DEMO "-timedemo", demo_name,
#elif defined(FOX_ATTRACT)
#define FOX_EMU_DEMO               /* normal start: title screen and the attract-mode demos */
#else
#define FOX_EMU_DEMO "-timedemo", "demo1",   /* benchmark start: demo1 as fast as the chip can go */
#endif

int main(void)
{
    static char *argv[] = { "doom", "-iwad", "doom1.wad", "-nosound", "-nomusic", FOX_EMU_DEMO NULL };
#ifdef FOX_EMU
    demo_name[4] = (char)('0' + EMU_DEMO);
#endif
    int argc = (int)(sizeof argv / sizeof argv[0]) - 1;
#ifndef FOX_EMU
    SYSCFG_DL_init();
    SysTick_Config(CPUCLK_FREQ / 1000u);
#endif
    doomgeneric_Create(argc, argv);
    for (;;) doomgeneric_Tick();
}
