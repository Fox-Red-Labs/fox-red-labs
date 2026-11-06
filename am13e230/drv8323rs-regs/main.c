/* BOOSTXL-DRV8323RS register read-back on the LP-AM13E230 Rev E2. Reads only: no register is written, no PWM runs.
 * The six gate inputs are driven low (6x PWM mode with every input low leaves all FETs off), ENABLE PA3 high,
 * then registers 0x00 to 0x06 are read over a bit-banged SPI (mode 1, about 50 kHz; SDO is open drain, so PA20 has its pull-up on) once a second.
 * Frame: bit 15 = 1 read, bits 14:11 address, bits 10:0 data. The driver samples SDI on SCLK's falling edge and
 * shifts SDO out on the rising edge. Its logic runs from VM: with the motor supply off every read is 0x000.
 * Also reads, once a second, what the BoosterPack drives back: ADC codes (12 bit, 3.3 V) g_adc[7] = VSENVM, VSENA, VSENB,
 * VSENC, ISENA, ISENB, ISENC; and g_pins = bit 0 nFAULT, 1 EVM ID 8323x (PA12), 2 EVM ID SPI variant (PA11), 3 to 5 Halls A to C,
 * 6 SDO idle. Each pin is read with the pull-up (low byte) then the pull-down (second byte): a pin the board drives reads the same.
 * Live values for run_image.py: g_reg[7] (11-bit data), g_nfault (1 = no fault), g_reads, g_vm_seen, g_adc[7], g_pins.
 * Expected defaults with VM on (DRV8323RS data sheet): 0x02 0x000, 0x03 0x3FF, 0x04 0x7FF, 0x05 0x159, 0x06 0x283.
 */
#include "device.h"
#include "ti_sdk_dl_config.h"

volatile uint32_t g_reg[7], g_nfault, g_reads, g_vm_seen, g_adc[7], g_pins;

static uint32_t conv(ADC_Regs *a, ADC_RESULT_Regs *r, uint32_t n, volatile uint32_t *out)
{
    uint32_t t = 10000U, i;
    DL_ADC_forceSequencer(a, DL_ADC_SEQ_NUMBER1);
    while (DL_ADC_isBusy(a) && t) { DEVICE_DELAY_US(1); t--; }
    for (i = 0; i < n; i++) out[i] = t ? DL_ADC_readResult(r, (DL_ADC_SOC_NUMBER)i) : 0xFFFFU;
    return t;
}

typedef struct { GPIO_Regs *port; uint32_t pin; uint32_t pcm; } Pin;
static const Pin probe[7] = {
    { DRV_NFAULT_PORT, DRV_NFAULT_PIN, DRV_NFAULT_IOMUX }, { BRD_ID323_PORT, BRD_ID323_PIN, BRD_ID323_IOMUX },
    { BRD_IDSPI_PORT, BRD_IDSPI_PIN, BRD_IDSPI_IOMUX },    { BRD_HALLA_PORT, BRD_HALLA_PIN, BRD_HALLA_IOMUX },
    { BRD_HALLB_PORT, BRD_HALLB_PIN, BRD_HALLB_IOMUX },    { BRD_HALLC_PORT, BRD_HALLC_PIN, BRD_HALLC_IOMUX },
    { SPIA_PORT, SPIA_SDO_PIN, SPIA_SDO_IOMUX },
};

static uint32_t scan(void)
{
    uint32_t i, v = 0;
    for (i = 0; i < 7; i++) {
        DL_GPIO_initDigitalInputFeatures(probe[i].pcm, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_WAKEUP_DISABLE);
        DEVICE_DELAY_US(200);
        if (DL_GPIO_readPins(probe[i].port, probe[i].pin)) v |= 1U << i;
        DL_GPIO_initDigitalInputFeatures(probe[i].pcm, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_DOWN, DL_GPIO_WAKEUP_DISABLE);
        DEVICE_DELAY_US(200);
        if (DL_GPIO_readPins(probe[i].port, probe[i].pin)) v |= 1U << (i + 8);
        DL_GPIO_initDigitalInputFeatures(probe[i].pcm, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_WAKEUP_DISABLE);
    }
    return v;
}

/* mode: bit 1 = CPOL (SCLK idle level), bit 0 = CPHA (sample on the second edge). The DRV8323 is CPOL 0, CPHA 1. */
static uint16_t xferm(uint16_t out, uint32_t mode, uint32_t half_us)
{
    uint16_t in = 0;
    int b;
    uint32_t pol = (mode >> 1) & 1U, pha = mode & 1U;
    if (pol) DL_GPIO_setPins(SPIB_PORT, SPIB_SCLK_PIN); else DL_GPIO_clearPins(SPIB_PORT, SPIB_SCLK_PIN);
    DEVICE_DELAY_US(half_us);
    DL_GPIO_clearPins(SPIB_PORT, SPIB_NSCS_PIN);
    DEVICE_DELAY_US(half_us);
    for (b = 15; b >= 0; b--) {
        if (!pha) { if (out & (1U << b)) DL_GPIO_setPins(SPIB_PORT, SPIB_SDI_PIN); else DL_GPIO_clearPins(SPIB_PORT, SPIB_SDI_PIN); DEVICE_DELAY_US(half_us); }
        if (pol) DL_GPIO_clearPins(SPIB_PORT, SPIB_SCLK_PIN); else DL_GPIO_setPins(SPIB_PORT, SPIB_SCLK_PIN);   /* first edge */
        if (pha) { if (out & (1U << b)) DL_GPIO_setPins(SPIB_PORT, SPIB_SDI_PIN); else DL_GPIO_clearPins(SPIB_PORT, SPIB_SDI_PIN); }
        DEVICE_DELAY_US(half_us);
        if (!pha) in = (uint16_t)((in << 1) | (DL_GPIO_readPins(SPIA_PORT, SPIA_SDO_PIN) ? 1U : 0U));
        if (pol) DL_GPIO_setPins(SPIB_PORT, SPIB_SCLK_PIN); else DL_GPIO_clearPins(SPIB_PORT, SPIB_SCLK_PIN);   /* second edge */
        DEVICE_DELAY_US(half_us);
        if (pha) in = (uint16_t)((in << 1) | (DL_GPIO_readPins(SPIA_PORT, SPIA_SDO_PIN) ? 1U : 0U));
    }
    DL_GPIO_clearPins(SPIB_PORT, SPIB_SCLK_PIN);
    DEVICE_DELAY_US(half_us);
    DL_GPIO_setPins(SPIB_PORT, SPIB_NSCS_PIN);
    DEVICE_DELAY_US(20);
    return in;
}
static uint16_t xfer(uint16_t out) { return xferm(out, 1U, 10U); }

/* g_modes[m][r]: register r (3 to 6) read in SPI mode m at 50 us half period (10 kHz), after a fault-reset pulse.
   g_sdo_low: count of SDO samples seen low during the whole sweep (any value above 0 means the driver drove the line). */
volatile uint32_t g_modes[4][4], g_sdo_low;

int main(void)
{
    uint32_t a;
    Device_Init();
    SYSCFG_DL_init();
    DL_GPIO_clearPins(GATE_B_PORT, GATE_B_INHA_PIN | GATE_B_INHB_PIN | GATE_B_INHC_PIN);
    DL_GPIO_clearPins(GATE_A_PORT, GATE_A_INLA_PIN | GATE_A_INLB_PIN | GATE_A_INLC_PIN);
    DL_GPIO_clearPins(SPIB_PORT, SPIB_SCLK_PIN | SPIB_SDI_PIN);
    DL_ADC_powerUp(ADC_0_INST); DL_ADC_powerUp(ADC_1_INST); DL_ADC_powerUp(ADC_2_INST);
    DL_ADC_enableSequencer(ADC_0_INST, DL_ADC_SEQ_NUMBER1);
    DL_ADC_enableSequencer(ADC_1_INST, DL_ADC_SEQ_NUMBER1);
    DL_ADC_enableSequencer(ADC_2_INST, DL_ADC_SEQ_NUMBER1);
    DEVICE_DELAY_MS(5);
    DL_GPIO_setPins(DRV_ENABLE_PORT, DRV_ENABLE_PIN);
    DEVICE_DELAY_MS(2);                                     /* wake time after ENABLE, 1 ms max */
    /* fault reset: ENABLE low for 10 us (under the sleep time) clears latched faults (DRV8323 data sheet, ENABLE pin) */
    DL_GPIO_clearPins(DRV_ENABLE_PORT, DRV_ENABLE_PIN); DEVICE_DELAY_US(10); DL_GPIO_setPins(DRV_ENABLE_PORT, DRV_ENABLE_PIN);
    DEVICE_DELAY_MS(2);
    { uint32_t m, r;
      for (m = 0; m < 4; m++) for (r = 0; r < 4; r++) {
          uint16_t v = xferm((uint16_t)(0x8000U | ((r + 3U) << 11)), m, 50U);
          g_modes[m][r] = v;
          if (v != 0xFFFFU) g_sdo_low++;
      } }
    while (1) {
        for (a = 0; a < 7; a++) g_reg[a] = xfer((uint16_t)(0x8000U | (a << 11))) & 0x7FFU;
        { uint32_t t[3];
          conv(ADC_2_INST, ADC_2_RESULT_INST, 3, t); g_adc[0] = t[0]; g_adc[3] = t[1]; g_adc[4] = t[2];
          conv(ADC_0_INST, ADC_0_RESULT_INST, 2, t); g_adc[1] = t[0]; g_adc[6] = t[1];
          conv(ADC_1_INST, ADC_1_RESULT_INST, 2, t); g_adc[2] = t[0]; g_adc[5] = t[1]; }
        g_pins = scan();
        g_nfault = DL_GPIO_readPins(DRV_NFAULT_PORT, DRV_NFAULT_PIN) ? 1U : 0U;
        if (g_reg[3] == 0x3FFU && g_reg[4] == 0x7FFU) g_vm_seen = 1;
        g_reads++;
        DL_GPIO_togglePins(GPIO_LED_PORT, GPIO_LED_LED4_PIN);
        DEVICE_DELAY_MS(1000);
    }
}
