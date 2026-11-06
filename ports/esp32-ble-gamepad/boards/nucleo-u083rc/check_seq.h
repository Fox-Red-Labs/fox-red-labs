/* The job's call sequence, shared by host test and firmware. Fills out[0..3]; lens[]. */
#include "gamepad_core.h"
#include <string.h>
static void frl_run_sequence(gp_t *g, uint8_t out[4][GP_REPORT_MAX], int lens[4])
{
  gp_config_t c; gp_default_config(&c);
#ifdef FRL_START_SELECT
  c.whichSpecial[START_BUTTON] = 1; c.whichSpecial[SELECT_BUTTON] = 1;
#endif
  gp_begin(g, &c);
  lens[0] = gp_pack(g, out[0]);
  gp_press(g, BUTTON_5); gp_press(g, BUTTON_16); gp_press_start(g);
  gp_set_axes(g, 32767,32767,32767,32767,32767,32767,32767,32767); gp_set_hat1(g, HAT_DOWN_RIGHT);
  lens[1] = gp_pack(g, out[1]);
  gp_release(g, BUTTON_5); gp_release_start(g); gp_set_hat1(g, HAT_CENTERED);
  gp_set_axes(g, 0,0,0,0,0,0,0,0);
  lens[2] = gp_pack(g, out[2]);
  gp_press(g, BUTTON_1); gp_press(g, BUTTON_8); gp_press(g, BUTTON_9); gp_press_select(g);
  gp_set_axes(g, -32768,-1,1,255,256,-256,12345,-12345); gp_set_hat1(g, HAT_UP_LEFT);
  lens[3] = gp_pack(g, out[3]);
}
