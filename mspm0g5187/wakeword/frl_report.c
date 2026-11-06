/* Fox Red Labs additions to TI's wakeword_detection_ai_npu (see frl_report.h). */
#include "ti_msp_dl_config.h"
#include <stdint.h>
#include <string.h>
#include "frl_report.h"
#include "common.h"
#include "buffer_manager.h"
#include "fir_hb.h"

#define HOLD_FRAMES 100            /* green LED stays on 100 frames = 2 s after a detection */

/* ---- serial (backchannel UART) ---- */
static void tx(uint8_t b) { DL_UART_Main_transmitDataBlocking(UART_0_INST, b); }
void frl_tx_str(const char *s) { while (*s) tx((uint8_t)*s++); }
void frl_tx_int(int32_t v)
{
    char b[12]; int i = 0; uint32_t u;
    if (v < 0) { tx('-'); u = (uint32_t)(-v); } else u = (uint32_t)v;
    do { b[i++] = (char)('0' + u % 10); u /= 10; } while (u);
    while (i) tx((uint8_t)b[--i]);
}

/* ---- time base (SysTick at 1 ms, from SysConfig; the shipped ADC build has none) ---- */
#if !INPUT_ADC
static volatile uint32_t ms_ticks;
void SysTick_Handler(void) { ms_ticks++; }
static uint32_t now_cyc(void)          /* core clock cycles, wraps after 53 s at 80 MHz; use differences */
{
    uint32_t m, v;
    do { m = ms_ticks; v = SysTick->VAL; } while (m != ms_ticks);
    return m * 80000u + (80000u - v);
}
static uint32_t now_us(void) { uint32_t m; do { m = ms_ticks; } while (m != ms_ticks); return m * 1000u + (80000u - SysTick->VAL) / 80u; }
#else
extern uint32_t systick_rollOver_counter;     /* the example's own SysTick: 8,000,000 cycles a rollover */
static uint32_t now_cyc(void)
{
    uint32_t m, v;
    do { m = systick_rollOver_counter; v = SysTick->VAL; } while (m != systick_rollOver_counter);
    return m * 8000000u + (8000000u - v);
}
static uint32_t now_us(void) { return 0; }
#endif

/* ---- audio in ---- */
volatile uint32_t frl_isr_count;          /* ADC build: samples taken by the shipped ISR (hook added by build.sh) */
extern volatile int16_t raw_audio_samples_copy[NUM_AUDIO_SAMPLES];
extern volatile uint16_t audio_sample;
extern volatile uint32_t samples_count;

#if INPUT_I2S || INPUT_REPLAY
/* One 8 kHz sample from the microphone chain. The example's ISR hands buffer_manager a 14-bit ADC
 * code and buffer_manager subtracts 8192 and shifts left 2; we give it the same kind of code. */
#ifndef MIC_GAIN
#define MIC_GAIN 1
#endif
void frl_push_sample(int32_t s18)
{
    int32_t v = ((s18 * MIC_GAIN) >> 4) + 8192;     /* 18-bit signed -> 14-bit code, mid-scale 8192 */
    if (v < 0) v = 0; else if (v > 16383) v = 16383;
    audio_sample = (uint16_t)v;
    shift_audio_buffer();
    samples_count++;
    frl_isr_count++;
}
#endif

#if INPUT_I2S
#define HALF 48                       /* words per DMA half: 1 ms at 47,619 Hz */
static volatile uint32_t dmabuf[2 * HALF];

/* 47,619 -> 7,936.5 Hz without a multiplier in the fast part (see make_fir2.py): CIC order 4 by 3, then a half-band FIR by 2 */
static uint32_t ci1, ci2, ci3, ci4;                 /* integrators (wrap around on purpose) */
static uint32_t cc1, cc2, cc3, cc4;                 /* comb delays */
static uint8_t cph;
static int32_t hb[32]; static uint8_t hbi, hph;

static void dec_word(uint32_t w)
{
    uint32_t x = (uint32_t)(((int32_t)w) >> 14);    /* 32-bit left-justified -> 18-bit signed */
    ci1 += x; ci2 += ci1; ci3 += ci2; ci4 += ci3;
    if (++cph < 3) return;
    cph = 0;
    uint32_t t = ci4, d;
    d = t - cc1; cc1 = t; t = d;
    d = t - cc2; cc2 = t; t = d;
    d = t - cc3; cc3 = t; t = d;
    d = t - cc4; cc4 = t; t = d;
    int32_t y = (int32_t)(((int32_t)t * 809) >> 16);        /* CIC gain 3^4 = 81 */
    hb[hbi] = y; hbi = (uint8_t)((hbi + 1) & 31);
    if (++hph < 2) return;
    hph = 0;
    uint8_t c = (uint8_t)(hbi - 1 - 15);                      /* centre tap: 15 samples back from the newest */
    int32_t acc = 0;
    for (int k = 0; k < HB_K; k++) {
        uint8_t o = (uint8_t)(2 * k + 1);
        acc += hb_q15[k] * (hb[(uint8_t)(c + o) & 31] + hb[(uint8_t)(c - o) & 31]);
    }
    frl_push_sample((acc >> 15) + (hb[c & 31] >> 1));
}

static volatile uint32_t last_word, isr_sum, isr_n, isr_max;
void DMA_IRQHandler(void)
{
    int h;
    uint32_t v0 = SysTick->VAL;
    switch (DL_DMA_getPendingInterrupt(DMA)) {
    case DL_DMA_FULL_CH_EVENT_IIDX_EARLY_IRQ_DMACH0: h = 0; break;
    case DL_DMA_EVENT_IIDX_DMACH0: h = 1; break;
    default: return;
    }
    last_word = dmabuf[h * HALF + 5];
    for (int k = 0; k < HALF; k++) dec_word(dmabuf[h * HALF + k]);
    {
        uint32_t d = (v0 + 80000u - SysTick->VAL) % 80000u;
        isr_sum += d; isr_n++; if (d > isr_max) isr_max = d;
    }
}

void frl_audio_start(void)
{
    DL_I2S_disable(I2S_0_INST);
    /* TI's device_uac_microphone_i2s turns the input buffer on for the clock pins it drives, samples on the rising edge */
    IOMUX->SECCFG.PINCM[GPIO_I2S_0_IOMUX_BCLK] |= IOMUX_PINCM_INENA_ENABLE | IOMUX_PINCM_PIPD_ENABLE;
    IOMUX->SECCFG.PINCM[GPIO_I2S_0_IOMUX_WCLK] |= IOMUX_PINCM_INENA_ENABLE | IOMUX_PINCM_PIPD_ENABLE;
    DL_I2S_setSampleEdge(I2S_0_INST, DL_I2S_SAMPLE_EDGE_POS);
    while (!DL_I2S_isRXFIFOEmpty(I2S_0_INST)) (void)DL_I2S_receiveData32(I2S_0_INST);
    DL_I2S_setSampleWordLength(I2S_0_INST, 31);
    NVIC_SetPriority(DMA_INT_IRQn, 1);
    NVIC_EnableIRQ(DMA_INT_IRQn);
    DL_DMA_setSrcAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t)(&I2S_0_INST->RXDATA));
    DL_DMA_setDestAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t)&dmabuf[0]);
    DL_DMA_setTransferSize(DMA, DMA_CH0_CHAN_ID, 2 * HALF);
    DL_DMA_enableChannel(DMA, DMA_CH0_CHAN_ID);
    DL_I2S_enable(I2S_0_INST);
}
#else
void frl_audio_start(void) { }
#endif

#if INPUT_REPLAY
/* 8 kHz samples from the Mac, 16-bit little-endian, over the serial port (replay input). The bytes go into a
 * 2,048-byte ring by DMA: with a plain receive interrupt the 4-byte FIFO overran (245 overruns in 1,400 frames)
 * whenever the NPU run held off interrupts, and one lost byte garbles every sample after it. */
#define RXN 2048
static volatile uint8_t rxring[RXN];
static uint16_t rxr;
static volatile uint32_t rx_ovr, rx_err;
static void rx_dma_start(void)
{
    DL_DMA_setSrcAddr(DMA, DMA_CH1_CHAN_ID, (uint32_t)(&UART_0_INST->uart->RXDATA));
    DL_DMA_setDestAddr(DMA, DMA_CH1_CHAN_ID, (uint32_t)&rxring[0]);
    DL_DMA_setTransferSize(DMA, DMA_CH1_CHAN_ID, RXN);
    DL_DMA_enableChannel(DMA, DMA_CH1_CHAN_ID);
}
void frl_poll(void)
{
    uint32_t st = DL_UART_Main_getRawInterruptStatus(UART_0_INST, DL_UART_MAIN_INTERRUPT_OVERRUN_ERROR | DL_UART_MAIN_INTERRUPT_FRAMING_ERROR | DL_UART_MAIN_INTERRUPT_NOISE_ERROR);
    if (st) {
        if (st & DL_UART_MAIN_INTERRUPT_OVERRUN_ERROR) rx_ovr++; else rx_err++;
        DL_UART_Main_clearInterruptStatus(UART_0_INST, DL_UART_MAIN_INTERRUPT_OVERRUN_ERROR | DL_UART_MAIN_INTERRUPT_FRAMING_ERROR | DL_UART_MAIN_INTERRUPT_NOISE_ERROR);
    }
    uint16_t w = (uint16_t)((RXN - DL_DMA_getTransferSize(DMA, DMA_CH1_CHAN_ID)) & (RXN - 1));
    while ((uint16_t)((w - rxr) & (RXN - 1)) >= 2) {
        uint8_t lo = rxring[rxr], hi = rxring[(rxr + 1) & (RXN - 1)];
        rxr = (uint16_t)((rxr + 2) & (RXN - 1));
        int16_t sm = (int16_t)(lo | (hi << 8));
        frl_push_sample((int32_t)sm * 4);          /* 16-bit -> the 18-bit scale frl_push_sample expects */
    }
}
#else
void frl_poll(void) { }
#endif

/* ---- pipelined loop pieces: TI's extract_features_lpc() split in two (same arithmetic as feature_extraction.c) ---- */
#include "lpc_float/lpc_defs.h"
#include "buffer_manager.h"
extern int16_t lpc[LPC_BANKS];
extern void get_lpc(int16_t *buffer);
void frl_lpc_only(void)
{
    get_lpc((int16_t *)raw_audio_samples_copy);
    for (uint16_t i = 0; i < LPC_BANKS; i++) {
        int32_t temp = lpc[i];
        temp += (1 * 256);
        temp *= 157;
        temp >>= (5 + 8);
        lpc[i] = (int16_t)temp;
        if (lpc[i] >= 127) lpc[i] = 127;
        if (lpc[i] < -128) lpc[i] = -128;
    }
}
/* Same result as TI's shift_NN_input_buffer() (drop the oldest frame, append the new one), as two block copies instead of 7,000 byte moves */
void frl_window_add(void)
{
    memmove((void *)&audio_features[0], (const void *)&audio_features[LPC_OUTPUT_SIZE], (AUDIO_FEATURE_WINDOW - 1) * LPC_OUTPUT_SIZE);
    for (int k = 0; k < LPC_OUTPUT_SIZE; k++) audio_features[(AUDIO_FEATURE_WINDOW - 1) * LPC_OUTPUT_SIZE + k] = (int8_t)lpc[k];
}

/* ---- detection bookkeeping and the report ---- */
static uint32_t inf_n, ev_n, run_n, hold, pos_n;
static int32_t mx = -255;
static uint32_t t_a, t_l, dt_npu_sum, dt_lpc_sum, dt_n, dt_l_n, peak, absum;

void frl_init(void)
{
#if INPUT_REPLAY
    rx_dma_start();
#endif
    DL_GPIO_clearPins(GPIO_LED_PORT, GPIO_LED_GREEN_PIN);
    frl_tx_str("\r\n#WAKEWORD input="
#if INPUT_ADC
        "adc"
#elif INPUT_I2S
        "i2s"
#else
        "replay"
#endif
        " model=OK-Kilby frame=160 samples (20 ms at 8 kHz)\r\n");
}

void frl_ready(void) { frl_tx_str("#ready\r\n"); }
void frl_t0(void) { t_a = now_cyc(); }
void frl_t1(void) { dt_npu_sum += now_cyc() - t_a; dt_n++; }
void frl_l0(void) { t_l = now_cyc(); }
void frl_l1(void) { dt_lpc_sum += now_cyc() - t_l; dt_l_n++; }

void frl_after_inference(int8_t c0, int8_t c1)
{
    int32_t m = (int32_t)c0 - (int32_t)c1;
    inf_n++;
    if (m > mx) mx = m;
    if (c0 > c1) { run_n++; pos_n++; } else run_n = 0;
    /* level of the frame just analysed */
    {
        uint32_t pk = 0, ab = 0;
        for (int k = 0; k < NUM_AUDIO_SAMPLES; k++) {
            int32_t a = raw_audio_samples_copy[k]; if (a < 0) a = -a;
            ab += (uint32_t)a; if ((uint32_t)a > pk) pk = (uint32_t)a;
        }
        if (pk > peak) peak = pk;
        absum += ab / NUM_AUDIO_SAMPLES;
    }
    if (run_n == 6) {                       /* same rule as post_inference_ww: 6 in a row */
        ev_n++;
        hold = HOLD_FRAMES;
        DL_GPIO_setPins(GPIO_LED_PORT, GPIO_LED_GREEN_PIN);
        frl_tx_str("WAKE n="); frl_tx_int((int32_t)ev_n);
        frl_tx_str(" inf="); frl_tx_int((int32_t)inf_n);
        frl_tx_str(" t_us="); frl_tx_int((int32_t)now_us());
        frl_tx_str(" margin="); frl_tx_int(m);
        frl_tx_str("\r\n");
    }
    if (hold && --hold == 0) DL_GPIO_clearPins(GPIO_LED_PORT, GPIO_LED_GREEN_PIN);
    if (inf_n % 50 == 0) {                  /* about once a second */
        frl_tx_str("#inf="); frl_tx_int((int32_t)inf_n);
        frl_tx_str(" ev="); frl_tx_int((int32_t)ev_n);
        frl_tx_str(" pos="); frl_tx_int((int32_t)pos_n);
        frl_tx_str(" maxmargin="); frl_tx_int(mx);
        frl_tx_str(" peak="); frl_tx_int((int32_t)peak);
        frl_tx_str(" meanabs="); frl_tx_int((int32_t)(absum / 50));
        if (dt_n) { frl_tx_str(" npu_cyc="); frl_tx_int((int32_t)(dt_npu_sum / dt_n)); }
        if (dt_l_n) { frl_tx_str(" lpc_cyc="); frl_tx_int((int32_t)(dt_lpc_sum / dt_l_n)); }
#if INPUT_I2S
        { static const char hx[] = "0123456789abcdef"; uint32_t w = last_word; frl_tx_str(" raw=");
          for (int b = 28; b >= 0; b -= 4) tx((uint8_t)hx[(w >> b) & 15]); }
#endif
#if INPUT_I2S
        if (isr_n) { frl_tx_str(" isr_avg="); frl_tx_int((int32_t)(isr_sum / isr_n)); frl_tx_str(" isr_max="); frl_tx_int((int32_t)isr_max); frl_tx_str(" isr_n="); frl_tx_int((int32_t)isr_n); isr_sum = 0; isr_n = 0; isr_max = 0; }
#endif
#if INPUT_REPLAY
        frl_tx_str(" rx_ovr="); frl_tx_int((int32_t)rx_ovr); frl_tx_str(" rx_err="); frl_tx_int((int32_t)rx_err); 
#endif
        frl_tx_str(" adc_isr="); frl_tx_int((int32_t)frl_isr_count);
        frl_tx_str(" backlog="); frl_tx_int((int32_t)samples_count);
        frl_tx_str(" cyc="); frl_tx_int((int32_t)now_cyc());
        frl_tx_str("\r\n");
        mx = -255; peak = 0; absum = 0; pos_n = 0; dt_npu_sum = 0; dt_n = 0; dt_lpc_sum = 0; dt_l_n = 0;
    }
}
