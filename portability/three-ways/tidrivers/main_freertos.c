/* Three ways, TI Drivers under FreeRTOS: start one task that runs the app. */
#include <FreeRTOS.h>
#include <task.h>
#include "ti_msp_dl_config.h"
#include "app.h"

static void app_task(void *arg)
{
    (void) arg;
    board_init();
    app_run();
}

int main(void)
{
    SYSCFG_DL_init();
    xTaskCreate(app_task, "app", 256, NULL, 1, NULL);
    vTaskStartScheduler();
    for (;;) {
    }
}
