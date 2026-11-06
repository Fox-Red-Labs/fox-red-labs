/* BOOSTXL-AUDIO on the LP-MSPM0G3519: board layer. Pins: boards/mspm0g3519/audio.syscfg. Build only (this board is not on the audio bench). */
#include "ti_msp_dl_config.h"
#include "board_audio.h"

void SysTick_Handler(void) { app_tick(); }
void board_tick_enable(void) { }

void board_init(void) { DL_ADC12_enableConversions(ADC12_0_INST); }
void board_sync(int l) { if (l) DL_GPIO_setPins(DAC_PORT, DAC_SYNC_PIN); else DL_GPIO_clearPins(DAC_PORT, DAC_SYNC_PIN); }
void board_sclk(int l) { if (l) DL_GPIO_setPins(DAC_PORT, DAC_SCLK_PIN); else DL_GPIO_clearPins(DAC_PORT, DAC_SCLK_PIN); }
void board_din(int l)  { if (l) DL_GPIO_setPins(DAC_PORT, DAC_DIN_PIN);  else DL_GPIO_clearPins(DAC_PORT, DAC_DIN_PIN); }
void board_amp(int mode)
{
    if (mode == 0) { DL_GPIO_disableOutput(DAC_PORT, DAC_AMP_PIN); return; }
    if (mode == 1) DL_GPIO_clearPins(DAC_PORT, DAC_AMP_PIN); else DL_GPIO_setPins(DAC_PORT, DAC_AMP_PIN);
    DL_GPIO_enableOutput(DAC_PORT, DAC_AMP_PIN);
}
void board_mic_power(int on) { if (on) DL_GPIO_setPins(DAC_PORT, DAC_MIC_PIN); else DL_GPIO_clearPins(DAC_PORT, DAC_MIC_PIN); }
void board_adc_start(void) { DL_ADC12_startConversion(ADC12_0_INST); }
uint16_t board_adc_read(void) { return DL_ADC12_getMemResult(ADC12_0_INST, DL_ADC12_MEM_IDX_0); }
void board_uart_put(uint8_t c) { DL_UART_Main_transmitDataBlocking(UART_0_INST, c); }
int board_uart_get(void) { return DL_UART_Main_isRXFIFOEmpty(UART_0_INST) ? -1 : (int)DL_UART_Main_receiveData(UART_0_INST); }
void board_delay_ms(uint32_t ms) { while (ms--) delay_cycles(32000); }

int main(void)
{
    SYSCFG_DL_init();
    app_main();
    return 0;
}
