/* Three ways, TI Drivers with no RTOS: TI's no-RTOS driver porting layer (kernel/nortos) runs
 * ClockP on SysTick, so the app runs straight from main. */
#include "ti_msp_dl_config.h"
#include "app.h"

int main(void)
{
    SYSCFG_DL_init();
    board_init();
    app_run();
}
