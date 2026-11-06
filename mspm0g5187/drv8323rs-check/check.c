/* LP-MSPM0G5187 cross-check of the BOOSTXL-DRV8323RS, read only: a second host and a hardware SPI (UC2, mode 0/phase 1,
 * 16 bit, 500 kHz, as TI's MSPM0 DRV8323RS examples) to tell a dead driver from a bug in the AM13 bit-banged reader.
 * Gate inputs held low; ENABLE high; registers 0x00 to 0x06 read once a second; nFAULT and VSENVM (ADC0 ch2) beside them.
 * Results in RAM (struct ck), read over the probe. Expected with VM on: 0x03 0x3FF, 0x04 0x7FF, 0x05 0x159, 0x06 0x283. */
#include "ti_msp_dl_config.h"

volatile struct { uint32_t magic, reads, nfault, vsenvm, reg[7], raw[7]; } ck;

static uint16_t xfer(uint16_t w)
{
    DL_GPIO_clearPins(OUT_NSCS_PORT, OUT_NSCS_PIN);
    delay_cycles(320);
    while (!DL_SPI_isRXFIFOEmpty(SPI_0_INST)) (void)DL_SPI_receiveData16(SPI_0_INST);
    DL_SPI_transmitData16(SPI_0_INST, w);
    while (DL_SPI_isBusy(SPI_0_INST)) {}
    while (DL_SPI_isRXFIFOEmpty(SPI_0_INST)) {}
    uint16_t r = DL_SPI_receiveData16(SPI_0_INST);
    delay_cycles(320);
    DL_GPIO_setPins(OUT_NSCS_PORT, OUT_NSCS_PIN);
    delay_cycles(1600);
    return r;
}

int main(void)
{
    SYSCFG_DL_init();
    DL_GPIO_clearPins(OUT_INHA_PORT, OUT_INHA_PIN); DL_GPIO_clearPins(OUT_INLA_PORT, OUT_INLA_PIN);
    DL_GPIO_clearPins(OUT_INHB_PORT, OUT_INHB_PIN); DL_GPIO_clearPins(OUT_INLB_PORT, OUT_INLB_PIN);
    DL_GPIO_clearPins(OUT_INHC_PORT, OUT_INHC_PIN); DL_GPIO_clearPins(OUT_INLC_PORT, OUT_INLC_PIN);
    DL_GPIO_setPins(OUT_NSCS_PORT, OUT_NSCS_PIN);
    DL_GPIO_setPins(OUT_ENABLE_PORT, OUT_ENABLE_PIN);
    delay_cycles(32000 * 5);                      /* 5 ms wake */
    while (1) {
        for (uint32_t a = 0; a < 7; a++) { uint16_t r = xfer((uint16_t)(0x8000U | (a << 11))); ck.raw[a] = r; ck.reg[a] = r & 0x7FFU; }
        ck.nfault = DL_GPIO_readPins(IN_PORT, IN_NFAULT_PIN) ? 1U : 0U;
        DL_ADC12_enableConversions(ADC12_0_INST);
        DL_ADC12_startConversion(ADC12_0_INST);
        while (DL_ADC12_isConversionStarted(ADC12_0_INST)) {}
        delay_cycles(32000);
        ck.vsenvm = DL_ADC12_getMemResult(ADC12_0_INST, DL_ADC12_MEM_IDX_0);
        ck.reads++; ck.magic = 0xD8323C0DU;
        delay_cycles(32000000);
    }
}
