/* Force-included into llama2q.c in the board build: LL_CYCLES() reads the DWT cycle counter (enabled in main.c). */
#include "ti_msp_dl_config.h"
#define LL_CYCLES() ((uint32_t)DWT->CYCCNT)
