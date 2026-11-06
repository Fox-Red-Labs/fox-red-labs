/* DRV5055 (SBAS640): ratiometric analog output. At 0 mT the output is half the supply; sensitivity scales with the supply. */
#include "bassens.h"

int bs_drv5055_read_mv(int32_t *mv)
{
    int32_t v = bs_port_adc_drv_mv();
    if (v < 0) return -1;
    *mv = v;
    return 0;
}

int32_t bs_drv5055_dmt(int32_t mv, int32_t rest_mv, int32_t sens_uv_per_mt)
{
    /* tenths of a milli-tesla = (mv - rest) * 1000 uV / sens * 10 */
    return (mv - rest_mv) * 10000 / sens_uv_per_mt;
}
