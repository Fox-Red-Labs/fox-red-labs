// Entry for the NoRTOS main() of the bench: init the board, then run the sketch's setup() and loop().
#include "Arduino.h"
#include "frl_hal.h"
void setup(void);
void loop(void);
extern "C" void *mainThread(void *) {
  hal_init();
  setup();
  for (;;) loop();
}
