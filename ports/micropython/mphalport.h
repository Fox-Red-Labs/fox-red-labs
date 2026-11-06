#pragma once
#include <stdint.h>
#include "py/obj.h"
#include "shared/runtime/interrupt_char.h"

#define MP_HAL_PIN_FMT "%s"

// One entry per GPIO pin that exists on the package
typedef struct _machine_pin_obj_t {
    mp_obj_base_t base;
    uint8_t port;      // 0 = A, 1 = B, 2 = C
    uint8_t num;       // 0 to 31
    uint8_t iomux;     // IOMUX index = PINCM number minus 1
} machine_pin_obj_t;

#define mp_hal_pin_obj_t const machine_pin_obj_t *

mp_hal_pin_obj_t mp_hal_get_pin_obj(mp_obj_t obj);
const char *mp_hal_pin_name(mp_hal_pin_obj_t pin);
void mp_hal_pin_input(mp_hal_pin_obj_t pin);
void mp_hal_pin_output(mp_hal_pin_obj_t pin);
void mp_hal_pin_open_drain(mp_hal_pin_obj_t pin);
void mp_hal_pin_od_low(mp_hal_pin_obj_t pin);
void mp_hal_pin_od_high(mp_hal_pin_obj_t pin);
int mp_hal_pin_read(mp_hal_pin_obj_t pin);
void mp_hal_pin_write(mp_hal_pin_obj_t pin, int v);

void frl_board_init(void);
uint32_t frl_cpu_hz(void);
#define mp_hal_delay_us_fast(us) mp_hal_delay_us(us)
