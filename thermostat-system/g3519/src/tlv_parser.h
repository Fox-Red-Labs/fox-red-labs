/* Streaming parser for the IWRL6432 demo UART stream: magic word 02 01 04 03 06 05 08 07, a 40-byte header,
   then TLVs (type u32, length u32, payload). One byte at a time, no allocation, about 100 bytes of state.
   Same rules as iwrl6432boost/tools/tlv_parse.py: a frame is the bytes from the magic word to the magic word plus
   totalPacketLen; a header whose length is under 40 or over 65536 is skipped and the scan resumes just after the magic
   word; TLVs are read while 8 header bytes fit inside the frame. Plain C, no TI headers. */
#ifndef TLV_PARSER_H
#define TLV_PARSER_H
#include <stdint.h>

typedef struct {
    uint32_t version, length, platform, frame, cycles, objs, ntlv, subframe;
    int32_t  pres_b0, pres_z0;   /* TLV 315: first two payload bytes, -1 when absent or shorter */
    uint32_t points;             /* TLV 1: whole 16-byte points (x, y, z, velocity as float32) */
    int32_t  rmin_mm;            /* nearest point, sqrt(x*x + y*y + z*z) in mm rounded, -1 when none */
    uint32_t pts301;             /* TLV 301 (fixed-point points of the Motion and Presence demo): whole 10-byte points */
    int32_t  near301_mm;         /* nearest of them in mm, -1 when none */
    int32_t  snr301_x10;         /* highest point SNR in tenths of a dB, -1 when none */
} tlv_frame_t;

typedef struct {
    uint8_t  st;
    uint8_t  m;                  /* magic bytes matched so far */
    uint8_t  hdr[40];
    uint8_t  hn;                 /* header bytes collected */
    uint32_t left;               /* bytes of the frame still to come */
    uint32_t tidx;               /* TLVs read */
    uint8_t  th[8];
    uint8_t  thn;
    uint32_t ttype, tlen, tgot;  /* current TLV: type, bytes of payload to take (capped at frame end), taken */
    uint8_t  pt[16];             /* a point being collected (TLV 1) */
    uint8_t  p3[2];              /* first two bytes of TLV 315 */
    uint8_t  u301[16];           /* TLV 301 unit floats: xyzUnit, dopplerUnit, snrUnit, noiseUnit */
    uint8_t  q301[10];           /* a 10-byte point being collected */
    float    r2_301; uint8_t snr301;
    float    rmin2;              /* smallest squared range seen */
    tlv_frame_t f;
    uint32_t frames_done;
} tlv_parser_t;

void tlv_init(tlv_parser_t *p);
/* Feed one byte. Returns 1 when it completed a frame (result in p->f), else 0. */
int  tlv_feed(tlv_parser_t *p, uint8_t b);
#endif
