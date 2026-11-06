/* Host test for fom_sweep(): a simulated loop that misses periods shorter than a set limit, plus noisy and
   edge cases. Build and run: ./host_test.sh (prints OK or the failing case). */
#include <stdio.h>
#include "fom.h"

static uint32_t limit, flaky_at, calls;

static int sim(uint32_t period, uint32_t *why)
{
	calls++;
	if (period < limit) { *why = FOM_WHY_LATE | FOM_WHY_OVF; return 0; }
	if (flaky_at && period == flaky_at) { *why = FOM_WHY_WRITE; return (calls % 2) == 0; }   /* passes every other call */
	return 1;
}

static int check(const char *name, uint32_t lim, uint32_t flaky, uint32_t p_start, uint32_t p_min, uint32_t p_max,
		 uint32_t want_pass, uint32_t want_fail)
{
	volatile uint32_t w[FOM_WORDS] = {0};
	limit = lim; flaky_at = flaky; calls = 0;
	fom_sweep(&w[FOM_BLK(0)], sim, p_start, p_min, p_max);
	uint32_t got = w[FOM_BLK(0) + FOM_P_PASS], gf = w[FOM_BLK(0) + FOM_P_FAIL];
	int ok = got == want_pass && (want_fail == 0xFFFFFFFFu || gf == want_fail) && w[FOM_BLK(0) + FOM_TRIALS] == calls;
	printf("%-44s pass %5u fail %5u trials %2u confirm %u steps %u why %u  %s\n", name, (unsigned)got, (unsigned)gf,
	       (unsigned)w[FOM_BLK(0) + FOM_TRIALS], (unsigned)w[FOM_BLK(0) + FOM_CONFIRM], (unsigned)w[FOM_BLK(0) + FOM_STEPS],
	       (unsigned)w[FOM_BLK(0) + FOM_WHY_FAIL], ok ? "ok" : "WRONG");
	return ok;
}

int main(void)
{
	int ok = 1;
	/* L1306-like: 32 MHz timer, p_start 3,200 (10 kHz); loop needs 300 counts */
	ok &= check("fast loop: limit 300 from 3,200", 300, 0, 3200, 16, 65535, 300, 299);
	/* slow loop: needs 13,000 counts, start fails, doubles to 25,600 then searches down */
	ok &= check("slow loop: limit 13,000 from 3,200", 13000, 0, 3200, 16, 65535, 13000, 12999);
	/* exact power-of-two edge */
	ok &= check("limit exactly 1,600", 1600, 0, 3200, 16, 65535, 1600, 1599);
	/* faster than p_min: reports p_min, no failure seen */
	ok &= check("limit 5, floor 16", 5, 0, 3200, 16, 65535, 25, 0);
	/* never passes inside the timer: 0 */
	ok &= check("limit above the 16-bit timer", 70000, 0, 3200, 16, 65535, 0, 0xFFFFFFFFu);
	/* flaky at the edge: the confirmation steps up one count (1% of 300 is 3) */
	ok &= check("flaky at the found edge 300", 300, 300, 3200, 16, 65535, 303, 299);
	/* the load pass's period: 100 kHz (320 counts at 32 MHz) when the highest rate is 200 kHz or more, else twice
	   the shortest passing period, capped at 65,535 */
	{
		struct { uint32_t pass, want; const char *name; } L[] = {
			{ 100, 320, "load: holds 320 kHz, runs at 100 kHz" }, { 160, 320, "load: holds 200 kHz exactly, 100 kHz" },
			{ 200, 400, "load: holds 160 kHz, runs at 80 kHz" }, { 13000, 26000, "load: holds 2.5 kHz, runs at 1.2 kHz" },
			{ 40000, 65535, "load: capped at the 16-bit timer" }, { 0, 0, "load: no rate, no load pass" },
			{ 200, 320, "load negative control (expects 320, must say WRONG)" } };
		for (unsigned i = 0; i < sizeof L / sizeof L[0]; i++) {
			volatile uint32_t w[FOM_WORDS] = {0};
			w[FOM_BLK(0) + FOM_P_PASS] = L[i].pass;
			uint32_t got = fom_load_period(&w[FOM_BLK(0)], 320);
			int good = got == L[i].want;
			printf("%-44s period %5u  %s\n", L[i].name, (unsigned)got, good ? "ok" : "WRONG");
			ok &= (i + 1 < sizeof L / sizeof L[0]) ? good : !good;
		}
	}
	/* the check itself, on a known bad case: a wrong expectation must be caught */
	ok &= !check("negative control (expects 301, must say WRONG)", 300, 0, 3200, 16, 65535, 301, 299);
	puts(ok ? "OK" : "FAIL");
	return ok ? 0 : 1;
}
