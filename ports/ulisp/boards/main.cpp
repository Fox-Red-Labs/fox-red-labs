// Entry point: Arduino's main() calls setup() once and loop() forever.
#include "Arduino.h"
#include "frl_hal.h"
void setup(void);
void loop(void);
int main(void) {
  hal_init();
  setup();
  for (;;) loop();
}
