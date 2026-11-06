/* Tool telemetry wire formats, version 1. See ../PROTOCOL.md.
 * Shared by the tag firmware (parse UART lines, build advertising data) and the gateway firmware (decode advertising
 * data). No heap, no libc beyond memcpy/strlen, so it builds for any of the boards and on the host for the tests. */
#ifndef TT_PROTO_H
#define TT_PROTO_H

#include <stdint.h>
#include <stddef.h>

#define TT_COMPANY_ID       0xFFFFu
#define TT_MAGIC0           'F'
#define TT_MAGIC1           'R'
#define TT_VT_LEGACY        0x11u
#define TT_VT_EXT           0x12u
#define TT_LINE_MAX         160
#define TT_MAX_EVENTS       8
#define TT_MAX_PULLS        3       /* pull summaries per extended frame */
#define TT_LEGACY_LEN       25      /* advertising data bytes for set 1 */
#define TT_EXT_MAX          230     /* buffer size for set 2 (one AUX_ADV_IND holds up to 245 bytes of data) */

enum { TT_ST_IDLE = 0, TT_ST_RUN = 1, TT_ST_FAULT = 2, TT_ST_LOCKED = 3 };
enum { TT_EV_TRIG = 1, TT_EV_OC = 2, TT_EV_FLT = 3, TT_EV_TOOLBOOT = 4, TT_EV_TAGBOOT = 5, TT_EV_MODEL = 6, TT_EV_CMD = 7 };
/* MODEL: v is the version installed. CMD: a signed command passed to the tool, v = cmd | duty << 8 (cmd 1 stop, 2 lock,
   3 unlock, 4 run). */
enum { TT_F_RUN = 1, TT_F_FAULT = 2, TT_F_BATLOW = 4, TT_F_LINK = 8, TT_F_BAT = 16, TT_F_EXT = 32 };

typedef struct {
    char     id[9];
    uint32_t trig, run;
    uint16_t oc, flt, rpm, rpmx, ipk, vb;
    int8_t   tc;
    uint8_t  st;
    uint8_t  has_bat, soc;     /* soc 0xFF when has_bat is 0 */
    uint16_t cyc, bvb;
    int8_t   btc;
} tt_tool_t;

typedef struct {
    uint16_t id;
    uint8_t  type;
    uint32_t t;
    uint16_t v;
} tt_event_t;

/* One trigger pull, summarized by the tool MCU when it ends (TP line), numbered by the tag. The training unit. */
typedef struct {
    uint16_t id;       /* tag's running pull number */
    uint16_t dur10;    /* length, 10 ms units */
    uint16_t ravg, rmax;   /* rpm */
    uint16_t iavg, ipk;    /* phase current, mA (0 when the tool cannot measure it) */
    uint16_t vmin;     /* lowest supply during the pull, mV */
    int8_t   tc;       /* tool temperature at the end, deg C */
    uint8_t  cls;      /* load class: 0 light, 1 normal, 2 heavy, 3 jam; 255 not classified */
} tt_pull_t;

typedef struct {
    uint32_t up;
    uint16_t link;     /* 0xFFFF: never */
    uint16_t rx_ok;
    uint8_t  rx_bad;
    uint8_t  fw;
    uint16_t mv;       /* model version the tag runs: 0 the one built in, else the last one installed */
} tt_tagstat_t;

/* One parsed UART line. */
typedef struct {
    char     type[3];          /* "TS", "TB", "TE", "TC" */
    int32_t  n;                /* -1 if absent */
    uint8_t  ev;               /* TE: TT_EV_*, 0 unknown */
    uint16_t v;                /* TE */
    char     cmd[8];           /* TC */
    uint8_t  duty;             /* TC */
    tt_pull_t pull;            /* TP: id is left 0 for the tag to number; cls 255 unless the tool classified it */
} tt_line_t;

/* Returns 0 and fills *line (and updates *tool with any status keys) on a good line;
 * -1 framing, -2 checksum. *tool is only changed when the checksum is good. */
int tt_parse_line(const char *s, size_t len, tt_line_t *line, tt_tool_t *tool);

/* Builds a line: "$<type>,n=<n>,<body>*HH\n". body may be "" . Returns length, 0 if it does not fit. */
size_t tt_make_line(char *out, size_t cap, const char *type, int32_t n, const char *body);

uint8_t tt_flags(const tt_tool_t *tool, int link_alive, int ext_on);

/* Advertising data builders. Return the number of bytes written. */
size_t tt_build_legacy(uint8_t *out, uint8_t seq, const tt_tool_t *tool, uint8_t flags);
size_t tt_build_ext(uint8_t *out, size_t cap, uint8_t seq, const tt_tool_t *tool, const tt_event_t *ev, uint8_t nev,
                    const tt_pull_t *pull, uint8_t npull, const tt_tagstat_t *tag, const char *name);

/* Decoded advertising frame (gateway side). */
typedef struct {
    uint8_t      vt;           /* TT_VT_LEGACY or TT_VT_EXT */
    uint8_t      seq;
    uint8_t      flags;        /* legacy only (ext: recomputed by the receiver) */
    uint8_t      has_counters, has_live, has_bat, has_id, has_tag;
    tt_tool_t    tool;
    uint8_t      nev;
    tt_event_t   ev[TT_MAX_EVENTS];
    uint8_t      npull;
    tt_pull_t    pull[TT_MAX_PULLS];
    tt_tagstat_t tag;
    char         name[20];
} tt_frame_t;

/* Walks the AD structures in data; returns 0 and fills *f if one of ours is found, -1 otherwise. */
int tt_decode_adv(const uint8_t *data, size_t len, tt_frame_t *f);

#endif
