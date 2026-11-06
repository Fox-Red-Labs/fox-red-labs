/* Field data: the generic frame any app can send through a tag and a hub. See ../README.md.
 *
 * UART line, device to tag:  $FD,n=<counter>,app=<app id>,<field id>=<integer>,...*HH
 * Advertising data:          FF FF 'F' 'R' 0x21 <app id> <seq> then per field: <id> <len 1|2|4> <signed little-endian>
 * The tag and the hub never need to know what a field means; the app's schema (apps/<name>.json) does. */
#ifndef FD_FRAME_H
#define FD_FRAME_H

#include <stdint.h>
#include <stddef.h>

#define FD_VT          0x21u
#define FD_MAX_FIELDS  16

typedef struct {
    uint8_t  app;
    uint8_t  n;
    uint8_t  id[FD_MAX_FIELDS];
    int32_t  v[FD_MAX_FIELDS];
} fd_fields_t;

/* Parse one $FD line (checksum as in tool-telemetry/PROTOCOL.md). 0 ok, -1 framing, -2 checksum, -3 not an FD line.
 * *seqn gets the line counter, or -1. Later fields with the same id replace earlier ones. */
int fd_parse_line(const char *s, size_t len, fd_fields_t *out, int32_t *seqn);

/* Manufacturer data AD structure (length, 0xFF, company, magic, ...). Returns bytes written, 0 if it does not fit. */
size_t fd_build_ad(uint8_t *out, size_t cap, uint8_t seq, const fd_fields_t *f);

/* Decode manufacturer data that starts at the company ID. 0 ok, -1 not a field-data frame. */
int fd_decode_msd(const uint8_t *m, size_t len, fd_fields_t *out, uint8_t *seq);

#endif
