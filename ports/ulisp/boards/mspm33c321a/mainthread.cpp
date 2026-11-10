// Entry for the no-RTOS image: init the board (clocks, UART, SysTick), then run the sketch's setup() and loop().
#include "Arduino.h"
#include "frl_hal.h"
void setup(void);
void loop(void);
extern "C" int main(void) {
  hal_init();
  setup();
  for (;;) loop();
}
