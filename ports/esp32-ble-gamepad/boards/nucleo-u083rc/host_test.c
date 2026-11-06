#include <stdio.h>
#include "check_seq.h"
static void hex(const uint8_t *p, int n) { for (int i = 0; i < n; i++) printf("%02x", p[i]); }
int main(void) {
  gp_t g; uint8_t out[4][GP_REPORT_MAX]; int lens[4];
  frl_run_sequence(&g, out, lens);
  printf("MAP len=%d ", g.descSize); hex(g.desc, g.descSize); printf("\n");
  for (int i = 0; i < 4; i++) { printf("RPT%d len=%d ", i+1, lens[i]); hex(out[i], lens[i]); printf("\n"); }
  return 0;
}
