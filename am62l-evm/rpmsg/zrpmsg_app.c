/* Userspace round trip through /dev/zrpmsg: write one message, read the echo, time it with CLOCK_MONOTONIC.
 * Usage: zrpmsg_app SIZE COUNT   (SIZE payload bytes, 56 to 496). Prints median, min, p99, max in ns for COUNT round trips. */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
static int cmp(const void *a, const void *b) { double x = *(double *)a, y = *(double *)b; return x < y ? -1 : x > y; }
int main(int argc, char **argv)
{
	int size = argc > 1 ? atoi(argv[1]) : 64, n = argc > 2 ? atoi(argv[2]) : 1000, i, bad = 0;
	char buf[512] = {0};
	double *v = malloc(sizeof(double) * n);
	int fd = open("/dev/zrpmsg", O_RDWR);
	struct timespec a, b;

	if (fd < 0) { perror("open"); return 1; }
	for (i = 0; i < n + 50; i++) {
		clock_gettime(CLOCK_MONOTONIC, &a);
		if (write(fd, buf, size) != size) { perror("write"); return 1; }
		if (read(fd, buf, sizeof buf) < size) { bad++; }
		clock_gettime(CLOCK_MONOTONIC, &b);
		if (i >= 50) v[i - 50] = (b.tv_sec - a.tv_sec) * 1e9 + (b.tv_nsec - a.tv_nsec);
	}
	qsort(v, n, sizeof(double), cmp);
	printf("userspace write+read size=%d n=%d bad=%d median %.0f min %.0f p99 %.0f max %.0f ns\n", size, n, bad, v[n / 2], v[0], v[n * 99 / 100], v[n - 1]);
	return 0;
}
