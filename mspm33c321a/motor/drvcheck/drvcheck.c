/*
 * DRV8376EVM safe-state check on the LP-MSPM33C321A (Fox Red Labs, 2026-10-20). Nothing here drives the motor:
 * the six gate inputs are held low, DRVOFF high (outputs Hi-Z) and nSLEEP low before anything else runs. Then:
 * the Hall lines, nFAULT and the ILIMIT net (J3 pin 30, PC2) under pull-up, pull-down and no pull; the ADC
 * (current-sense zeros, bus and phase voltages); a wake (nSLEEP high, DRVOFF still high) with nFAULT watched for
 * 200 ms, once with PC2 floating and once with PC2 driven high (3.3 V, above VREF: the driver's cycle-by-cycle
 * limit off); then back to sleep. Results in RAM (`mc`), read over the probe. Pins: see the port plan.
 */
#include "ti_msp_dl_config.h"

#define CPU_HZ 32000000u
static void delay_ms(uint32_t ms) { while (ms--) DL_Common_delayCycles(CPU_HZ / 1000u); }

/* PINCM indices (mspm33c321x.h IOMUX_PINCMn) */
#define PCM_INHA IOMUX_PINCM70   /* PC17 */
#define PCM_INLA IOMUX_PINCM94   /* PC29 */
#define PCM_INHB IOMUX_PINCM71   /* PC18 */
#define PCM_INLB IOMUX_PINCM72   /* PC19 */
#define PCM_INHC IOMUX_PINCM23   /* PB6 */
#define PCM_INLC IOMUX_PINCM24   /* PB7 */
#define PCM_SLEEP IOMUX_PINCM30  /* PB13 nSLEEP */
#define PCM_DRVOFF IOMUX_PINCM65 /* PB28 */
#define PCM_FAULT IOMUX_PINCM67  /* PB30 nFAULT */
#define PCM_HA IOMUX_PINCM21     /* PA10 */
#define PCM_HB IOMUX_PINCM22     /* PA11 */
#define PCM_HC IOMUX_PINCM80     /* PC21 */
#define PCM_ILIM IOMUX_PINCM76   /* PC2, J3 pin 30 */

volatile struct {
    uint32_t magic;
    uint32_t hall_sleep[3], fault_sleep[3], ilim_pull[3];   /* [none, up, down] */
    uint32_t isen_sleep[3], vsen_sleep[4];
    uint32_t fault_wake_float[4], fault_wake_high[4];        /* nFAULT at 1, 10, 50, 200 ms after nSLEEP high */
    uint32_t hall_wake[3], isen_wake[3], vsen_wake[4];
    uint32_t done;
    uint32_t sleep_pull[3];       /* PB13 as an input, [none, up, down], read before it is driven: does the EVM pull nSLEEP? */
    uint32_t hold_fault, hold_loops;
    /* v3: one high side at a time, the other two phases off (INH and INL low: Hi-Z), so no current can flow.
     * [phase][0] DRVOFF high (outputs must stay off), [phase][1] DRVOFF low: phase voltages A B C and nFAULT */
    uint32_t ph_v[3][2][3], ph_fault[3][2];
    uint32_t asleep_fault, asleep_isen[3];   /* PB13 low for 5 ms: nFAULT and the sense outputs */   /* awake hold at the end: nFAULT now, loops since */
} mc;

static void out_pin(uint32_t pcm, GPIO_Regs *port, uint32_t pin, int high)
{
    DL_GPIO_initDigitalOutput(pcm);
    if (high) DL_GPIO_setPins(port, pin); else DL_GPIO_clearPins(port, pin);
    DL_GPIO_enableOutput(port, pin);
}

static void in_pin(uint32_t pcm, DL_GPIO_RESISTOR r)
{
    DL_GPIO_initDigitalInputFeatures(pcm, DL_GPIO_INVERSION_DISABLE, r, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
}

static uint32_t rd(GPIO_Regs *port, uint32_t pin) { return DL_GPIO_readPins(port, pin) ? 1u : 0u; }

static void adc(volatile uint32_t *isen, volatile uint32_t *vsen)
{
    for (int k = 0; k < 3; k++) isen[k] = 0;
    for (int k = 0; k < 4; k++) vsen[k] = 0;
    for (int n = 0; n < 16; n++) {
        DL_HSADC_triggerSequencerSoftwareForce(HSADC0_INST, DL_HSADC_SEQ_NUMBER1);
        DL_HSADC_triggerSequencerSoftwareForce(HSADC1_INST, DL_HSADC_SEQ_NUMBER1);
        while (DL_HSADC_isBusy(HSADC0_INST) || DL_HSADC_isBusy(HSADC1_INST)) {}
        for (int k = 0; k < 3; k++) isen[k] += DL_HSADC_getResult(HSADC0_RESULT_INST, (DL_HSADC_SOC_NUMBER)k);
        for (int k = 0; k < 4; k++) vsen[k] += DL_HSADC_getResult(HSADC1_RESULT_INST, (DL_HSADC_SOC_NUMBER)k);
    }
    for (int k = 0; k < 3; k++) isen[k] /= 16;
    for (int k = 0; k < 4; k++) vsen[k] /= 16;
}

static void wake_watch(volatile uint32_t *f)
{
    DL_GPIO_setPins(GPIOB, DL_GPIO_PIN_13);              /* nSLEEP high; DRVOFF stays high */
    delay_ms(1);   f[0] = rd(GPIOB, DL_GPIO_PIN_30);
    delay_ms(9);   f[1] = rd(GPIOB, DL_GPIO_PIN_30);
    delay_ms(40);  f[2] = rd(GPIOB, DL_GPIO_PIN_30);
    delay_ms(150); f[3] = rd(GPIOB, DL_GPIO_PIN_30);
}

int main(void)
{
    /* SysConfig's init resets GPIOA, B and C (SYSCFG_DL_initPower), so it runs first and the pins are set after it;
     * until then every pin is an input and the EVM's own pull-up holds DRVOFF high (pin scan, 2026-10-20).
     * Revisions 1 to 4 called it after the safe state, which undid every output: their wake and phase tests drove nothing. */
    SYSCFG_DL_init();
    out_pin(PCM_DRVOFF, GPIOB, DL_GPIO_PIN_28, 1);
    {   /* v2: nSLEEP pin pull test, outputs already off */
        DL_GPIO_RESISTOR r0[3] = {DL_GPIO_RESISTOR_NONE, DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_RESISTOR_PULL_DOWN};
        for (int i = 0; i < 3; i++) { in_pin(PCM_SLEEP, r0[i]); delay_ms(5); mc.sleep_pull[i] = rd(GPIOB, DL_GPIO_PIN_13); }
    }
    out_pin(PCM_SLEEP, GPIOB, DL_GPIO_PIN_13, 0);
    out_pin(PCM_INHA, GPIOC, DL_GPIO_PIN_17, 0); out_pin(PCM_INLA, GPIOC, DL_GPIO_PIN_29, 0);
    out_pin(PCM_INHB, GPIOC, DL_GPIO_PIN_18, 0); out_pin(PCM_INLB, GPIOC, DL_GPIO_PIN_19, 0);
    out_pin(PCM_INHC, GPIOB, DL_GPIO_PIN_6, 0);  out_pin(PCM_INLC, GPIOB, DL_GPIO_PIN_7, 0);

    mc.magic = 0xD8376C06u;

    in_pin(PCM_HA, DL_GPIO_RESISTOR_NONE); in_pin(PCM_HB, DL_GPIO_RESISTOR_NONE); in_pin(PCM_HC, DL_GPIO_RESISTOR_NONE);
    DL_GPIO_RESISTOR rs[3] = {DL_GPIO_RESISTOR_NONE, DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_RESISTOR_PULL_DOWN};
    for (int i = 0; i < 3; i++) {
        in_pin(PCM_FAULT, rs[i]); in_pin(PCM_ILIM, rs[i]); delay_ms(20);
        mc.fault_sleep[i] = rd(GPIOB, DL_GPIO_PIN_30);
        mc.ilim_pull[i] = rd(GPIOC, DL_GPIO_PIN_2);
    }
    in_pin(PCM_FAULT, DL_GPIO_RESISTOR_NONE); in_pin(PCM_ILIM, DL_GPIO_RESISTOR_NONE);
    mc.hall_sleep[0] = rd(GPIOA, DL_GPIO_PIN_10); mc.hall_sleep[1] = rd(GPIOA, DL_GPIO_PIN_11); mc.hall_sleep[2] = rd(GPIOC, DL_GPIO_PIN_21);
    adc(mc.isen_sleep, mc.vsen_sleep);

    /* wake 1: ILIMIT net left as it is (PC2 an input, no pull) */
    wake_watch(mc.fault_wake_float);
    mc.hall_wake[0] = rd(GPIOA, DL_GPIO_PIN_10); mc.hall_wake[1] = rd(GPIOA, DL_GPIO_PIN_11); mc.hall_wake[2] = rd(GPIOC, DL_GPIO_PIN_21);
    adc(mc.isen_wake, mc.vsen_wake);
    DL_GPIO_clearPins(GPIOB, DL_GPIO_PIN_13); delay_ms(5);

    /* wake 2: PC2 driven high (3.3 V on ILIMIT, if connected: above VREF, cycle-by-cycle limit off) */
    out_pin(PCM_ILIM, GPIOC, DL_GPIO_PIN_2, 1); delay_ms(5);
    wake_watch(mc.fault_wake_high);
    /* v3: phase output check, PC2 still high */
    {
        GPIO_Regs *hp[3] = {GPIOC, GPIOC, GPIOB}; uint32_t hpin[3] = {DL_GPIO_PIN_17, DL_GPIO_PIN_18, DL_GPIO_PIN_6};
        GPIO_Regs *lp[3] = {GPIOC, GPIOC, GPIOB}; uint32_t lpin[3] = {DL_GPIO_PIN_29, DL_GPIO_PIN_19, DL_GPIO_PIN_7};
        uint32_t isd[3], vsd[4];
        for (int ph = 0; ph < 3; ph++) {
            for (int en = 0; en < 2; en++) {
                if (en) DL_GPIO_clearPins(GPIOB, DL_GPIO_PIN_28); else DL_GPIO_setPins(GPIOB, DL_GPIO_PIN_28);
                delay_ms(2);
                /* v6: INH high, INL low: in 6x PWM mode the high side on (v5 showed INH and INL both high is off) */
                DL_GPIO_setPins(hp[ph], hpin[ph]); delay_ms(20);
                adc(isd, vsd);
                for (int k = 0; k < 3; k++) mc.ph_v[ph][en][k] = vsd[k + 1];
                mc.ph_fault[ph][en] = rd(GPIOB, DL_GPIO_PIN_30);
                DL_GPIO_clearPins(hp[ph], hpin[ph]); DL_GPIO_clearPins(lp[ph], lpin[ph]);
                DL_GPIO_setPins(GPIOB, DL_GPIO_PIN_28); delay_ms(20);
            }
        }
        DL_GPIO_clearPins(GPIOB, DL_GPIO_PIN_13); delay_ms(5);
        mc.asleep_fault = rd(GPIOB, DL_GPIO_PIN_30); adc(mc.asleep_isen, vsd);
    }
    /* v2: end awake, outputs still off (DRVOFF high), PC2 high, so the EVM's red D3 shows the driver's own state.
     * The blue LED (PA2) blinks once a second to show this image runs. */
    DL_GPIO_clearPins(GPIOB, DL_GPIO_PIN_13); delay_ms(2);
    DL_GPIO_setPins(GPIOB, DL_GPIO_PIN_13); delay_ms(10);
    out_pin(IOMUX_PINCM7, GPIOA, DL_GPIO_PIN_2, 0);       /* PA2 blue LED */
    mc.done = 0x600Du;
    while (1) {
        mc.hold_fault = rd(GPIOB, DL_GPIO_PIN_30); mc.hold_loops++;
        DL_GPIO_togglePins(GPIOA, DL_GPIO_PIN_2); delay_ms(500);
    }
}
