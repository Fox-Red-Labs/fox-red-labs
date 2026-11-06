#include <zephyr/kernel.h>
#include <stdio.h>
extern int coremark_main(void);
int main(void)
{
	printf("COREMARK START\n");
	coremark_main();
	printf("BENCH DONE\n");
	return 0;
}
