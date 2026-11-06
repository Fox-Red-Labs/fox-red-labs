#!/bin/bash
# TI's driverlib/hsadc_max_freq_dma, changed so it measures the ADC and not the DMA.
# TI's example runs the DMA in single-block mode ("each DMA trigger results in a transfer of a block of data,
# once", dl_dma.h): the first ADC trigger makes the DMA copy the result register 2,000 times back to back, so its
# completion time is the DMA's speed. Here the DMA is set to one transfer per ADC trigger (syscfg transferMode
# SINGLE), so 2,000 transfers need 2,000 finished conversions, and the timer period is swept from 250 ns down to
# 25 ns (the timer runs at 160 MHz) to find where the ADC stops keeping up. Time is read by the DWT cycle counter.
# Output: <dir>/adc-maxrate/ticlang/hsadc_max_freq_dma.out ; RAM struct `ar`.
R="${1:-$PWD/out/mspm33c321a}"; O="$R/adc-maxrate"
TOOLPATHS_FAMILY=mspm33-cc; . "$(dirname "$0")/../../tools/frl/toolpaths.sh"   # SDK and CC: MSPM33 SDK and TI Arm Clang, found or FRL_MSPM33_SDK / FRL_TICLANG
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
rm -rf "$O"; cp -R "$SDK/examples/nortos/LP_MSPM33C321A/driverlib/hsadc_max_freq_dma" "$O"
python3 - "$O" <<'P'
import sys
O=sys.argv[1]
c=O+'/hsadc_max_freq_dma.syscfg'
s=open(c).read()
old='DMA1.associatedChannels[0].transferMode       = "SINGLE_BLOCK";'
assert old in s
open(c,'w').write(s.replace(old, 'DMA1.associatedChannels[0].transferMode       = "SINGLE";'))
p=O+'/hsadc_max_freq_dma.c'
s=open(p).read()
i=s.index('volatile uint16_t adcResult')
s=s[:i]+'''volatile uint16_t adcResult[ADC_NUM_SAMPLES] = {0U};
volatile struct { uint32_t magic, cpu_hz, n; struct { uint32_t period_cnt, cycles, left, min, max, distinct; } r[16]; } ar;

static void one(int k, uint32_t cnt)
{
    DL_TimerA_stopCounter(TIMER_0_INST);
    DL_DMA_disableChannel(DMA0, DMA0_CH0_CHAN_ID);
    for (uint32_t i = 0; i < ADC_NUM_SAMPLES; i++) adcResult[i] = 0;
    DL_DMA_setSrcAddr(DMA0, DMA0_CH0_CHAN_ID, (uint32_t)(&(HSADC0_RESULT_INST->ADC_LITE_RESULT_REGS.ADCRESULT0)));
    DL_DMA_setDestAddr(DMA0, DMA0_CH0_CHAN_ID, (uint32_t)&adcResult);
    DL_DMA_setTransferSize(DMA0, DMA0_CH0_CHAN_ID, ADC_NUM_SAMPLES);
    DL_DMA_enableChannel(DMA0, DMA0_CH0_CHAN_ID);
    DL_TimerA_setLoadValue(TIMER_0_INST, cnt - 1);
    DWT->CYCCNT = 0;
    DL_TimerA_startCounter(TIMER_0_INST);
    uint32_t limit = 160000u * 4u;              /* 4 ms */
    while (DL_DMA_isChannelEnabled(DMA0, DMA0_CH0_CHAN_ID) && DWT->CYCCNT < limit) {
    }
    uint32_t cyc = DWT->CYCCNT;
    DL_TimerA_stopCounter(TIMER_0_INST);
    ar.r[k].period_cnt = cnt;
    ar.r[k].cycles = cyc;
    ar.r[k].left = DL_DMA_getTransferSize(DMA0, DMA0_CH0_CHAN_ID);
    uint32_t mn = 0xFFFF, mx = 0, d = 0, prev = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < ADC_NUM_SAMPLES; i++) {
        uint32_t v = adcResult[i];
        if (v < mn) mn = v;
        if (v > mx) mx = v;
        if (v != prev) d++;
        prev = v;
    }
    ar.r[k].min = mn; ar.r[k].max = mx; ar.r[k].distinct = d;
}

static struct { uint32_t window, cycles, left; } rw_dummy;
volatile struct { uint32_t n; struct { uint32_t window, cycles, left, max; } r[8]; } aw;

int main()
{
    SYSCFG_DL_init();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    ar.cpu_hz = CPUCLK_FREQ;
    static const uint16_t cnts[] = { 40, 32, 24, 20, 18, 17, 16, 15, 14, 13, 12, 10, 8, 6, 4 };
    ar.n = sizeof(cnts) / sizeof(cnts[0]);
    for (uint32_t k = 0; k < ar.n; k++) one(k, cnts[k]);
    /* sample window sweep at a 50 ns trigger (faster than the ADC): window in ADC clocks, as the third argument of DL_HSADC_setupSequencer */
    static const uint8_t wins[] = { 16, 12, 10, 8, 6, 4, 2, 1 };
    aw.n = sizeof(wins);
    for (uint32_t k = 0; k < aw.n; k++) {
        DL_HSADC_setupSequencer(HSADC0_INST, DL_HSADC_SEQ_NUMBER1, wins[k], DL_HSADC_TRIGGER_GEN_SUB_0, DL_HSADC_SOC_NUMBER0);
        one(15, 8);
        aw.r[k].window = wins[k]; aw.r[k].cycles = ar.r[15].cycles; aw.r[k].left = ar.r[15].left; aw.r[k].max = ar.r[15].max;
    }
    ar.magic = 0xADC00001u;
    while (1);
}
'''
open(p,'w').write(s)
P
make -C "$O/ticlang" MSPM33_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CC" GCC_ARMCOMPILER= > "$O/build.log" 2>&1 && echo built || { tail "$O/build.log"; exit 1; }
