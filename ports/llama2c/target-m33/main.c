/* LP-MSPM33C321A: run the TinyStories 260K model (int8) once at 160 MHz and leave the result in RAM for the probe to read.
 * No UART or screen is touched. Result block, all in RAM:  ll_result (see struct), ll_out (the text), ll_ids, ll_fwd_cycles.
 * Built with -DLL_EMU for the Cortex-M33 emulator (no clock setup, no cycle counter). */
#include <stdint.h>
#include <string.h>
#ifndef LL_EMU
#include "ti_msp_dl_config.h"
#endif
#include "llama2q.h"

extern const unsigned char ll_model_blob[], ll_tok_blob[];

#ifndef LL_PROMPT
#define LL_PROMPT "Once upon a time"
#endif
#ifndef LL_TEMP
#define LL_TEMP 1.0f
#endif
#ifndef LL_TOPP
#define LL_TOPP 0.9f
#endif
#ifndef LL_SEED
#define LL_SEED 42
#endif

typedef struct {
    uint32_t magic;          /* 0x4C4C4D32 when the run finished */
    int32_t  init_rc;        /* ll_init() result, 0 = ok */
    int32_t  tokens;         /* positions processed, prompt included */
    int32_t  prompt_tokens;
    uint32_t out_len;
    uint32_t cycles_lo, cycles_hi;   /* total cycles for all forward passes (64-bit) */
    uint32_t run_cycles_lo, run_cycles_hi;   /* cycles for the whole ll_generate call, sampling and tokenizing included */
} ll_result_t;

volatile ll_result_t ll_result;
char ll_out[1024];
static unsigned out_len;

static void emit(const char *piece)
{
    size_t n = strlen(piece);
    if (piece[0] == '\0') return;
    if (piece[1] == '\0' && !((piece[0] >= 32 && piece[0] < 127) || piece[0] == '\n' || piece[0] == '\t' || piece[0] == '\r')) return;
    if (out_len + n < sizeof ll_out - 1) { memcpy(ll_out + out_len, piece, n); out_len += n; ll_out[out_len] = '\0'; }
}

__attribute__((noinline)) void ll_halt(void) { for (;;) { __asm volatile ("nop"); } }

int main(void)
{
#ifndef LL_EMU
    SYSCFG_DL_init();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;   /* the DWT cycle counter, as mspm33c321a/bench/m33_timer.h */
    DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
#endif
    ll_result.init_rc = ll_init(ll_model_blob, ll_tok_blob);
    if (ll_result.init_rc == 0) {
#ifndef LL_EMU
        uint32_t r0 = DWT->CYCCNT;
#endif
        int n = ll_generate(LL_PROMPT, LL_STEPS, LL_TEMP, LL_TOPP, LL_SEED, emit);
        uint64_t tot = 0; for (int i = 0; i < n; i++) tot += ll_fwd_cycles[i];
#ifndef LL_EMU
        uint64_t run = (uint32_t)(DWT->CYCCNT - r0);
#else
        uint64_t run = 0;
#endif
        ll_result.tokens = n; ll_result.prompt_tokens = ll_prompt_tokens; ll_result.out_len = out_len;
        ll_result.cycles_lo = (uint32_t)tot; ll_result.cycles_hi = (uint32_t)(tot >> 32);
        ll_result.run_cycles_lo = (uint32_t)run; ll_result.run_cycles_hi = (uint32_t)(run >> 32);
    }
    ll_result.magic = 0x4C4C4D32u;
    ll_halt();
}
