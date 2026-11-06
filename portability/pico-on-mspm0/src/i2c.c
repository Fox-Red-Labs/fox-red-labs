/* Pico SDK on MSPM0: hardware/i2c.h, blocking and polled. A read is sent with the controller's ACK bit off, so the controller
 * NACKs the last byte and the read ends; with it on, the slave keeps sending and the bus stays busy. */
#include "ti_msp_dl_config.h"
#include "hardware/i2c.h"
#include "hardware/timer.h"
struct i2c_inst { I2C_Regs *hw; };
static struct i2c_inst s_i2c0 = { PICO_I2C0_INST };
i2c_inst_t *const i2c0 = &s_i2c0;
uint i2c_init(i2c_inst_t *i2c, uint baudrate)              /* SCL period = 10 * (TPR + 1) I2C clocks; the I2C clock is the 32 MHz bus clock */
{
    uint32_t tpr = CPUCLK_FREQ / (10u * baudrate); if (tpr) { tpr--; }
    DL_I2C_setTimerPeriod(i2c->hw, tpr);
    return CPUCLK_FREQ / (10u * (tpr + 1u));
}
#define DONE_US 10000u                                     /* longest wait for one transfer, then PICO_ERROR_GENERIC */
static bool wait_idle(I2C_Regs *hw, uint64_t until)
{
    while (DL_I2C_getControllerStatus(hw) & DL_I2C_CONTROLLER_STATUS_BUSY_BUS) { if (time_us_64() > until) { return false; } }
    return true;
}
int i2c_read_blocking(i2c_inst_t *i2c, uint8_t addr, uint8_t *dst, size_t len, bool nostop)
{
    I2C_Regs *hw = i2c->hw; uint64_t until = time_us_64() + DONE_US; size_t got = 0;
    if (len == 0 || len > 0xfff) { return PICO_ERROR_GENERIC; }
    if (!wait_idle(hw, until)) { return PICO_ERROR_GENERIC; }
    DL_I2C_startControllerTransferAdvanced(hw, addr, DL_I2C_CONTROLLER_DIRECTION_RX, (uint16_t) len, DL_I2C_CONTROLLER_START_ENABLE,
        nostop ? DL_I2C_CONTROLLER_STOP_DISABLE : DL_I2C_CONTROLLER_STOP_ENABLE, DL_I2C_CONTROLLER_ACK_DISABLE);
    while (got < len) {
        if (!DL_I2C_isControllerRXFIFOEmpty(hw)) { dst[got++] = DL_I2C_receiveControllerData(hw); continue; }
        if (DL_I2C_getControllerStatus(hw) & DL_I2C_CONTROLLER_STATUS_ERROR) { break; }
        if (!(DL_I2C_getControllerStatus(hw) & DL_I2C_CONTROLLER_STATUS_BUSY)) { if (DL_I2C_isControllerRXFIFOEmpty(hw)) { break; } }
        if (time_us_64() > until) { break; }
    }
    while ((DL_I2C_getControllerStatus(hw) & DL_I2C_CONTROLLER_STATUS_BUSY) && time_us_64() <= until) { }
    bool err = DL_I2C_getControllerStatus(hw) & DL_I2C_CONTROLLER_STATUS_ERROR;
    while (!DL_I2C_isControllerRXFIFOEmpty(hw)) { (void) DL_I2C_receiveControllerData(hw); }
    return (err || got < len) ? PICO_ERROR_GENERIC : (int) got;
}
