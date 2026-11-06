// MicroPython on the MSPM0G3519 and MSPM0G3507: startup glue, heap, REPL loop.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ti/devices/msp/msp.h>
#include "py/builtin.h"
#include "py/compile.h"
#include "py/runtime.h"
#include "py/repl.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "py/stackctrl.h"
#include "shared/runtime/gchelper.h"
#include "shared/runtime/pyexec.h"
#include "mphalport.h"

// Set by the linker script
extern uint32_t __heap_start__, __StackTop, __stack_floor__;

// Write every word of the SRAM we are about to use. The G3519's first SRAM bank has error
// correction; a byte write to a word that was never written first can raise a double-error
// interrupt, so heap and stack are cleared with word writes before the first byte store.
static void sram_prime(void) {
    uint32_t *p = &__heap_start__;
    uint32_t *top = (uint32_t *)((uintptr_t)__builtin_frame_address(0) - 256);
    while (p < top) {
        *p++ = 0;
    }
}

int main(void) {
    sram_prime();
    frl_board_init();
    mp_stack_set_top(&__StackTop);
    mp_stack_set_limit((char *)&__StackTop - (char *)&__stack_floor__ - 512);

    for (;;) {
        #if MICROPY_ENABLE_GC
        {
            uint32_t *heap_end = &__stack_floor__;
            #ifdef FRL_HEAP_MAX
            if ((uintptr_t)heap_end - (uintptr_t)&__heap_start__ > FRL_HEAP_MAX) {
                heap_end = (uint32_t *)((uintptr_t)&__heap_start__ + FRL_HEAP_MAX);
            }
            #endif
            gc_init(&__heap_start__, heap_end);
        }
        #endif
        mp_init();
        #ifdef FRL_BANNER_EXTRA
        mp_hal_stdout_tx_str(FRL_BANNER_EXTRA "\r\n");
        #endif
        for (;;) {
            if (pyexec_mode_kind == PYEXEC_MODE_RAW_REPL) {
                if (pyexec_raw_repl() != 0) {
                    break;
                }
            } else {
                if (pyexec_friendly_repl() != 0) {
                    break;
                }
            }
        }
        mp_printf(MP_PYTHON_PRINTER, "MPY: soft reboot\r\n");
        #if MICROPY_ENABLE_GC
        gc_sweep_all();
        #endif
        mp_deinit();
    }
    return 0;
}

#if MICROPY_ENABLE_GC
void gc_collect(void) {
    gc_collect_start();
    gc_helper_collect_regs_and_stack();
    gc_collect_end();
}
#endif

mp_lexer_t *mp_lexer_new_from_file(qstr filename) {
    mp_raise_OSError(MP_ENOENT);
}

mp_import_stat_t mp_import_stat(const char *path) {
    return MP_IMPORT_STAT_NO_EXIST;
}

void nlr_jump_fail(void *val) {
    for (;;) {
    }
}

#ifndef NDEBUG
void MP_WEAK __assert_func(const char *file, int line, const char *func, const char *expr) {
    for (;;) {
    }
}
#endif

// Fault handlers: stop where a debugger can see the state
void HardFault_Handler(void) {
    for (;;) {
    }
}

// There is no file system, so open() always fails with ENOENT.
static mp_obj_t frl_open(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    mp_raise_OSError(MP_ENOENT);
}
MP_DEFINE_CONST_FUN_OBJ_KW(mp_builtin_open_obj, 1, frl_open);
