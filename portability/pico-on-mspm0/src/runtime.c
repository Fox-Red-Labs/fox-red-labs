/* Pico SDK on MSPM0: what the Pico SDK's runtime does before main(): clocks and pins (SysConfig), the microsecond time base,
 * the board's own start-up. Runs as a C constructor, so the example's main() is the program's main(). */
#include "ti_msp_dl_config.h"
#include "pico_mspm0_board.h"
void pico_time_init(void);
__attribute__((constructor)) static void pico_runtime_init(void)
{
    SYSCFG_DL_init();
    pico_time_init();
    pico_board_init();
}
