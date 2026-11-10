/* BOOSTXL-AUDIO on the LP-MSPM33C321A: board layer. Pins: boards/mspm33c321a/audio.syscfg. Build only (this board is on motor work, not the audio bench). */
#include "ti_msp_dl_config.h"
#include "board_audio.h"

void SysTick_Handler(void) { app_tick(); }
void board_tick_enable(void) { }

/* DWT cycle counter, used by the algorithms app for cycle counts */
uint32_t board_cycles(void) { return DWT->CYCCNT; }
void board_init(void) { CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk; }
void board_sync(int l) { if (l) DL_GPIO_setPins(DAC_PORT, DAC_SYNC_PIN); else DL_GPIO_clearPins(DAC_PORT, DAC_SYNC_PIN); }
void board_sclk(int l) { if (l) DL_GPIO_setPins(DAC_PORT, DAC_SCLK_PIN); else DL_GPIO_clearPins(DAC_PORT, DAC_SCLK_PIN); }
void board_din(int l)  { if (l) DL_GPIO_setPins(DAC_PORT, DAC_DIN_PIN);  else DL_GPIO_clearPins(DAC_PORT, DAC_DIN_PIN); }
void board_amp(int mode)
{
    if (mode == 0) { DL_GPIO_disableOutput(DAC_PORT, DAC_AMP_PIN); return; }
    if (mode == 1) DL_GPIO_clearPins(DAC_PORT, DAC_AMP_PIN); else DL_GPIO_setPins(DAC_PORT, DAC_AMP_PIN);
    DL_GPIO_enableOutput(DAC_PORT, DAC_AMP_PIN);
}
/* PA21 (pin 5) is the VREF- pin, tied to ground: it stays an input. The pack's microphone is powered from pin 8 (PB30) through one wire to pin 5. */
void board_mic_power(int on) { DL_GPIO_disableOutput(MICPWR_PORT, MICPWR_ON_PIN); if (on) DL_GPIO_setPins(MICALT_PORT, MICALT_PWR_PIN); else DL_GPIO_clearPins(MICALT_PORT, MICALT_PWR_PIN); }
void board_adc_start(void) { DL_HSADC_triggerSequencerSoftwareForce(HSADC0_INST, DL_HSADC_SEQ_NUMBER1); }
uint16_t board_adc_read(void) { return (uint16_t)DL_HSADC_getResult(HSADC0_RESULT_INST, DL_HSADC_SOC_NUMBER0); }
void board_uart_put(uint8_t c) { DL_UART_Main_transmitDataBlocking(UART_0_INST, c); }
int board_uart_get(void) { return DL_UART_Main_isRXFIFOEmpty(UART_0_INST) ? -1 : (int)DL_UART_Main_receiveData(UART_0_INST); }
void board_delay_ms(uint32_t ms) { while (ms--) delay_cycles(32000); }

int main(void)
{
    SYSCFG_DL_init();
    app_main();
    return 0;
}
