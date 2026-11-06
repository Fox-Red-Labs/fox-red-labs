/* Mac (or any host) reference run of the same front end: same sources, same fixed-point build. Writes the report lines
 * to stdout and the echo canceller's and final output samples to aec.raw and out.raw (int16, little-endian). */
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include "fe_run.h"
static FILE *fa, *fo;
uint32_t plat_ticks(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (uint32_t)((uint64_t)t.tv_sec * 1000000000u + (uint64_t)t.tv_nsec); }
void plat_log(const char *f, ...) { va_list a; va_start(a, f); vprintf(f, a); va_end(a); }
static void sink(const int16_t *aec, const int16_t *out, int n) { fwrite(aec, 2, (size_t)n, fa); fwrite(out, 2, (size_t)n, fo); }
int main(int argc, char **argv)
{
	const char *d = argc > 1 ? argv[1] : ".";
	char p[1024];
	snprintf(p, sizeof p, "%s/aec.raw", d); fa = fopen(p, "wb");
	snprintf(p, sizeof p, "%s/out.raw", d); fo = fopen(p, "wb");
	if (!fa || !fo) { perror("open"); return 1; }
	fe_main(1000000000u, sink);
	fclose(fa); fclose(fo);
	return 0;
}
