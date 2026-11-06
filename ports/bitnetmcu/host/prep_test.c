/* Mac check of the touch demo's drawing-to-input step: each test digit drawn into the grid
   (pixels above 40 become ink), prepared, classified. Expect most of the 10 to read right. */
#include <stdio.h>
#include "../bitnet_run.c"
#include "../touch/prepare.h"
int main(void)
{
    int right = 0;
    for (int t = 0; t < N_TEST; t++) {
        int8_t in[256];
        for (unsigned i = 0; i < sizeof grid; i++) grid[i] = 0;
        for (int y = 0; y < N; y++)
            for (int x = 0; x < N; x++)
                if (test_in[t][(y * 16 / N) * 16 + x * 16 / N] > 40) SET(x, y);
        prepare(in);
        uint32_t p = BitMnistInference(in);
        right += p == *test_label[t];
        printf("%d", (int) p);
    }
    printf("  (%d of %d read right through the drawing path)\n", right, N_TEST);
}
