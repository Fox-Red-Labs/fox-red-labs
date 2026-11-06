/* ---- Tool telemetry, added by tool-telemetry/tool/am13e230/apply.py (PROTOCOL.md section 1) ----
 * The motor firmware becomes the "tool MCU": every go-to-stop run is one trigger pull, run time is counted while the
 * bridge is driving, a driver fault (nFAULT) is a fault, and the firmware's own stall trip (no Hall edge for 1.5 s while
 * driving: a jammed shaft) is counted as an overcurrent trip, because the EVM's current sense is not validated yet.
 * Lines go out on the same UART as the console, at 115200; the tag ignores everything that does not start with '$'.
 * Totals restart at 0 when this MCU restarts (no flash storage yet); a TE boot line says so. */
#include <string.h>
#define TT_ID "BLDC-01"
static uint32_t tt_n, tt_trig, tt_oc, tt_flt, tt_ms, tt_run_ms, tt_acc, tt_last, tt_pull_ms, tt_next_ts, tt_rpmx;
static bool tt_was_running, tt_booted, tt_locked;

static void tt_put(const char *s, uint8_t *x) { while (*s) { *x ^= (uint8_t)*s; putc_(*s++); } }
static void tt_num(uint32_t v, uint8_t *x) { char b[11]; int i = 0; do { b[i++] = (char)('0' + v % 10U); v /= 10U; } while (v); while (i) { char c = b[--i]; *x ^= (uint8_t)c; putc_(c); } }
static void tt_kv(const char *k, uint32_t v, uint8_t *x) { tt_put(",", x); tt_put(k, x); tt_put("=", x); tt_num(v, x); }
static void tt_begin(const char *type, uint8_t *x) { putc_('$'); *x = 0; tt_put(type, x); tt_put(",n=", x); tt_num(tt_n++ & 0xFFFFU, x); }
static void tt_end(uint8_t x) { static const char H[] = "0123456789ABCDEF"; putc_('*'); putc_(H[x >> 4]); putc_(H[x & 15U]); putc_('\r'); putc_('\n'); }

static void tt_event(const char *ev, uint32_t v)
{
    uint8_t x;
    tt_begin("TE", &x); tt_put(",ev=", &x); tt_put(ev, &x); tt_kv("v", v > 65535U ? 65535U : v, &x); tt_end(x);
}

static void tt_status(void)
{
    uint8_t x;
    uint32_t a, vm, rpm = g_rpm;
    if (rpm > tt_rpmx) tt_rpmx = rpm;
    adc_read(&a, &vm);
    tt_begin("TS", &x);
    tt_put(",id=" TT_ID, &x);
    tt_kv("trig", tt_trig, &x); tt_kv("oc", tt_oc, &x); tt_kv("flt", tt_flt, &x); tt_kv("run", tt_run_ms / 1000U, &x);
    tt_kv("rpm", running ? rpm : 0U, &x); tt_kv("ipk", 0U, &x);
    tt_kv("vb", (uint32_t)counts_to_mv(vm), &x);
    tt_kv("st", g_fault ? 2U : tt_locked ? 3U : running ? 1U : 0U, &x);
    tt_end(x);
}

/* Once per pass of the main loop. */
static void tt_tick(uint32_t now)
{
    uint32_t ms;
    if (!tt_booted) { tt_booted = true; tt_last = now; tt_event("boot", 0); tt_status(); tt_next_ts = 10000U; }
    tt_acc += now - tt_last; tt_last = now;
    ms = tt_acc / (CPU_HZ / 1000U); tt_acc -= ms * (CPU_HZ / 1000U);
    tt_ms += ms;
    if (running) { tt_run_ms += ms; if (g_rpm > tt_rpmx) tt_rpmx = g_rpm; }
    if (running && !tt_was_running) { tt_trig++; tt_pull_ms = tt_ms; tt_status(); tt_next_ts = tt_ms + 1000U; }
    if (!running && tt_was_running) { tt_event("trig", tt_ms - tt_pull_ms); tt_status(); tt_next_ts = tt_ms + 10000U; }
    tt_was_running = running;
    if ((int32_t)(tt_ms - tt_next_ts) >= 0) { tt_status(); tt_next_ts = tt_ms + (running ? 1000U : 10000U); }
}
static void tt_fault(void) { tt_flt++; tt_event("flt", 1); }

/* Stage two: TC lines from the tag (it checked their signature). Console keys are single letters, so a line that
   starts with '$' is collected whole and never read as keys. Lock stops the motor and refuses 'g' until unlock; stop
   stops it. A remote run is refused on purpose: starting a motor from the network needs a person at the tool. */
static bool tt_rx(int c)
{
    static char b[48];
    static uint32_t n;
    static bool in;
    uint8_t x = 0;
    uint32_t i;
    if (c == '$') { in = true; n = 0; return true; }
    if (!in) return false;
    if (c != '\n' && c != '\r') { if (n < sizeof(b) - 1U) b[n++] = (char)c; else in = false; return true; }
    in = false; b[n] = 0;
    for (i = 0; i < n && b[i] != '*'; i++) x ^= (uint8_t)b[i];
    if (i + 2U >= n) return true;                     /* no "*XX" after the body */
    {
        static const char H[] = "0123456789ABCDEF";
        if (b[i + 1] != H[x >> 4] || b[i + 2] != H[x & 15U]) return true;   /* bad checksum: ignore the line */
    }
    b[i] = 0;
    if (!strcmp(b, "TC,cmd=lock")) { stop(); tt_locked = true; puts_("locked by the fleet\r\n"); tt_status(); }
    else if (!strcmp(b, "TC,cmd=unlock")) { tt_locked = false; puts_("unlocked by the fleet\r\n"); tt_status(); }
    else if (!strcmp(b, "TC,cmd=stop")) { stop(); puts_("stopped by the fleet\r\n"); tt_status(); }
    else if (!strncmp(b, "TC,cmd=run", 10)) { puts_("remote run refused: start it at the tool\r\n"); }
    return true;
}
static void tt_stall(void) { tt_oc++; tt_event("oc", 0); }
/* ---- end of tool telemetry ---- */

