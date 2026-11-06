// Port functions for the shared machine module (included by extmod/modmachine.c).
#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include "py/runtime.h"
#include "mphalport.h"

static void mp_machine_idle(void) {
    __asm volatile ("wfi");
}

MP_NORETURN static void mp_machine_reset(void) {
    NVIC_SystemReset();
    for (;;) {
    }
}

static mp_int_t mp_machine_reset_cause(void) {
    return (mp_int_t)DL_SYSCTL_getResetCause();
}

static mp_obj_t mp_machine_unique_id(void) {
    uint32_t id = DL_FactoryRegion_getTraceID();
    return mp_obj_new_bytes((const byte *)&id, sizeof(id));
}

static mp_obj_t mp_machine_get_freq(void) {
    return mp_obj_new_int_from_uint(frl_cpu_hz());
}

static void mp_machine_set_freq(size_t n_args, const mp_obj_t *args) {
    mp_raise_NotImplementedError(MP_ERROR_TEXT("fixed at build time"));
}

static void mp_machine_lightsleep(size_t n_args, const mp_obj_t *args) {
    mp_int_t ms = n_args ? mp_obj_get_int(args[0]) : 0;
    if (ms > 0) {
        mp_hal_delay_ms(ms);
    } else {
        __asm volatile ("wfi");
    }
}

MP_NORETURN static void mp_machine_deepsleep(size_t n_args, const mp_obj_t *args) {
    mp_machine_reset();
}
