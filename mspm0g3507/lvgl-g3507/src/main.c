#include "ti_msp_dl_config.h"
#include "lvgl.h"
#include "port.h"
#include "app.h"
#include "board.h"

int main(void)
{
    SYSCFG_DL_init();
    port_init();
    port_uart_puts("\r\nLVGL on LP-" BOARD_NAME ": starting\r\n");
    app_create();
    for (;;) {
        lv_timer_handler();
        __WFI();                 /* sleep until the next 1 ms SysTick */
    }
}
