/* Microstepping the kit 28BYJ-48 through its ULN2003 board on the LP-AM13E230, same six wires as the on-off version.
 * Four coils 90 electrical degrees apart get sinusoidal duties from the PWM units (20 kHz). MCPWM4's zero event
 * interrupts 20,000 times a second; the handler advances the angle and writes four compare values. The sine comes
 * from the C library (sinf on the FPU) or, with -DUSE_TMU, from the chip's trig unit (TMU) through fastrts_tmu.h.
 * -DNO_ISR builds the same image with the interrupt left off, to measure what the main loop does with no motor.
 * Motion: one turn forward (512 electrical cycles, 100,000 updates = 5 s), 1 s pause, one turn back, 1 s pause.
 * Measured in RAM for run_image.py: isr best, worst, mean cycles (DWT counter), g_loops (main loop iterations),
 * g_isrs (handler count), g_turns.
 */
#include <math.h>
#include "device.h"
#include "ti_sdk_dl_config.h"
#ifdef USE_TMU
#include "fastrts_tmu.h"
#endif
#include "../../bench/m33_timer.h"

#define PRD          10000U            /* counts a PWM period */
#define UPD_HZ       20000U
#define CYCLES_TURN  512U              /* electrical cycles per shaft turn: 2,048 full steps / 4 */
#define UPD_TURN     (5U * UPD_HZ)     /* 5 s a turn */
#define UPD_PAUSE    (1U * UPD_HZ)
#define AMP          0.95f             /* peak duty */
#define TWO_PI       6.28318530718f

volatile struct {
    uint32_t magic;                    /* 0x5BEE0001 once the first turn has started */
    uint32_t best, worst, count;       /* handler cycles: best, worst, number measured */
    uint64_t sum;
    uint32_t loops;                    /* main loop iterations since start */
    uint32_t turns, phase;             /* turns done; 1 fwd, 2 pause, 3 back, 4 pause */
    float    theta;
} m;

static uint32_t upd = 0;
static float dtheta = TWO_PI * (float)CYCLES_TURN / (float)UPD_TURN;

static inline void coils(float theta, float amp)
{
    /* unipolar: each coil only takes the positive half of its sine; 1 count minimum keeps the output defined */
    float s0 = sinf(theta), s1 = sinf(theta - 1.57079633f), s2 = -s0, s3 = -s1;
    uint16_t c0 = s0 > 0 ? (uint16_t)(s0 * amp * (float)PRD) : 1U;
    uint16_t c1 = s1 > 0 ? (uint16_t)(s1 * amp * (float)PRD) : 1U;
    uint16_t c2 = s2 > 0 ? (uint16_t)(s2 * amp * (float)PRD) : 1U;
    uint16_t c3 = s3 > 0 ? (uint16_t)(s3 * amp * (float)PRD) : 1U;
    DL_MCPWM_setCounterCompareShadowValue(COILS_INST, DL_MCPWM_COUNTER_COMPARE_1A, c0 ? c0 : 1U);
    DL_MCPWM_setCounterCompareShadowValue(COILS_INST, DL_MCPWM_COUNTER_COMPARE_2A, c1 ? c1 : 1U);
    DL_MCPWM_setCounterCompareShadowValue(COILS_INST, DL_MCPWM_COUNTER_COMPARE_3A, c2 ? c2 : 1U);
    DL_MCPWM_setCounterCompareShadowValue(COIL4_INST, DL_MCPWM_COUNTER_COMPARE_2A, c3 ? c3 : 1U);
}

void COILS_INT_Handler(void)
{
    uint32_t t0 = DWT->CYCCNT;
    uint32_t ph = m.phase;
    if (ph == 1U || ph == 3U) {
        m.theta += (ph == 1U) ? dtheta : -dtheta;
        if (m.theta > TWO_PI) m.theta -= TWO_PI;
        if (m.theta < 0.0f)   m.theta += TWO_PI;
        coils(m.theta, AMP);
    } else {
        coils(0.0f, 0.0f);
    }
    if (++upd >= ((ph == 1U || ph == 3U) ? UPD_TURN : UPD_PAUSE)) {
        upd = 0;
        if (ph == 4U) m.turns++;
        m.phase = (ph == 4U) ? 1U : ph + 1U;
        if (m.phase == 1U || m.phase == 3U) DL_GPIO_clearPins(GPIO_LED_PORT, GPIO_LED_LED4_PIN);
        else                                DL_GPIO_setPins(GPIO_LED_PORT, GPIO_LED_LED4_PIN);
    }
    DL_MCPWM_clearInterrupt(COILS_INST, DL_MCPWM_INT_ET_1);
    DL_MCPWM_clearGlobalInterrupt(COILS_INST);
    uint32_t d = DWT->CYCCNT - t0;
    if (d < m.best)  m.best = d;
    if (d > m.worst) m.worst = d;
    m.sum += d; m.count++;
}

int main(void)
{
    Device_Init();
    SYSCFG_DL_init();
    bench_timer_start();
    m.best = 0xFFFFFFFFU; m.worst = 0; m.sum = 0; m.count = 0; m.loops = 0; m.turns = 0; m.theta = 0.0f;
    m.phase = 1U;
    DL_GPIO_clearPins(GPIO_LED_PORT, GPIO_LED_LED4_PIN);
#ifdef NO_ISR
    NVIC_DisableIRQ(COILS_INT);
#endif
    DL_MCPWM_enableTBCLK();
    m.magic = 0x5BEE0001U;
    while (1) { m.loops++; }
}
