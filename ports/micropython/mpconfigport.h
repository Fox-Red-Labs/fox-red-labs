// MicroPython port for the TI MSPM0G3519 and MSPM0G3507 (Arm Cortex-M0+).
// Two builds from one file: the full build, and the minimal build (-DFRL_MIN).
#include <stdint.h>

#ifdef FRL_MIN
// Minimal: the smallest feature level, no floating point, no machine.Pin extras.
#define MICROPY_CONFIG_ROM_LEVEL            (MICROPY_CONFIG_ROM_LEVEL_MINIMUM)
#define MICROPY_PY_BUILTINS_FLOAT           (0)
#define MICROPY_PY_MACHINE_SOFTI2C          (0)
#define MICROPY_PY_MACHINE_SOFTSPI          (0)
#define MICROPY_ENABLE_EXTERNAL_IMPORT      (0)
#define MICROPY_PY_TIME                     (1)
#define MICROPY_PY_SYS_STDFILES             (0)
#define MICROPY_KBD_EXCEPTION               (1)
#define MICROPY_ENABLE_SCHEDULER            (1)
#define MICROPY_SCHEDULER_DEPTH             (2)
#define MICROPY_REPL_AUTO_INDENT            (0)
#define MICROPY_HELPER_REPL                 (1)
#define MICROPY_ERROR_REPORTING             (MICROPY_ERROR_REPORTING_TERSE)
#define MICROPY_BUILD_TYPE                  "minimal"
#else
// Full: core plus extra features, single-precision floats, soft I2C and SPI.
#define MICROPY_CONFIG_ROM_LEVEL            (MICROPY_CONFIG_ROM_LEVEL_EXTRA_FEATURES)
#define MICROPY_FLOAT_IMPL                  (MICROPY_FLOAT_IMPL_FLOAT)
#define MICROPY_PY_MACHINE_SOFTI2C          (1)
#define MICROPY_PY_MACHINE_SOFTSPI          (1)
#define MICROPY_ENABLE_EXTERNAL_IMPORT      (0)
#define MICROPY_ENABLE_SCHEDULER            (1)
#define MICROPY_REPL_AUTO_INDENT            (1)
#define MICROPY_HELPER_REPL                 (1)
#define MICROPY_PY_SYS_STDFILES             (1)
#define MICROPY_ERROR_REPORTING             (MICROPY_ERROR_REPORTING_NORMAL)
#define MICROPY_BUILD_TYPE                  "full"
#endif

#define MICROPY_ENABLE_COMPILER             (1)
#define MICROPY_ENABLE_GC                   (1)
#define MICROPY_ALLOC_PATH_MAX              (64)
#define MICROPY_ALLOC_PARSE_CHUNK_INIT      (16)
#define MICROPY_PERSISTENT_CODE_LOAD        (0)
#define MICROPY_VFS                         (0)
#define MICROPY_READER_VFS                  (0)
#define MICROPY_HW_ENABLE_USB               (0)
#define MICROPY_STACK_CHECK                 (1)
#define MICROPY_PY_GC                       (1)

// sys: the port has no file system, so no sys.path or sys.argv
#define MICROPY_PY_SYS_MODULES              (0)
#define MICROPY_PY_SYS_PATH                 (0)
#define MICROPY_PY_SYS_ARGV                 (0)
#define MICROPY_PY_SYS_EXIT                 (1)

// machine module: Pin written for this port, SoftI2C and SoftSPI from the shared code
#define MICROPY_PY_MACHINE                  (1)
#define MICROPY_PY_MACHINE_INCLUDEFILE      "port_machine.inc"
#define MICROPY_PY_MACHINE_BARE_METAL_FUNCS (1)
#define MICROPY_PY_MACHINE_RESET            (1)
#define MICROPY_PY_MACHINE_SIGNAL           (0)
#define MICROPY_PY_MACHINE_I2C              (0)
#define MICROPY_PY_MACHINE_SPI              (0)
#define MICROPY_PY_MACHINE_MEMX             (1)
#if defined(FRL_G3519) && !defined(FRL_MIN)
#define MICROPY_PY_MACHINE_EXTRA_GLOBALS    { MP_ROM_QSTR(MP_QSTR_Pin), MP_ROM_PTR(&machine_pin_type) }, \
    { MP_ROM_QSTR(MP_QSTR_SPI), MP_ROM_PTR(&machine_spi_type) },
extern const struct _mp_obj_type_t machine_spi_type;
#else
#define MICROPY_PY_MACHINE_EXTRA_GLOBALS    { MP_ROM_QSTR(MP_QSTR_Pin), MP_ROM_PTR(&machine_pin_type) },
#endif
#define MICROPY_PY_MACHINE_DISABLE_IRQ_ENABLE_IRQ (1)

#define MICROPY_PY_TIME_TICKS_PERIOD        (0x40000000)

#define MICROPY_HW_BOARD_NAME               FRL_BOARD_NAME
#define MICROPY_HW_MCU_NAME                 FRL_MCU_NAME

// Types
typedef intptr_t mp_int_t;
typedef uintptr_t mp_uint_t;
typedef long mp_off_t;

#include <alloca.h>

#define MP_STATE_PORT MP_STATE_VM

// Interrupt masking for the scheduler and the UART receive ring
#define MICROPY_BEGIN_ATOMIC_SECTION()      frl_irq_disable()
#define MICROPY_END_ATOMIC_SECTION(state)   frl_irq_restore(state)
#define MICROPY_EVENT_POLL_HOOK             __asm volatile ("wfi");

extern const struct _mp_obj_type_t machine_pin_type;
uint32_t frl_irq_disable(void);
void frl_irq_restore(uint32_t state);
