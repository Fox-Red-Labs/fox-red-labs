/*
 * Alarm listener on the LP-MSPM0G5187, built two ways from this one file:
 *   APP_CAPTURE  streams the onboard microphone to the Mac at 15,873 samples a second (recording datasets)
 *   APP_DETECT   listens for a smoke (temporal-3) or CO (temporal-4) alarm with a small CNN on the
 *                TinyEngine NPU, gated by the tone and loudness learned from your own alarm (button S2)
 *
 * Audio path: ICS-40720 microphone -> PCM1809 ADC -> I2S on PB10 (WCLK), PB11 (BCLK), PB12 (data),
 * 47,619 Hz, one 32-bit slot, DMA in two halves of 48 words (TI's device_uac_microphone_i2s setup).
 * Each sample is cut to 18 bits, low-pass filtered (fir_dec3.h) and every third one kept.
 *
 * PB10 and PB11 are also the CC1407P radio's RESET_N and SPI_INT wires when the radio is fitted
 * (mspm0g5187/radio/README.md): take those two wires off J20 before running this.
 */
#include "ti_msp_dl_config.h"
#include <stdint.h>
#include <string.h>
#include "fir_dec3.h"

#define HALF 48                       /* words per DMA half: 1 ms at 47,619 Hz */
static volatile uint32_t dmabuf[2 * HALF];
static volatile uint8_t half_ready[2];
static volatile uint32_t overruns;
static volatile uint32_t ms_ticks;

#ifdef APP_DETECT
static void ring_push_half(int h);     /* detect.h: decimate inside the interrupt so feature work never stalls it */
#define HALF_DONE(h) ring_push_half(h)
#else
#define HALF_DONE(h) do { if (half_ready[h]) overruns++; half_ready[h] = 1; } while (0)
#endif

void DMA_IRQHandler(void)
{
    switch (DL_DMA_getPendingInterrupt(DMA)) {
    case DL_DMA_FULL_CH_EVENT_IIDX_EARLY_IRQ_DMACH0:
        HALF_DONE(0);
        break;
    case DL_DMA_EVENT_IIDX_DMACH0:
        HALF_DONE(1);
        break;
    default:
        break;
    }
}

void SysTick_Handler(void) { ms_ticks++; }

/* ---- UART (backchannel, 921600) ---- */
static void tx(uint8_t b) { DL_UART_Main_transmitDataBlocking(UART_0_INST, b); }
static void tx_str(const char *s) { while (*s) tx((uint8_t)*s++); }
static void tx_int(int32_t v)
{
    char b[12]; int i = 0; uint32_t u;
    if (v < 0) { tx('-'); u = (uint32_t)(-v); } else u = (uint32_t)v;
    do { b[i++] = (char)('0' + u % 10); u /= 10; } while (u);
    while (i) tx((uint8_t)b[--i]);
}

/* ---- decimate by 3 ---- */
static int32_t hist[FIR_TAPS];
static uint8_t hidx, phase;

/* Push one raw I2S word; returns 1 and writes *out when a 15,873 Hz sample is ready. */
static int dec_push(uint32_t w, int32_t *out)
{
    hist[hidx] = ((int32_t)w) >> 14;          /* 32-bit left-justified -> 18-bit signed */
    uint8_t i = hidx;
    hidx = (uint8_t)(hidx + 1 == FIR_TAPS ? 0 : hidx + 1);
    if (++phase < 3) return 0;
    phase = 0;
    int32_t acc = 0;
    for (int k = 0; k < FIR_TAPS; k++) {
        acc += fir_q12[k] * hist[i];
        i = (uint8_t)(i == 0 ? FIR_TAPS - 1 : i - 1);
    }
    *out = acc >> 12;
    return 1;
}

static void leds(int r, int g, int b)
{
    if (r) DL_GPIO_setPins(GPIO_LED_RED_PORT, GPIO_LED_RED_PIN); else DL_GPIO_clearPins(GPIO_LED_RED_PORT, GPIO_LED_RED_PIN);
    if (g) DL_GPIO_setPins(GPIO_LED_GREEN_PORT, GPIO_LED_GREEN_PIN); else DL_GPIO_clearPins(GPIO_LED_GREEN_PORT, GPIO_LED_GREEN_PIN);
    if (b) DL_GPIO_setPins(GPIO_LED_BLUE_PORT, GPIO_LED_BLUE_PIN); else DL_GPIO_clearPins(GPIO_LED_BLUE_PORT, GPIO_LED_BLUE_PIN);
}

static void audio_start(void)
{
    DL_I2S_disable(I2S_0_INST);
    /* TI's example turns the input buffer on for the clock pins it drives, and samples on the rising edge. */
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

#ifdef APP_CAPTURE
/*
 * Stream: one packet per DMA half (1 ms): A5 5A seq n=16, 16 samples as 3-byte little-endian
 * signed 18-bit values, then the low byte of the payload sum. 52 bytes/ms = 52 kB/s of 92 kB/s.
 * A text line "#..." once a second carries the overrun count and the peak level.
 */
int main(void)
{
    SYSCFG_DL_init();
    leds(0, 0, 0);
    tx_str("\r\n#ALARM-CAPTURE fs=15873 fmt=s18-in-3-bytes-le packet=A55A seq n payload csum\r\n");
    audio_start();
    uint8_t seq = 0, pkt[48];
    uint32_t nh = 0; int32_t peak = 0;
    for (;;) {
        for (int h = 0; h < 2; h++) {
            if (!half_ready[h]) continue;
            half_ready[h] = 0;
            int n = 0;
            for (int k = 0; k < HALF; k++) {
                int32_t y;
                if (dec_push(dmabuf[h * HALF + k], &y)) {
                    int32_t a = y < 0 ? -y : y;
                    if (a > peak) peak = a;
                    pkt[3 * n] = (uint8_t)y; pkt[3 * n + 1] = (uint8_t)(y >> 8); pkt[3 * n + 2] = (uint8_t)(y >> 16);
                    n++;
                }
            }
            uint8_t cs = 0;
            tx(0xA5); tx(0x5A); tx(seq++); tx((uint8_t)n);
            for (int k = 0; k < 3 * n; k++) { tx(pkt[k]); cs = (uint8_t)(cs + pkt[k]); }
            tx(cs);
            if (++nh % 1000 == 0) {
#ifdef RAW_DIAG
                {   /* raw words from this DMA half, and how many have bits below the 24-bit sample set */
                    static const char hx[] = "0123456789abcdef"; int low = 0;
                    for (int k = 0; k < HALF; k++) low += (dmabuf[h * HALF + k] & 0xFF) != 0;
                    tx_str("#raw low8set="); tx_int(low); tx_str("/48");
                    for (int k = 0; k < 6; k++) { uint32_t w = dmabuf[h * HALF + k]; tx(' '); for (int b = 28; b >= 0; b -= 4) tx((uint8_t)hx[(w >> b) & 15]); }
                    tx_str("\r\n");
                }
#endif
                tx_str("#t="); tx_int((int32_t)(nh / 1000)); tx_str(" overruns="); tx_int((int32_t)overruns);
                tx_str(" peak="); tx_int(peak); tx_str("\r\n");
                peak = 0;
                leds(0, (nh / 1000) & 1, 0);
            }
        }
    }
}
#endif

#ifdef NO_AUDIO
/* Self-test image (APP_DETECT with NO_AUDIO): SysConfig's weak init functions replaced so PB10, PB11 and
 * PB12 stay unconnected and the I2S module never starts. Safe with the CC1407P radio still wired. */
void SYSCFG_DL_I2S_0_init(void) { }
void SYSCFG_DL_GPIO_init(void)
{
    DL_GPIO_initPeripheralOutputFunction(GPIO_UART_0_IOMUX_TX, GPIO_UART_0_IOMUX_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction(GPIO_UART_0_IOMUX_RX, GPIO_UART_0_IOMUX_RX_FUNC);
    DL_GPIO_initDigitalInputFeatures(GPIO_BTN_S2_IOMUX, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalOutput(GPIO_LED_BLUE_IOMUX);
    DL_GPIO_initDigitalOutput(GPIO_LED_RED_IOMUX);
    DL_GPIO_initDigitalOutput(GPIO_LED_GREEN_IOMUX);
    DL_GPIO_clearPins(GPIOA, GPIO_LED_RED_PIN | GPIO_LED_GREEN_PIN);
    DL_GPIO_enableOutput(GPIOA, GPIO_LED_RED_PIN | GPIO_LED_GREEN_PIN);
    DL_GPIO_clearPins(GPIOB, GPIO_LED_BLUE_PIN);
    DL_GPIO_enableOutput(GPIOB, GPIO_LED_BLUE_PIN);
}
#endif

#ifdef APP_DETECT
#include "detect.h"
#endif

#ifdef APP_BENCH
#include "bench.h"
#endif
