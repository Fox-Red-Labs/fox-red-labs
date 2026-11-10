// Entry for the NoRTOS image: SDK start-up, then the sketch's setup() and loop().
#include "Arduino.h"
#include "frl_hal.h"
extern "C" {
#include "ti_drivers_config.h"
#include "ti_board_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
}
void setup(void);
void loop(void);
int main(void) {
  System_init(); Board_init(); Drivers_open(); Board_driversOpen();
  uint32_t pmcr = 0x5; __asm volatile("mcr p15, 0, %0, c9, c12, 0" :: "r"(pmcr));
  uint32_t en = 0x80000000u; __asm volatile("mcr p15, 0, %0, c9, c12, 1" :: "r"(en));
  hal_init();
  setup();
  for (;;) loop();
}
