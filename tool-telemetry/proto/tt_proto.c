/* Tool telemetry wire formats, version 1. See tt_proto.h and ../PROTOCOL.md. */
#include "tt_proto.h"
#include <string.h>

static const char HEX[] = "0123456789ABCDEF";

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/* Parses a decimal (optionally negative) in s[0..len). Returns 0 on success. */
static int parse_num(const char *s, size_t len, int32_t *out)
{
    int neg = 0; int64_t v = 0; size_t i = 0;
    if (len == 0) return -1;
    if (s[0] == '-') { neg = 1; i = 1; if (len == 1) return -1; }
    for (; i < len; i++) {
        if (s[i] < '0' || s[i] > '9') return -1;
        v = v * 10 + (s[i] - '0');
        if (v > 0xFFFFFFFFLL) return -1;
    }
    *out = (int32_t)(neg ? -v : v);
    return 0;
}

static int key_is(const char *k, size_t kl, const char *name)
{
    size_t n = strlen(name);
    return kl == n && memcmp(k, name, n) == 0;
}

static uint16_t clamp16(int32_t v) { return v < 0 ? 0 : (v > 65535 ? 65535 : (uint16_t)v); }
static int8_t clamp8s(int32_t v) { return v < -128 ? -128 : (v > 127 ? 127 : (int8_t)v); }

int tt_parse_line(const char *s, size_t len, tt_line_t *line, tt_tool_t *tool)
{
    size_t star, i;
    uint8_t x = 0;
    int h1, h2;
    tt_tool_t t;

    while (len && (s[len - 1] == '\n' || s[len - 1] == '\r')) len--;
    if (len < 7 || len > TT_LINE_MAX || s[0] != '$') return -1;
    star = len - 3;
    if (s[star] != '*') return -1;
    h1 = hexval(s[star + 1]); h2 = hexval(s[star + 2]);
    if (h1 < 0 || h2 < 0) return -1;
    for (i = 1; i < star; i++) x ^= (uint8_t)s[i];
    if (x != (uint8_t)(h1 * 16 + h2)) return -2;

    memset(line, 0, sizeof(*line));
    line->n = -1;
    line->pull.cls = 255;
    line->type[0] = s[1]; line->type[1] = s[2]; line->type[2] = 0;
    if (!(star == 3 || s[3] == ',')) return -1;

    t = *tool;
    i = 4;
    while (i < star) {
        size_t ks = i, ke, vs, ve;
        int32_t num = 0; int isnum;
        while (i < star && s[i] != '=' && s[i] != ',') i++;
        ke = i;
        if (i >= star || s[i] != '=') return -1;
        vs = ++i;
        while (i < star && s[i] != ',') i++;
        ve = i;
        if (i < star) i++;   /* skip the comma */
        isnum = parse_num(s + vs, ve - vs, &num) == 0;
        const char *k = s + ks; size_t kl = ke - ks;
        if (key_is(k, kl, "id")) {
            size_t n = ve - vs, j;
            if (n > 8) n = 8;
            for (j = 0; j < n; j++) t.id[j] = s[vs + j];
            t.id[n] = 0;
        } else if (key_is(k, kl, "ev")) {
            const char *v = s + vs; size_t vl = ve - vs;
            line->ev = key_is(v, vl, "trig") ? TT_EV_TRIG : key_is(v, vl, "oc") ? TT_EV_OC :
                       key_is(v, vl, "flt") ? TT_EV_FLT : key_is(v, vl, "boot") ? TT_EV_TOOLBOOT : 0;
        } else if (key_is(k, kl, "cmd")) {
            size_t n = ve - vs; if (n > 7) n = 7;
            memcpy(line->cmd, s + vs, n); line->cmd[n] = 0;
        } else if (!isnum) {
            continue;                       /* unknown or malformed value: ignore the key */
        } else if (key_is(k, kl, "n"))    line->n = num & 0xFFFF;
        else if (key_is(k, kl, "v"))      line->v = clamp16(num);
        else if (key_is(k, kl, "duty"))   line->duty = (uint8_t)(num < 0 ? 0 : num > 100 ? 100 : num);
        else if (line->type[1] == 'P') {    /* pull summary: its own fields, the tool totals are untouched */
            tt_pull_t *pl = &line->pull;
            if (key_is(k, kl, "dur"))       pl->dur10 = clamp16((num + 5) / 10);
            else if (key_is(k, kl, "ravg")) pl->ravg = clamp16(num);
            else if (key_is(k, kl, "rmax")) pl->rmax = clamp16(num);
            else if (key_is(k, kl, "iavg")) pl->iavg = clamp16(num);
            else if (key_is(k, kl, "ipk"))  pl->ipk = clamp16(num);
            else if (key_is(k, kl, "vmin")) pl->vmin = clamp16(num);
            else if (key_is(k, kl, "tc"))   pl->tc = clamp8s(num);
            else if (key_is(k, kl, "cls"))  pl->cls = (uint8_t)(num < 0 || num > 3 ? 255 : num);
        }
        else if (line->type[1] == 'B') {    /* battery keys share names with tool keys */
            if (key_is(k, kl, "soc"))      { t.soc = (uint8_t)(num < 0 ? 0 : num > 100 ? 100 : num); t.has_bat = 1; }
            else if (key_is(k, kl, "cyc")) { t.cyc = clamp16(num); t.has_bat = 1; }
            else if (key_is(k, kl, "vb"))  { t.bvb = clamp16(num); t.has_bat = 1; }
            else if (key_is(k, kl, "tc"))  { t.btc = clamp8s(num); t.has_bat = 1; }
        }
        else if (key_is(k, kl, "trig"))   t.trig = (uint32_t)num;
        else if (key_is(k, kl, "oc"))     t.oc = clamp16(num);
        else if (key_is(k, kl, "flt"))    t.flt = clamp16(num);
        else if (key_is(k, kl, "run"))    t.run = (uint32_t)num;
        else if (key_is(k, kl, "rpm"))    { t.rpm = clamp16(num); if (t.rpm > t.rpmx) t.rpmx = t.rpm; }
        else if (key_is(k, kl, "ipk"))    t.ipk = clamp16(num);
        else if (key_is(k, kl, "vb"))     t.vb = clamp16(num);
        else if (key_is(k, kl, "tc"))     t.tc = clamp8s(num);
        else if (key_is(k, kl, "st"))     t.st = (uint8_t)(num & 3);
    }
    *tool = t;
    return 0;
}

size_t tt_make_line(char *out, size_t cap, const char *type, int32_t n, const char *body)
{
    size_t p = 0, i, bl = strlen(body);
    uint8_t x = 0;
    char num[8]; int nl = 0; uint32_t u = (uint32_t)(n & 0xFFFF);
    do { num[nl++] = (char)('0' + u % 10); u /= 10; } while (u);
    if (cap < 4 + 3 + (size_t)nl + (bl ? bl + 1 : 0) + 4) return 0;
    out[p++] = '$'; out[p++] = type[0]; out[p++] = type[1];
    out[p++] = ','; out[p++] = 'n'; out[p++] = '=';
    while (nl) out[p++] = num[--nl];
    if (bl) { out[p++] = ','; memcpy(out + p, body, bl); p += bl; }
    for (i = 1; i < p; i++) x ^= (uint8_t)out[i];
    out[p++] = '*'; out[p++] = HEX[x >> 4]; out[p++] = HEX[x & 15]; out[p++] = '\n';
    if (p < cap) out[p] = 0;
    return p;
}

uint8_t tt_flags(const tt_tool_t *t, int link_alive, int ext_on)
{
    uint8_t f = 0;
    if (t->st == TT_ST_RUN) f |= TT_F_RUN;
    if (t->st == TT_ST_FAULT) f |= TT_F_FAULT;
    if (t->has_bat) { f |= TT_F_BAT; if (t->soc < 20) f |= TT_F_BATLOW; }
    if (link_alive) f |= TT_F_LINK;
    if (ext_on) f |= TT_F_EXT;
    return f;
}

static uint8_t *put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); return p + 2; }
static uint8_t *put32(uint8_t *p, uint32_t v) { p = put16(p, (uint16_t)v); return put16(p, (uint16_t)(v >> 16)); }
static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t get32(const uint8_t *p) { return get16(p) | ((uint32_t)get16(p + 2) << 16); }

static uint8_t *msd_head(uint8_t *p, uint8_t vt, uint8_t seq)
{
    p = put16(p, TT_COMPANY_ID);
    *p++ = TT_MAGIC0; *p++ = TT_MAGIC1; *p++ = vt; *p++ = seq;
    return p;
}

size_t tt_build_legacy(uint8_t *out, uint8_t seq, const tt_tool_t *t, uint8_t flags)
{
    uint8_t *p = out;
    uint32_t trig = t->trig > 0xFFFFFFu ? 0xFFFFFFu : t->trig;
    *p++ = 2; *p++ = 0x01; *p++ = 0x06;
    *p++ = 21; *p++ = 0xFF;
    p = msd_head(p, TT_VT_LEGACY, seq);
    *p++ = (uint8_t)trig; *p++ = (uint8_t)(trig >> 8); *p++ = (uint8_t)(trig >> 16);
    p = put16(p, t->oc);
    p = put16(p, t->flt);
    p = put32(p, t->run);
    *p++ = flags;
    *p++ = t->has_bat ? t->soc : 0xFF;
    *p++ = (uint8_t)t->tc;
    return (size_t)(p - out);
}

size_t tt_build_ext(uint8_t *out, size_t cap, uint8_t seq, const tt_tool_t *t, const tt_event_t *ev, uint8_t nev,
                    const tt_pull_t *pull, uint8_t npull, const tt_tagstat_t *tag, const char *name)
{
    static uint8_t buf[TT_EXT_MAX];      /* static: firmware callers run on small task stacks */
    uint8_t *p = buf, *msd_len, i;
    size_t nl = name ? strlen(name) : 0, idl = strlen(t->id), total;
    if (nev > TT_MAX_EVENTS) nev = TT_MAX_EVENTS;
    if (npull > TT_MAX_PULLS) npull = TT_MAX_PULLS;
    if (nl > 16) nl = 16;

    *p++ = 2; *p++ = 0x01; *p++ = 0x06;
    if (nl) { *p++ = (uint8_t)(nl + 1); *p++ = 0x09; memcpy(p, name, nl); p += nl; }
    msd_len = p++; *p++ = 0xFF;
    p = msd_head(p, TT_VT_EXT, seq);
    *p++ = 0x01; *p++ = 12;
    p = put32(p, t->trig); p = put16(p, t->oc); p = put16(p, t->flt); p = put32(p, t->run);
    *p++ = 0x02; *p++ = 10;
    p = put16(p, t->rpm); p = put16(p, t->rpmx); p = put16(p, t->ipk); p = put16(p, t->vb);
    *p++ = (uint8_t)t->tc; *p++ = t->st;
    if (t->has_bat) {
        *p++ = 0x03; *p++ = 6;
        *p++ = t->soc; p = put16(p, t->cyc); p = put16(p, t->bvb); *p++ = (uint8_t)t->btc;
    }
    if (idl) { *p++ = 0x04; *p++ = (uint8_t)idl; memcpy(p, t->id, idl); p += idl; }
    if (nev) {
        *p++ = 0x05; *p++ = (uint8_t)(9 * nev);
        for (i = 0; i < nev; i++) {
            p = put16(p, ev[i].id); *p++ = ev[i].type; p = put32(p, ev[i].t); p = put16(p, ev[i].v);
        }
    }
    if (npull) {
        *p++ = 0x07; *p++ = (uint8_t)(16 * npull);
        for (i = 0; i < npull; i++) {
            const tt_pull_t *q = &pull[i];
            p = put16(p, q->id); p = put16(p, q->dur10); p = put16(p, q->ravg); p = put16(p, q->rmax);
            p = put16(p, q->iavg); p = put16(p, q->ipk); p = put16(p, q->vmin); *p++ = (uint8_t)q->tc; *p++ = q->cls;
        }
    }
    if (tag) {
        *p++ = 0x06; *p++ = 12;
        p = put32(p, tag->up); p = put16(p, tag->link); p = put16(p, tag->rx_ok); *p++ = tag->rx_bad; *p++ = tag->fw;
        p = put16(p, tag->mv);
    }
    *msd_len = (uint8_t)(p - msd_len - 1);
    total = (size_t)(p - buf);
    if (total > cap) return 0;
    memcpy(out, buf, total);
    return total;
}

static int decode_msd(const uint8_t *m, size_t ml, tt_frame_t *f)
{
    const uint8_t *p, *end = m + ml;
    if (ml < 6 || get16(m) != TT_COMPANY_ID || m[2] != TT_MAGIC0 || m[3] != TT_MAGIC1) return -1;
    f->vt = m[4]; f->seq = m[5];
    p = m + 6;
    if (f->vt == TT_VT_LEGACY) {
        if (end - p < 14) return -1;
        f->tool.trig = p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16); p += 3;
        f->tool.oc = get16(p); p += 2;
        f->tool.flt = get16(p); p += 2;
        f->tool.run = get32(p); p += 4;
        f->flags = *p++;
        f->tool.soc = *p++;
        f->tool.has_bat = (f->flags & TT_F_BAT) ? 1 : 0;
        f->tool.tc = (int8_t)*p++;
        f->tool.st = (f->flags & TT_F_FAULT) ? TT_ST_FAULT : (f->flags & TT_F_RUN) ? TT_ST_RUN : TT_ST_IDLE;
        f->has_counters = 1;
        return 0;
    }
    if (f->vt != TT_VT_EXT) return -1;
    while (end - p >= 2) {
        uint8_t ty = p[0], l = p[1];
        const uint8_t *v = p + 2;
        if (v + l > end) return -1;
        if (ty == 0x01 && l >= 12) {
            f->tool.trig = get32(v); f->tool.oc = get16(v + 4); f->tool.flt = get16(v + 6); f->tool.run = get32(v + 8);
            f->has_counters = 1;
        } else if (ty == 0x02 && l >= 10) {
            f->tool.rpm = get16(v); f->tool.rpmx = get16(v + 2); f->tool.ipk = get16(v + 4); f->tool.vb = get16(v + 6);
            f->tool.tc = (int8_t)v[8]; f->tool.st = v[9]; f->has_live = 1;
        } else if (ty == 0x03 && l >= 6) {
            f->tool.soc = v[0]; f->tool.cyc = get16(v + 1); f->tool.bvb = get16(v + 3); f->tool.btc = (int8_t)v[5];
            f->tool.has_bat = 1; f->has_bat = 1;
        } else if (ty == 0x04 && l >= 1) {
            uint8_t n = l > 8 ? 8 : l;
            memcpy(f->tool.id, v, n); f->tool.id[n] = 0; f->has_id = 1;
        } else if (ty == 0x05) {
            uint8_t n = l / 9, i;
            if (n > TT_MAX_EVENTS) n = TT_MAX_EVENTS;
            for (i = 0; i < n; i++) {
                const uint8_t *e = v + 9 * i;
                f->ev[i].id = get16(e); f->ev[i].type = e[2]; f->ev[i].t = get32(e + 3); f->ev[i].v = get16(e + 7);
            }
            f->nev = n;
        } else if (ty == 0x07) {
            uint8_t n = l / 16, i;
            if (n > TT_MAX_PULLS) n = TT_MAX_PULLS;
            for (i = 0; i < n; i++) {
                const uint8_t *e = v + 16 * i;
                tt_pull_t *q = &f->pull[i];
                q->id = get16(e); q->dur10 = get16(e + 2); q->ravg = get16(e + 4); q->rmax = get16(e + 6);
                q->iavg = get16(e + 8); q->ipk = get16(e + 10); q->vmin = get16(e + 12); q->tc = (int8_t)e[14]; q->cls = e[15];
            }
            f->npull = n;
        } else if (ty == 0x06 && l >= 10) {
            f->tag.up = get32(v); f->tag.link = get16(v + 4); f->tag.rx_ok = get16(v + 6); f->tag.rx_bad = v[8]; f->tag.fw = v[9];
            f->tag.mv = l >= 12 ? get16(v + 10) : 0;      /* older tags send 10 bytes */
            f->has_tag = 1;
        }
        p = v + l;
    }
    return 0;
}

int tt_decode_adv(const uint8_t *d, size_t len, tt_frame_t *f)
{
    size_t i = 0;
    int found = -1;
    memset(f, 0, sizeof(*f));
    f->tool.soc = 0xFF;
    while (i < len) {
        uint8_t l = d[i];
        if (l == 0) break;
        if (i + 1 + l > len) break;
        if (d[i + 1] == 0x09 || d[i + 1] == 0x08) {
            size_t n = l - 1; if (n > sizeof(f->name) - 1) n = sizeof(f->name) - 1;
            memcpy(f->name, d + i + 2, n); f->name[n] = 0;
        } else if (d[i + 1] == 0xFF && found != 0) {
            found = decode_msd(d + i + 2, l - 1, f);
        }
        i += 1 + l;
    }
    return found;
}
