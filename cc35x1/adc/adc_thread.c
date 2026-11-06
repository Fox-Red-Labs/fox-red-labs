/* Replacement for TI's threadFxn0: convert as fast as the driver allows for 3 s of RTOS ticks, then print. */
#include <FreeRTOS.h>
#include <task.h>
void *threadFxn0(void *arg0)
{
    ADC_Params params;
    ADC_Params_init(&params);
    ADC_Handle adc = ADC_open(CONFIG_ADC_0, &params);
    if (adc == NULL) { Display_printf(display, 0, 0, "ADC_open failed\n"); return NULL; }
    for (;;) {
        uint32_t n = 0, bad = 0, t0 = xTaskGetTickCount(), mn = 0xFFFF, mx = 0;
        uint16_t v;
        while ((uint32_t)(xTaskGetTickCount() - t0) < 3000) {
            if (ADC_convert(adc, &v) == ADC_STATUS_SUCCESS) { n++; if (v < mn) mn = v; if (v > mx) mx = v; } else bad++;
        }
        uint32_t ms = (uint32_t)(xTaskGetTickCount() - t0);
        Display_printf(display, 0, 0, "A,%lu,%lu,%lu,%lu,%lu,%lu", (unsigned long)n, (unsigned long)ms, (unsigned long)bad, (unsigned long)mn, (unsigned long)mx, (unsigned long)((uint64_t)n * 1000u / ms));
    }
}
