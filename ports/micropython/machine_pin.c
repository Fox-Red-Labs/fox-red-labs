// machine.Pin for the MSPM0G3519 and MSPM0G3507: a pin is named "PA10", "A10", or a number
// port*32+pin. Modes: IN, OUT, OPEN_DRAIN. Pull-ups and pull-downs: PULL_UP, PULL_DOWN.
#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <string.h>
#include "py/runtime.h"
#include "py/mphal.h"
#include "mphalport.h"

#if defined(FRL_G3519)
#include "boards/pins_g3519.h"
#else
#include "boards/pins_g3507.h"
#endif

enum { MODE_IN = 0, MODE_OUT = 1, MODE_OPEN_DRAIN = 2 };

static machine_pin_obj_t pin_table[3][32];
static uint8_t pin_ready;

static void pin_table_init(void) {
    static const uint8_t *const maps[3] = { frl_pincm_a, frl_pincm_b, frl_pincm_c };
    for (int p = 0; p < 3; p++) {
        for (int n = 0; n < 32; n++) {
            pin_table[p][n] = (machine_pin_obj_t){ { &machine_pin_type }, (uint8_t)p, (uint8_t)n,
                maps[p][n] ? (uint8_t)(maps[p][n] - 1) : 0xff };
        }
    }
    pin_ready = 1;
}

static mp_hal_pin_obj_t pin_find(int port, int num) {
    if (!pin_ready) {
        pin_table_init();
    }
    if (port < 0 || port > 2 || num < 0 || num > 31 || pin_table[port][num].iomux == 0xff) {
        mp_raise_ValueError(MP_ERROR_TEXT("no such pin"));
    }
    return &pin_table[port][num];
}

mp_hal_pin_obj_t mp_hal_get_pin_obj(mp_obj_t obj) {
    if (mp_obj_is_type(obj, &machine_pin_type)) {
        return MP_OBJ_TO_PTR(obj);
    }
    if (mp_obj_is_int(obj)) {
        int v = mp_obj_get_int(obj);
        return pin_find(v >> 5, v & 31);
    }
    size_t len;
    const char *s = mp_obj_str_get_data(obj, &len);
    if (len > 0 && (s[0] == 'P' || s[0] == 'p')) {
        s++;
        len--;
    }
    if (len < 2 || len > 3) {
        mp_raise_ValueError(MP_ERROR_TEXT("no such pin"));
    }
    int port = (s[0] | 0x20) - 'a';
    int num = 0;
    for (size_t i = 1; i < len; i++) {
        if (s[i] < '0' || s[i] > '9') {
            mp_raise_ValueError(MP_ERROR_TEXT("no such pin"));
        }
        num = num * 10 + (s[i] - '0');
    }
    return pin_find(port, num);
}

const char *mp_hal_pin_name(mp_hal_pin_obj_t pin) {
    static char buf[2][6];
    static uint8_t which;
    which ^= 1;
    char *b = buf[which];
    *b++ = 'P';
    *b++ = 'A' + pin->port;
    if (pin->num >= 10) {
        *b++ = '0' + pin->num / 10;
    }
    *b++ = '0' + pin->num % 10;
    *b = 0;
    return buf[which];
}

static void pin_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    mp_printf(print, "Pin(%s)", mp_hal_pin_name(MP_OBJ_TO_PTR(self_in)));
}

static void pin_configure(mp_hal_pin_obj_t pin, int mode, int pull) {
    if (mode == MODE_OUT) {
        mp_hal_pin_output(pin);
    } else if (mode == MODE_OPEN_DRAIN) {
        mp_hal_pin_open_drain(pin);
    } else {
        mp_hal_pin_input(pin);
        if (pull == 1) {
            DL_GPIO_setDigitalInternalResistor(pin->iomux, DL_GPIO_RESISTOR_PULL_UP);
        } else if (pull == 2) {
            DL_GPIO_setDigitalInternalResistor(pin->iomux, DL_GPIO_RESISTOR_PULL_DOWN);
        }
    }
}

// Pin(id, mode=IN, pull=-1, *, value=None)
static mp_obj_t pin_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 3, true);
    mp_hal_pin_obj_t pin = mp_hal_get_pin_obj(args[0]);
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, args + n_args);
    int mode = n_args > 1 ? mp_obj_get_int(args[1]) : MODE_IN;
    int pull = (n_args > 2 && args[2] != mp_const_none) ? mp_obj_get_int(args[2]) : 0;
    mp_map_elem_t *val = mp_map_lookup(&kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_value), MP_MAP_LOOKUP);
    if (val != NULL && mode != MODE_IN) {
        // set the level before the output turns on, so the pin does not glitch
        mp_hal_pin_write(pin, mp_obj_is_true(val->value));
    }
    pin_configure(pin, mode, pull);
    return MP_OBJ_FROM_PTR(pin);
}

// pin() reads, pin(v) writes
static mp_obj_t pin_call(mp_obj_t self_in, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 1, false);
    mp_hal_pin_obj_t pin = MP_OBJ_TO_PTR(self_in);
    if (n_args == 0) {
        return MP_OBJ_NEW_SMALL_INT(mp_hal_pin_read(pin));
    }
    mp_hal_pin_write(pin, mp_obj_is_true(args[0]));
    return mp_const_none;
}

static mp_obj_t pin_value(size_t n_args, const mp_obj_t *args) {
    return pin_call(args[0], n_args - 1, 0, args + 1);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(pin_value_obj, 1, 2, pin_value);

static mp_obj_t pin_on(mp_obj_t self_in) {
    mp_hal_pin_write(MP_OBJ_TO_PTR(self_in), 1);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(pin_on_obj, pin_on);

static mp_obj_t pin_off(mp_obj_t self_in) {
    mp_hal_pin_write(MP_OBJ_TO_PTR(self_in), 0);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(pin_off_obj, pin_off);

static mp_obj_t pin_toggle(mp_obj_t self_in) {
    mp_hal_pin_obj_t pin = MP_OBJ_TO_PTR(self_in);
    GPIO_Regs *r = pin->port == 0 ? GPIOA : GPIOB;
    #ifdef FRL_G3519
    if (pin->port == 2) {
        r = GPIOC;
    }
    #endif
    DL_GPIO_togglePins(r, 1u << pin->num);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(pin_toggle_obj, pin_toggle);

static mp_obj_t pin_init(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    mp_hal_pin_obj_t pin = MP_OBJ_TO_PTR(pos_args[0]);
    int mode = n_args > 1 ? mp_obj_get_int(pos_args[1]) : MODE_IN;
    int pull = (n_args > 2 && pos_args[2] != mp_const_none) ? mp_obj_get_int(pos_args[2]) : 0;
    pin_configure(pin, mode, pull);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(pin_init_obj, 1, pin_init);

static const mp_rom_map_elem_t pin_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&pin_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_value), MP_ROM_PTR(&pin_value_obj) },
    { MP_ROM_QSTR(MP_QSTR_on), MP_ROM_PTR(&pin_on_obj) },
    { MP_ROM_QSTR(MP_QSTR_off), MP_ROM_PTR(&pin_off_obj) },
    { MP_ROM_QSTR(MP_QSTR_toggle), MP_ROM_PTR(&pin_toggle_obj) },
    { MP_ROM_QSTR(MP_QSTR_IN), MP_ROM_INT(MODE_IN) },
    { MP_ROM_QSTR(MP_QSTR_OUT), MP_ROM_INT(MODE_OUT) },
    { MP_ROM_QSTR(MP_QSTR_OPEN_DRAIN), MP_ROM_INT(MODE_OPEN_DRAIN) },
    { MP_ROM_QSTR(MP_QSTR_PULL_UP), MP_ROM_INT(1) },
    { MP_ROM_QSTR(MP_QSTR_PULL_DOWN), MP_ROM_INT(2) },
};
static MP_DEFINE_CONST_DICT(pin_locals_dict, pin_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    machine_pin_type,
    MP_QSTR_Pin,
    MP_TYPE_FLAG_NONE,
    make_new, pin_make_new,
    print, pin_print,
    call, pin_call,
    locals_dict, &pin_locals_dict
    );
