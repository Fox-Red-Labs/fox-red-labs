#include "board.h"
#include "check_seq.h"

static void puts_(const char *s) { while (*s) board_putc(*s++); }
static void putu(uint32_t v) { char b[11]; int n = 0; if (!v) b[n++] = '0'; while (v) { b[n++] = '0' + v % 10; v /= 10; } while (n) board_putc(b[--n]); }
static void puthex(const uint8_t *p, int n) { static const char h[] = "0123456789abcdef"; for (int i = 0; i < n; i++) { board_putc(h[p[i] >> 4]); board_putc(h[p[i] & 15]); } }

static gp_t g;
static uint8_t out[4][GP_REPORT_MAX];
static int lens[4];
static uint32_t desc_us, pack_us;

int main(void)
{
  board_init();
  frl_run_sequence(&g, out, lens);

  /* average of 200 descriptor builds, 1000 report packs (microseconds, rounded to nearest) */
  gp_config_t c = g.cfg;
  uint8_t tmp[GP_REPORT_MAX];
  uint32_t t0 = board_us();
  for (int i = 0; i < 200; i++) gp_begin(&g, &c);
  uint32_t dt = board_us() - t0;
  desc_us = (dt + 100) / 200;
  t0 = board_us();
  for (int i = 0; i < 1000; i++) { gp_pack(&g, tmp); __asm volatile("" ::: "memory"); }
  dt = board_us() - t0;
  pack_us = (dt + 500) / 1000;
  /* gp_begin cleared the state; rebuild the descriptor view only (reports were captured earlier) */

  for (;;) {
    puts_("FRL-GP1 begin\r\n");
    puts_("INFO heap_free="); putu(board_free_ram()); puts_(" uptime_ms="); putu(board_ms()); puts_("\r\n");
    puts_("TIME desc_us="); putu(desc_us); puts_(" pack_us="); putu(pack_us); puts_("\r\n");
    puts_("MAP len="); putu(g.descSize); board_putc(' '); puthex(g.desc, g.descSize); puts_("\r\n");
    for (int i = 0; i < 4; i++) {
      puts_("RPT"); putu(i + 1); puts_(" len="); putu(lens[i]); board_putc(' '); puthex(out[i], lens[i]); puts_("\r\n");
    }
    puts_("FRL-GP1 end\r\n");
    uint32_t until = board_ms() + 3000;
    while ((int32_t)(board_ms() - until) < 0) {}
  }
}
