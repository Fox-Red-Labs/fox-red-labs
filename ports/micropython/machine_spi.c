// machine.SPI on SPI1 of the MSPM0G3519: controller mode, 8 bits, MSB first, SCLK PB31, MOSI PB8,
// MISO PB7 (the pins of the LaunchPad kit's J1 and J2 headers; chip select is a plain Pin).
// Written for this port from the DriverLib API. Methods: init, deinit, write, read, readinto,
// write_readinto, and write_repeat(buf, n) (port extra: sends buf n times, no Python call per repeat).
#if defined(FRL_G3519)
#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include "py/runtime.h"
#include "py/mphal.h"
#include "mphalport.h"

extern const mp_obj_type_t machine_spi_type;

#define SPI_INST SPI1
#define SPI_BUSCLK_HZ 80000000u   // SPI1 is clocked from MCLK, 80 MHz in, so 40 MHz out at most

typedef struct _machine_spi_obj_t {
    mp_obj_base_t base;
    uint32_t baudrate;
    uint8_t polarity, phase;
    uint8_t ready;
} machine_spi_obj_t;

static machine_spi_obj_t spi_obj = { { &machine_spi_type }, 0, 0, 0, 0 };

static void spi_pin_check(mp_obj_t o, int port, int num) {
    mp_hal_pin_obj_t p = mp_hal_get_pin_obj(o);
    if (p->port != port || p->num != num) {
        mp_raise_ValueError(MP_ERROR_TEXT("SPI1 pins are sck=PB31, mosi=PB8, miso=PB7"));
    }
}

static void spi_apply(machine_spi_obj_t *self) {
    static const DL_SPI_ClockConfig clk = { .clockSel = DL_SPI_CLOCK_BUSCLK, .divideRatio = DL_SPI_CLOCK_DIVIDE_RATIO_1 };
    DL_SPI_Config cfg = {
        .mode = DL_SPI_MODE_CONTROLLER,
        .frameFormat = (DL_SPI_FRAME_FORMAT)(DL_SPI_FRAME_FORMAT_MOTO3_POL0_PHA0 +
            (self->polarity ? 1 : 0) + (self->phase ? 2 : 0)),
        .parity = DL_SPI_PARITY_NONE,
        .dataSize = DL_SPI_DATA_SIZE_8,
        .bitOrder = DL_SPI_BIT_ORDER_MSB_FIRST,
    };
    if (!self->ready) {
        DL_SPI_reset(SPI_INST);
        DL_SPI_enablePower(SPI_INST);
        mp_hal_delay_us(10);
        DL_GPIO_initPeripheralOutputFunction(IOMUX_PINCM68, IOMUX_PINCM68_PF_SPI1_SCLK);
        DL_GPIO_initPeripheralOutputFunction(IOMUX_PINCM25, IOMUX_PINCM25_PF_SPI1_PICO);
        DL_GPIO_initPeripheralInputFunction(IOMUX_PINCM24, IOMUX_PINCM24_PF_SPI1_POCI);
        self->ready = 1;
    }
    DL_SPI_disable(SPI_INST);
    DL_SPI_setClockConfig(SPI_INST, (DL_SPI_ClockConfig *)&clk);
    DL_SPI_init(SPI_INST, &cfg);
    uint32_t scr = (SPI_BUSCLK_HZ + 2 * self->baudrate - 1) / (2 * self->baudrate);
    scr = scr ? scr - 1 : 0;
    if (scr > 1023) {
        scr = 1023;
    }
    DL_SPI_setBitRateSerialClockDivider(SPI_INST, scr);
    DL_SPI_enable(SPI_INST);
}

static void spi_flush_rx(void) {
    while (!DL_SPI_isRXFIFOEmpty(SPI_INST)) {
        (void)DL_SPI_receiveData8(SPI_INST);
    }
}

static void spi_wait_done(void) {
    while (!DL_SPI_isTXFIFOEmpty(SPI_INST)) {
    }
    mp_hal_delay_us_fast(1);
    while (DL_SPI_isBusy(SPI_INST)) {
    }
    spi_flush_rx();
}

static void spi_send(const uint8_t *src, size_t len) {
    for (size_t i = 0; i < len; i++) {
        while (DL_SPI_isTXFIFOFull(SPI_INST)) {
        }
        DL_SPI_transmitData8(SPI_INST, src[i]);
    }
}

static void spi_transfer(const uint8_t *src, uint8_t *dest, size_t len, uint8_t fill) {
    spi_flush_rx();
    for (size_t i = 0; i < len; i++) {
        while (DL_SPI_isTXFIFOFull(SPI_INST)) {
        }
        DL_SPI_transmitData8(SPI_INST, src ? src[i] : fill);
        while (DL_SPI_isRXFIFOEmpty(SPI_INST)) {
        }
        uint8_t r = DL_SPI_receiveData8(SPI_INST);
        if (dest) {
            dest[i] = r;
        }
    }
    spi_wait_done();
}

static const mp_arg_t spi_init_args[] = {
    { MP_QSTR_baudrate, MP_ARG_INT, {.u_int = 8000000} },
    { MP_QSTR_polarity, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
    { MP_QSTR_phase, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
    { MP_QSTR_bits, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 8} },
    { MP_QSTR_firstbit, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
    { MP_QSTR_sck, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
    { MP_QSTR_mosi, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
    { MP_QSTR_miso, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
};

static void spi_configure(machine_spi_obj_t *self, size_t n_args, const mp_obj_t *pos, mp_map_t *kw) {
    mp_arg_val_t a[MP_ARRAY_SIZE(spi_init_args)];
    mp_arg_parse_all(n_args, pos, kw, MP_ARRAY_SIZE(spi_init_args), spi_init_args, a);
    if (a[0].u_int < 100000 || a[0].u_int > 40000000) {
        mp_raise_ValueError(MP_ERROR_TEXT("baudrate 100000 to 40000000"));
    }
    if (a[3].u_int != 8 || a[4].u_int != 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("8 bits, MSB first only"));
    }
    if (a[5].u_obj != MP_OBJ_NULL) {
        spi_pin_check(a[5].u_obj, 1, 31);
    }
    if (a[6].u_obj != MP_OBJ_NULL) {
        spi_pin_check(a[6].u_obj, 1, 8);
    }
    if (a[7].u_obj != MP_OBJ_NULL) {
        spi_pin_check(a[7].u_obj, 1, 7);
    }
    self->baudrate = a[0].u_int;
    self->polarity = a[1].u_int & 1;
    self->phase = a[2].u_int & 1;
    spi_apply(self);
}

// SPI(id, ...): id must be 1
static mp_obj_t spi_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, MP_OBJ_FUN_ARGS_MAX, true);
    if (mp_obj_get_int(args[0]) != 1) {
        mp_raise_ValueError(MP_ERROR_TEXT("only SPI(1)"));
    }
    mp_map_t kw;
    mp_map_init_fixed_table(&kw, n_kw, args + n_args);
    spi_configure(&spi_obj, n_args - 1, args + 1, &kw);
    return MP_OBJ_FROM_PTR(&spi_obj);
}

static mp_obj_t spi_init_m(size_t n_args, const mp_obj_t *args, mp_map_t *kw) {
    spi_configure(MP_OBJ_TO_PTR(args[0]), n_args - 1, args + 1, kw);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(spi_init_obj, 1, spi_init_m);

static mp_obj_t spi_deinit_m(mp_obj_t self_in) {
    machine_spi_obj_t *self = MP_OBJ_TO_PTR(self_in);
    DL_SPI_disable(SPI_INST);
    self->ready = 0;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(spi_deinit_obj, spi_deinit_m);

static mp_obj_t spi_write_m(mp_obj_t self_in, mp_obj_t buf) {
    mp_buffer_info_t b;
    mp_get_buffer_raise(buf, &b, MP_BUFFER_READ);
    spi_flush_rx();
    spi_send(b.buf, b.len);
    spi_wait_done();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(spi_write_obj, spi_write_m);

// write_repeat(buf, n): send buf n times
static mp_obj_t spi_write_repeat_m(mp_obj_t self_in, mp_obj_t buf, mp_obj_t n_in) {
    mp_buffer_info_t b;
    mp_get_buffer_raise(buf, &b, MP_BUFFER_READ);
    mp_int_t n = mp_obj_get_int(n_in);
    spi_flush_rx();
    while (n-- > 0) {
        spi_send(b.buf, b.len);
    }
    spi_wait_done();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(spi_write_repeat_obj, spi_write_repeat_m);

static mp_obj_t spi_readinto_m(size_t n_args, const mp_obj_t *args) {
    mp_buffer_info_t b;
    mp_get_buffer_raise(args[1], &b, MP_BUFFER_WRITE);
    spi_transfer(NULL, b.buf, b.len, n_args > 2 ? mp_obj_get_int(args[2]) : 0);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(spi_readinto_obj, 2, 3, spi_readinto_m);

static mp_obj_t spi_read_m(size_t n_args, const mp_obj_t *args) {
    vstr_t vstr;
    vstr_init_len(&vstr, mp_obj_get_int(args[1]));
    spi_transfer(NULL, (uint8_t *)vstr.buf, vstr.len, n_args > 2 ? mp_obj_get_int(args[2]) : 0);
    return mp_obj_new_bytes_from_vstr(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(spi_read_obj, 2, 3, spi_read_m);

static mp_obj_t spi_write_readinto_m(mp_obj_t self_in, mp_obj_t wr, mp_obj_t rd) {
    mp_buffer_info_t w, r;
    mp_get_buffer_raise(wr, &w, MP_BUFFER_READ);
    mp_get_buffer_raise(rd, &r, MP_BUFFER_WRITE);
    if (w.len != r.len) {
        mp_raise_ValueError(MP_ERROR_TEXT("buffers must be the same length"));
    }
    spi_transfer(w.buf, r.buf, w.len, 0);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(spi_write_readinto_obj, spi_write_readinto_m);

static void spi_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    machine_spi_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "SPI(1, baudrate=%u, polarity=%u, phase=%u)", (unsigned)self->baudrate, self->polarity, self->phase);
}

static const mp_rom_map_elem_t spi_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&spi_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&spi_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&spi_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_write_repeat), MP_ROM_PTR(&spi_write_repeat_obj) },
    { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&spi_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto), MP_ROM_PTR(&spi_readinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_write_readinto), MP_ROM_PTR(&spi_write_readinto_obj) },
};
static MP_DEFINE_CONST_DICT(spi_locals_dict, spi_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    machine_spi_type,
    MP_QSTR_SPI,
    MP_TYPE_FLAG_NONE,
    make_new, spi_make_new,
    print, spi_print,
    locals_dict, &spi_locals_dict
    );
#endif
