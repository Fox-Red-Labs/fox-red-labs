/* Run the machine for N frames and write the 6,912 screen bytes plus the Z80 registers: dump rom.bin frames out.bin. Prints offsetof(zx_t, ram) and the struct size. */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include "../src/zx.h"
static zx_t z; static uint8_t rom[16384];
int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "rb"); if (!f || fread(rom, 1, 16384, f) != 16384) return 2; fclose(f);
    zx_init(&z, rom); for (int i = atoi(argv[2]); i; i--) zx_run_frame(&z);
    f = fopen(argv[3], "wb"); fwrite(z.ram, 1, 6912, f); fclose(f);
    printf("ram_offset=%zu size=%zu pc=%04X\n", offsetof(zx_t, ram), sizeof z, z.cpu.pc);
    return 0;
}
