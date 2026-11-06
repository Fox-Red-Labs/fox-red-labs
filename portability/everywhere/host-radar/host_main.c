/* Host program for the IWRL6432 frame parser (am13e230/radar-parser/tlv_parser.c): reads a UART capture file, feeds it one
   byte at a time, prints the same line per frame the LP-AM13E230 program prints:
     F<frame> L<length> O<objects> T<tlvs> P<byte0>,<zone0> N<points> R<nearest range in mm>   (-1 = absent)
   Plain C, no TI headers: builds on any host (Mac, the SK-TDA4VM's Linux) with cc.   host_main.c <capture.bin> */
#include <stdio.h>
#include <stdint.h>
#include "tlv_parser.h"
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s capture.bin\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "rb"); if (!f) { perror(argv[1]); return 2; }
    static tlv_parser_t P; tlv_init(&P); int c; unsigned long bytes = 0, frames = 0;
    while ((c = fgetc(f)) != EOF) {
        bytes++;
        if (tlv_feed(&P, (uint8_t)c)) {
            const tlv_frame_t *r = &P.f; frames++;
            printf("F%u L%u O%u T%u P%d,%d N%u R%d\n", r->frame, r->length, r->objs, r->ntlv, r->pres_b0, r->pres_z0, r->points, r->rmin_mm);
        }
    }
    fclose(f); fprintf(stderr, "%lu bytes, %lu frames\n", bytes, frames); return 0;
}
