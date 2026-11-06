/* Mac reference: the answer each model gives for each test digit. The board must give the same. */
#include <stdio.h>
#include "../bitnet_run.c"

int main(void)
{
    int right = 0;
    for (int i = 0; i < N_TEST; i++) {
        uint32_t p = BitMnistInference(test_in[i]);
        right += (p == *test_label[i]);
        printf("%d", (int) p);
    }
    printf("  (%d of %d match the label)\n", right, N_TEST);
    return 0;
}
