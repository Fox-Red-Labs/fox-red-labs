#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <stdint.h>
extern volatile uint32_t gFFTmaxValue, gFFTmaxFreqIndex;
static void report(int sig) { (void)sig; printf("gFFTmaxFreqIndex=%u gFFTmaxValue=%u %s\n", (unsigned)gFFTmaxFreqIndex, (unsigned)gFFTmaxValue, gFFTmaxFreqIndex == 5 ? "PASS" : "UNEXPECTED"); exit(gFFTmaxFreqIndex == 5 ? 0 : 2); }
void host_init(void) { signal(SIGALRM, report); alarm(1); }
void host_bkpt(int code) { printf("FAIL: __BKPT(%d): gFFTmaxFreqIndex=%u\n", code, (unsigned)gFFTmaxFreqIndex); exit(1); }
