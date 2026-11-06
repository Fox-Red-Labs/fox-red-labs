/* BOOSTXL-AUDIO portable core: the three DAC8311 lines as functions a board supplies.
 * Written fresh from the DAC8311 data sheet (SLASE23) and SLAU670A Figure 12; no vendor example code. */
#ifndef BA_HAL_H
#define BA_HAL_H
#include <stdint.h>

typedef struct {
    void (*sync)(int level); /* DAC8311 SYNC (chip select), BoosterPack pin 12 */
    void (*sclk)(int level); /* serial clock, pin 7 */
    void (*din)(int level);  /* serial data in (host MOSI), pin 15 */
} ba_hal_t;

#endif
