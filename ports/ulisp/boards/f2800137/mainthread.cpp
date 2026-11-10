// Entry for the NoRTOS main() of the bench: init the board, then run the sketch's setup() and loop().
#include "Arduino.h"
#include "frl_hal.h"
void setup(void);
void loop(void);
int main(void) {
  hal_init();
  setup();
  for (;;) loop();
}
