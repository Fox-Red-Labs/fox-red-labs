/* BOOSTXL-AUDIO on the LP-AM13E230: board layer. Pins: boards/am13e230/audio.syscfg. */
#include "device.h"
#include "ti_sdk_dl_config.h"
#include "board_audio.h"

int _system_pre_init(void) { SCB->CPACR |= (3UL << 0); __DSB(); __ISB(); return 1; } /* the TMU trap from am13e230/AGENTS.md, harmless here */

void SysTick_Handler(void) { app_tick(); }


void board_tick_enable(void) { }

/* DWT cycle counter, used by the algorithms app for cycle counts */
uint32_t board_cycles(void) { return DWT->CYCCNT; }

void board_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    DL_ADC_powerUp(MIC_ADC_INST);
    DEVICE_DELAY_US(5000);
}
void board_sync(int l) { if (l) DL_GPIO_setPins(DAC_PORT, DAC_SYNC_PIN); else DL_GPIO_clearPins(DAC_PORT, DAC_SYNC_PIN); }
void board_sclk(int l) { if (l) DL_GPIO_setPins(DAC_PORT, DAC_SCLK_PIN); else DL_GPIO_clearPins(DAC_PORT, DAC_SCLK_PIN); }
void board_din(int l)  { if (l) DL_GPIO_setPins(DAC_PORT, DAC_DIN_PIN);  else DL_GPIO_clearPins(DAC_PORT, DAC_DIN_PIN); }
void board_amp(int mode)
{
    if (mode == 0) { DL_GPIO_disableOutput(DAC_PORT, DAC_AMP_PIN); return; }
    if (mode == 1) DL_GPIO_clearPins(DAC_PORT, DAC_AMP_PIN); else DL_GPIO_setPins(DAC_PORT, DAC_AMP_PIN);
    DL_GPIO_enableOutput(DAC_PORT, DAC_AMP_PIN);
}
void board_mic_power(int on) { if (on) DL_GPIO_setPins(MICPWR_PORT, MICPWR_ON_PIN); else DL_GPIO_clearPins(MICPWR_PORT, MICPWR_ON_PIN); }
void board_adc_start(void) { DL_ADC_forceSequencer(MIC_ADC_INST, MIC_ADC_SEQ1); }
uint16_t board_adc_read(void) { return DL_ADC_readResult(MIC_ADC_RESULT_INST, MIC_ADC_SOC0); }
void board_uart_put(uint8_t c) { while (DL_UART_isTXFIFOFull(UART_0_INST)) { } DL_UART_transmitData(UART_0_INST, c); }
int board_uart_get(void) { return DL_UART_isRXFIFOEmpty(UART_0_INST) ? -1 : (int)DL_UART_receiveData(UART_0_INST); }
void board_delay_ms(uint32_t ms) { DEVICE_DELAY_MS(ms); }

int main(void)
{
    Device_Init();
    SYSCFG_DL_init();
    app_main();
    return 0;
}
